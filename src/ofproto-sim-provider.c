/*
 * (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
 * Copyright (c) 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <errno.h>
#include <unistd.h>
#include <net/if.h>
#include <string.h>

#include "config.h"
#include "ofproto/ofproto-provider.h"
#include "ofproto/bond.h"
#include "ofproto/tunnel.h"
#include "bundle.h"
#include "coverage.h"
#include "netdev.h"
#include "timer.h"
#include "seq.h"
#include "unaligned.h"
#include "vlan-bitmap.h"
#include "openvswitch/vlog.h"
#include "ofproto-sim-provider.h"
#include "vswitch-idl.h"

VLOG_DEFINE_THIS_MODULE(ofproto_provider_sim);

#define MAX_CMD_LEN             2048
#define MIRROR_OUTPUT_PORT_CMD_MIN_LEN 56
#define SWNS_EXEC               "/sbin/ip netns exec swns"

static struct sim_provider_ofport *
sim_provider_ofport_cast(const struct ofport *ofport)
{
    return ofport ?
        CONTAINER_OF(ofport, struct sim_provider_ofport, up) : NULL;
}

static inline struct sim_provider_node *
sim_provider_node_cast(const struct ofproto *ofproto)
{
    ovs_assert(ofproto->ofproto_class == &ofproto_sim_provider_class);

    return CONTAINER_OF(ofproto, struct sim_provider_node, up);
}

/* All existing ofproto provider instances, indexed by ->up.name. */
static struct hmap all_sim_provider_nodes =
HMAP_INITIALIZER(&all_sim_provider_nodes);

/* Factory functions. */

static void
init(const struct shash *iface_hints)
{
    return;
}

static void
enumerate_types(struct sset *types)
{
    struct sim_provider_node *ofproto;

    sset_add(types, "system");
    sset_add(types, "vrf");
}

static int
enumerate_names(const char *type, struct sset *names)
{
    struct sim_provider_node *ofproto;
    const char *port_type;

    sset_clear(names);
    HMAP_FOR_EACH(ofproto, all_sim_provider_node, &all_sim_provider_nodes) {
        if (strcmp(type, ofproto->up.type)) {
            continue;
        }

        sset_add(names, ofproto->up.name);
    }

    return 0;
}

static int
del(const char *type OVS_UNUSED, const char *name OVS_UNUSED)
{
    return 0;
}

static const char *
port_open_type(const char *datapath_type OVS_UNUSED, const char *port_type)
{
    if (port_type && (strcmp(port_type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0)) {
        return port_type;
    }
    if (port_type && (strcmp(port_type, OVSREC_INTERFACE_TYPE_VLANSUBINT) == 0)) {
            return port_type;
    }
    if (port_type && (strcmp(port_type, OVSREC_INTERFACE_TYPE_LOOPBACK) == 0)) {
            return port_type;
    }
    return "system";
}

struct mbridge *
mbridge_create(void)
{
    struct mbridge *mbridge;

    mbridge = xzalloc(sizeof *mbridge);
    ovs_refcount_init(&mbridge->ref_cnt);

    hmap_init(&mbridge->mbundles);
    return mbridge;
}

void
mbridge_register_bundle(struct mbridge *mbridge, struct ofbundle *ofbundle)
{
    struct mbundle *mbundle;

    mbundle = xzalloc(sizeof *mbundle);
    mbundle->ofbundle = ofbundle;
    hmap_insert(&mbridge->mbundles, &mbundle->hmap_node,
                hash_pointer(ofbundle, 0));
}

void
mbridge_unregister_bundle(struct mbridge *mbridge, struct ofbundle *ofbundle)
{
    struct mbundle *mbundle = mbundle_lookup(mbridge, ofbundle);
    size_t i;

    if (!mbundle) {
        return;
    }

    for (i = 0; i < MAX_MIRRORS; i++) {
        struct mirror *m = mbridge->mirrors[i];
        if (m) {
            if (m->out == mbundle) {
                mirror_destroy(mbridge, m->aux);
            } else if (hmapx_find_and_delete(&m->srcs, mbundle)
                       || hmapx_find_and_delete(&m->dsts, mbundle)) {
                mbridge->need_revalidate = true;
            }
        }
    }

    hmap_remove(&mbridge->mbundles, &mbundle->hmap_node);
    free(mbundle);
}

/* Basic life-cycle. */

static struct ofproto *
alloc(void)
{
    struct sim_provider_node *ofproto = xzalloc(sizeof *ofproto);

    return &ofproto->up;
}

static void
dealloc(struct ofproto *ofproto_)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);

    free(ofproto);
}

static int
construct(struct ofproto *ofproto_)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    struct shash_node *node, *next;
    int error = 0;
    char cmd_str[MAX_CMD_LEN];

    /* If the ofproto is of type BRIDGE, then create a bridge with the same
     * name in ASIC OVS. In ASIC OVS creating a bridge also creates a bundle &
     * port with the same name. The port will be 'internal' type. */
    if (strcmp(ofproto_->type, "system") == 0) {

        snprintf(cmd_str, MAX_CMD_LEN, "%s --may-exist add-br %s -- set bridge %s datapath_type=netdev",
                 OVS_VSCTL, ofproto->up.name, ofproto->up.name);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to add bridge in ASIC OVS. cmd=%s, rc=%s",
                     cmd_str, strerror(errno));
            error = 1;
        }

        snprintf(cmd_str, MAX_CMD_LEN, "%s set port %s trunks=0",
                 OVS_VSCTL, ofproto->up.name);
        if (system(cmd_str) != 0) {
            VLOG_ERR
                ("Failed to set trunks in the bridge bundle. cmd=%s, rc=%s",
                 cmd_str, strerror(errno));
            error = 1;
        }

        snprintf(cmd_str, MAX_CMD_LEN, "%s /sbin/ip link set dev %s up",
                 SWNS_EXEC, ofproto->up.name);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to set link state. cmd=%s, rc=%s", cmd_str,
                     strerror(errno));
            error = 1;
        }
        ofproto->vrf = false;

    } else {
        ofproto->vrf = true;
    }

    ofproto->netflow = NULL;
    ofproto->stp = NULL;
    ofproto->rstp = NULL;
    ofproto->dump_seq = 0;
    hmap_init(&ofproto->bundles);
    ofproto->ms = NULL;
    ofproto->mbridge = mbridge_create();
    ofproto->has_bonded_bundles = false;
    ofproto->lacp_enabled = false;
    ofproto_tunnel_init();
    ovs_mutex_init_adaptive(&ofproto->stats_mutex);
    ovs_mutex_init(&ofproto->vsp_mutex);

    guarded_list_init(&ofproto->pins);

    sset_init(&ofproto->ports);
    sset_init(&ofproto->ghost_ports);
    sset_init(&ofproto->port_poll_set);
    ofproto->port_poll_errno = 0;
    ofproto->change_seq = 0;
    ofproto->pins_seq = seq_create();
    ofproto->pins_seqno = seq_read(ofproto->pins_seq);

    hmap_insert(&all_sim_provider_nodes, &ofproto->all_sim_provider_node,
                hash_string(ofproto->up.name, 0));

    memset(&ofproto->stats, 0, sizeof ofproto->stats);
    ofproto->vlans_bmp = bitmap_allocate(VLAN_BITMAP_SIZE);
    ofproto->vlan_intf_bmp = bitmap_allocate(VLAN_BITMAP_SIZE);
    ofproto_init_tables(ofproto_, N_TABLES);
    ofproto->up.tables[TBL_INTERNAL].flags = OFTABLE_HIDDEN | OFTABLE_READONLY;

    return error;
}

static void
destruct(struct ofproto *ofproto_ OVS_UNUSED)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    char ovs_delbr[80];

    if (ofproto->vrf == false) {

        sprintf(ovs_delbr, "%s del-br %s", OVS_VSCTL, ofproto->up.name);
        if (system(ovs_delbr) != 0) {
            VLOG_ERR("Failed to delete the bridge. cmd=%s, rc=%s",
                     ovs_delbr, strerror(errno));
        }
    }

    hmap_remove(&all_sim_provider_nodes, &ofproto->all_sim_provider_node);

    hmap_destroy(&ofproto->bundles);

    sset_destroy(&ofproto->ports);
    sset_destroy(&ofproto->ghost_ports);
    sset_destroy(&ofproto->port_poll_set);

    ovs_mutex_destroy(&ofproto->stats_mutex);
    ovs_mutex_destroy(&ofproto->vsp_mutex);

    return;
}

static int
run(struct ofproto *ofproto_ OVS_UNUSED)
{
    return 0;
}

static void
wait(struct ofproto *ofproto_ OVS_UNUSED)
{
    return;
}

static void
query_tables(struct ofproto *ofproto,
             struct ofputil_table_features *features,
             struct ofputil_table_stats *stats)
{
    return;
}

static void
set_tables_version(struct ofproto *ofproto, cls_version_t version)
{
    return;
}

static struct ofport *
port_alloc(void)
{
    struct sim_provider_ofport *port = xzalloc(sizeof *port);

    return &port->up;
}

static void
port_dealloc(struct ofport *port_)
{
    struct sim_provider_ofport *port = sim_provider_ofport_cast(port_);

    free(port);
}

static int
port_construct(struct ofport *port_)
{
    struct sim_provider_ofport *port = sim_provider_ofport_cast(port_);

    return 0;
}

static void
port_destruct(struct ofport *port_ OVS_UNUSED)
{
    return;
}

static void
port_reconfigured(struct ofport *port_, enum ofputil_port_config old_config)
{
    return;
}

static bool
cfm_status_changed(struct ofport *ofport_)
{
    return false;
}

static bool
bfd_status_changed(struct ofport *ofport_ OVS_UNUSED)
{
    return false;
}

static struct ofbundle *
bundle_lookup(const struct sim_provider_node *ofproto, void *aux)
{
    struct ofbundle *bundle;

    HMAP_FOR_EACH_IN_BUCKET(bundle, hmap_node, hash_pointer(aux, 0),
                            &ofproto->bundles) {
        if (bundle->aux == aux) {
            return bundle;
        }
    }
    return NULL;
}

static void
enable_port_in_iptables(const char *port_name)
{
    char cmd[MAX_CMD_LEN];

    snprintf(cmd, MAX_CMD_LEN, "%s iptables -D INPUT -i %s -j DROP",
             SWNS_EXEC, port_name);
    if (system(cmd) != 0) {
        VLOG_ERR("Failed to delete DROP rule. cmd=%s rc=%s", cmd,
                 strerror(errno));
    }

    snprintf(cmd, MAX_CMD_LEN, "%s iptables -D FORWARD -i %s -j DROP",
             SWNS_EXEC, port_name);
    if (system(cmd) != 0) {
        VLOG_ERR("Failed to delete DROP rule. cmd=%s rc=%s", cmd,
                 strerror(errno));
    }
}

static void
disable_port_in_iptables(const char *port_name)
{
    int rc = 0;
    char cmd[MAX_CMD_LEN];

    /* Do not add drop rules if the "Check" command returns success. */
    snprintf(cmd, MAX_CMD_LEN, "%s iptables -C INPUT -i %s -j DROP",
             SWNS_EXEC, port_name);
    rc = system(cmd);
    if (rc != 0) {

        snprintf(cmd, MAX_CMD_LEN, "%s iptables -A INPUT -i %s -j DROP",
                 SWNS_EXEC, port_name);
        if (system(cmd) != 0) {
            VLOG_ERR("Failed to add DROP rules: cmd=%s rc=%s", cmd,
                     strerror(errno));
        }

        snprintf(cmd, MAX_CMD_LEN, "%s iptables -A FORWARD -i %s -j DROP",
                 SWNS_EXEC, port_name);
        if (system(cmd) != 0) {
            VLOG_ERR("Failed to add DROP rules: cmd=%s rc=%s", cmd,
                     strerror(errno));
        }
    }
}

static void
bundle_del_port(struct sim_provider_ofport *port)
{
    struct ofbundle *bundle = port->bundle;

    list_remove(&port->bundle_node);
    port->bundle = NULL;

    /* Enable the port in IP tables. So that regular L3 traffic can flow
     * across it. */
    if (port->iptable_rules_added == true) {
        enable_port_in_iptables(netdev_get_name(port->up.netdev));
        port->iptable_rules_added = false;
    }
}

static bool
bundle_add_port(struct ofbundle *bundle, ofp_port_t ofp_port)
{
    struct sim_provider_ofport *port;

    port = get_ofp_port(bundle->ofproto, ofp_port);
    if (!port) {
        return false;
    }

    if (port->bundle != bundle) {
        if (port->bundle) {
            bundle_remove(&port->up);
        }

        port->bundle = bundle;
        list_push_back(&bundle->ports, &port->bundle_node);
    }

    return true;
}

static void
sim_bridge_vlan_routing_update(struct sim_provider_node *ofproto, int vlan,
                               bool add)
{
    int i = 0, n = 0;
    int vlan_count = 0;
    char cmd_str[MAX_CMD_LEN];

    if (add) {

        /* If the vlan is already added to the list. */
        if (bitmap_is_set(ofproto->vlan_intf_bmp, vlan)) {
            return;
        }

        /* Save the VLAN routing interface IDs. */
        bitmap_set1(ofproto->vlan_intf_bmp, vlan);

    } else {

        /* If the vlan is already unset in the list. */
        if (!bitmap_is_set(ofproto->vlan_intf_bmp, vlan)) {
            return;
        }

        /* Unset the VLAN routing interface IDs. */
        bitmap_set0(ofproto->vlan_intf_bmp, vlan);
    }

    n = snprintf(cmd_str, MAX_CMD_LEN, "%s set port %s ", OVS_VSCTL,
                 ofproto->up.name);

    for (i = 1; i < 4095; i++) {

        if (bitmap_is_set(ofproto->vlan_intf_bmp, i)) {

            if (vlan_count == 0) {
                n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), " trunks=%d", i);
            } else {
                n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), ",%d", i);
            }
            ovs_assert(n <= MAX_CMD_LEN);

            vlan_count += 1;
        }
    }

    if (vlan_count == 0) {
        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), " trunks=0");
    }

    if (system(cmd_str) != 0) {
        VLOG_ERR("Failed to modify bridge interface trunks: cmd=%s, rc=%s",
                 cmd_str, strerror(errno));
    }
}

/* Freeing up bundle and its members on heap */
static void
bundle_destroy(struct ofbundle *bundle)
{
    struct sim_provider_node *ofproto = NULL;
    struct sim_provider_ofport *port = NULL, *next_port = NULL;
    const char *type = NULL;
    char cmd_str[MAX_CMD_LEN];

    if (!bundle) {
        return;
    }

    ofproto = bundle->ofproto;
    mbridge_unregister_bundle(ofproto->mbridge, bundle);

    if (bundle->is_added_to_sim_ovs == true) {
        snprintf(cmd_str, MAX_CMD_LEN, "%s del-port %s", OVS_VSCTL,
                 bundle->name);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to delete the existing port. cmd=%s, rc=%s",
                     cmd_str, strerror(errno));
        }
        bundle->is_added_to_sim_ovs = false;
    }

    /* If the ofproto is of type System/Bridge, and VLAN routing is enabled on
     * this bundle then delete that VLAN ID from BRIDGE interface created in
     * ASIC OVS. We should ignore the bundle whose name is same as the BRIDGE
     * name. */
    if (bundle->is_vlan_routing_enabled == true) {
        sim_bridge_vlan_routing_update(ofproto, bundle->vlan, false);
        bundle->is_vlan_routing_enabled = false;
    }

    LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
        bundle_del_port(port);
    }

    hmap_remove(&ofproto->bundles, &bundle->hmap_node);

    if (bundle->name) {
        free(bundle->name);
    }
    if (bundle->trunks) {
        free(bundle->trunks);
    }

    free(bundle);
}

static void
bundle_configure(struct ofbundle *bundle)
{
    struct sim_provider_node *ofproto = bundle->ofproto;
    struct sim_provider_ofport *port = NULL, *next_port = NULL;
    char cmd_str[MAX_CMD_LEN];
    int i = 0, n = 0, n_ports = 0;
    uint32_t vlan_count = 0;

    /* If this bundle is already added in the ASIC simulator OVS then delete
     * it. We are going to re-create it with new config again. */
    if (bundle->is_added_to_sim_ovs == true) {
        snprintf(cmd_str, MAX_CMD_LEN, "%s del-port %s", OVS_VSCTL,
                 bundle->name);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to delete the existing port. cmd=%s, rc=%s",
                     cmd_str, strerror(errno));
        }
        bundle->is_added_to_sim_ovs = false;
    }

    /* If this bundle is attached to VRF, there is no need to do any special
     * handling. Kernel will take care of routing. */
    if (ofproto->vrf) {
        return;
    }

    n_ports = list_size(&bundle->ports);

    /* If there is one or more slaves, then create a regular port. The Linux
     * bonding driver will take care of managing the bond if the number of
     * slaves is greater than 1. */
    if (n_ports >= 1) {
        /* If there is more than one slave, then we need to delete the
         * existent ports that are part of the bond, otherwise the bonding
         * driver does not work properly */
        if (n_ports > 1) {
            LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
                snprintf(cmd_str, MAX_CMD_LEN, "%s del-port %s", OVS_VSCTL,
                         netdev_get_name(port->up.netdev));
                if (system(cmd_str) != 0) {
                    VLOG_DBG("Failed to delete port. cmd=%s, rc=%s",
                             cmd_str, strerror(errno));
                }
            }
        }
        n = snprintf(cmd_str, MAX_CMD_LEN, "%s --may-exist add-port %s %s",
                     OVS_VSCTL, ofproto->up.name, bundle->name);
    } else {
        VLOG_INFO("Not enough ports to create a bundle, so skipping it.");
        return;
    }

    if (bundle->vlan_mode != PORT_VLAN_TRUNK) {
        if ((bundle->vlan > 0)
            && (bitmap_is_set(ofproto->vlans_bmp, bundle->vlan))) {
            n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), " tag=%d",
                          bundle->vlan);
            ovs_assert(n <= MAX_CMD_LEN);
        } else {
            goto done;
        }
    }

    /* The following logic is used for a port/bond in trunk mode:
     * If the configuration didn't list any trunk VLAN, trunk all VLANs that
     * are enabled in the VLAN table (bit set in VLAN bitmap).
     * If the configuration does list trunk VLANs, configure all of the VLANs
     * in that list which are also enabled in the VLAN table.
     *
     * NOTE: If no VLANs are enabled in the VLAN bitmap, the port is not added
     * in the Internal "ASIC" OVS */

    if (bundle->vlan_mode != PORT_VLAN_ACCESS) {
        for (i = 1; i < 4095; i++) {
            if (bitmap_is_set(ofproto->vlans_bmp, i)) {
                if (!(bundle->trunks) || bitmap_is_set(bundle->trunks, i)) {

                    if (vlan_count == 0) {
                        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n),
                                      " trunks=%d", i);
                    } else {
                        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), ",%d",
                                      i);
                    }

                    ovs_assert(n <= MAX_CMD_LEN);
                    vlan_count += 1;
                }
            }
        }
        /* If the trunk vlans are not defined in the global list. */
        if (vlan_count == 0) {
            goto done;
        }
    }

    if (bundle->vlan_mode == PORT_VLAN_ACCESS) {
        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), " vlan_mode=access");
        ovs_assert(n <= MAX_CMD_LEN);

    } else if (bundle->vlan_mode == PORT_VLAN_TRUNK) {
        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n), " vlan_mode=trunk");
        ovs_assert(n <= MAX_CMD_LEN);

    } else if (bundle->vlan_mode == PORT_VLAN_NATIVE_UNTAGGED) {
        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n),
                      " vlan_mode=native-untagged");
        ovs_assert(n <= MAX_CMD_LEN);

    } else if (bundle->vlan_mode == PORT_VLAN_NATIVE_TAGGED) {
        n += snprintf(&cmd_str[n], (MAX_CMD_LEN - n),
                      " vlan_mode=native-tagged");
        ovs_assert(n <= MAX_CMD_LEN);
    }

    VLOG_DBG(cmd_str);

    if (system(cmd_str) != 0) {
        VLOG_ERR("Failed to create bundle. %s, %s", cmd_str, strerror(errno));
    } else {
        bundle->is_added_to_sim_ovs = true;
    }

done:
    /* Install IP table rules to prevent the traffic going to kernel IP stack.
     * When L2 switching is enabled on a port, ASIC OVS takes care of switching
     * the packets. We only depend on kernel to do L3 routing. */
    LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {

        if (port->iptable_rules_added == false) {
            disable_port_in_iptables(netdev_get_name(port->up.netdev));
            port->iptable_rules_added = true;
        }
    }
}

/* Bundles. */
static int
bundle_set(struct ofproto *ofproto_, void *aux,
           const struct ofproto_bundle_settings *s)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    const struct ofport *ofport = NULL;
    bool ok = false;
    int ofp_port, i = 0, n = 0;
    char cmd_str[MAX_CMD_LEN];
    struct ofbundle *bundle;
    unsigned long *trunks = NULL;

    if (s == NULL) {
        bundle_destroy(bundle_lookup(ofproto, aux));
        return 0;
    }
    bundle = bundle_lookup(ofproto, aux);

    if (!bundle) {
        VLOG_DBG("bundle created\n");
        bundle = xmalloc(sizeof (struct ofbundle));

        bundle->ofproto = ofproto;
        hmap_insert(&ofproto->bundles, &bundle->hmap_node,
                    hash_pointer(aux, 0));
        bundle->aux = aux;
        bundle->name = NULL;

        list_init(&bundle->ports);
        bundle->vlan_mode = PORT_VLAN_ACCESS;
        bundle->vlan = -1;
        bundle->trunks = NULL;
        bundle->bond = NULL;
        bundle->is_added_to_sim_ovs = false;
        bundle->is_vlan_routing_enabled = false;
        bundle->is_bridge_bundle = false;
        mbridge_register_bundle(ofproto->mbridge, bundle);
    }

    if (!bundle->name || strcmp(s->name, bundle->name)) {
        free(bundle->name);
        bundle->name = xstrdup(s->name);
        VLOG_DBG("bundle name is %s",bundle->name);
    }

    /* Update set of ports. */
    ok = true;
    VLOG_DBG("s->n_slaves %d", s->n_slaves);
    for (i = 0; i < s->n_slaves; i++) {
        if (!bundle_add_port(bundle, s->slaves[i])) {
            ok = false;
        }
    }

    if (!ok || list_size(&bundle->ports) != s->n_slaves) {
        struct sim_provider_ofport *next_port = NULL, *port = NULL;

        LIST_FOR_EACH_SAFE(port, next_port, bundle_node, &bundle->ports) {
            for (i = 0; i < s->n_slaves; i++) {
                if (s->slaves[i] == port->up.ofp_port) {
                    goto found;
                }
            }
            bundle_del_port(port);
found:     ;
        }
    }
    ovs_assert(list_size(&bundle->ports) <= s->n_slaves);

    if (list_is_empty(&bundle->ports)) {
        VLOG_DBG("bundle %s destroyed\n",bundle->name);
        bundle_destroy(bundle);
        return EINVAL;
    }

    VLOG_DBG("Bridge/VRF name=%s type=%s bundle=%s",
             ofproto->up.name, ofproto->up.type, bundle->name);

    /* If it is bridge's internal bundle return from here. */
    if (bundle->is_bridge_bundle == true) {
        return 0;
    }

    /* Check if the given bundle is a VLAN routing enabled. */
    if (s->n_slaves == 1) {

        struct sim_provider_ofport *port = NULL;
        const char *type = NULL;

        port =
            sim_provider_ofport_cast(ofproto_get_port(ofproto_, s->slaves[0]));
        if (port) {
            type = netdev_get_type(port->up.netdev);
        }

        /* Configure trunk for bridge interface so we receive vlan frames on
         * internal vlan interfaces created on top of bridge. Skip this for
         * internal interface is bridge internal interface with same name as
         * bridge */
        if ((type != NULL)
            && (strcmp(type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0) && (s->vlan)
            && (strcmp(bundle->name, ofproto->up.name) != 0)
            && (ofproto->vrf == false)) {

            sim_bridge_vlan_routing_update(ofproto, s->vlan, true);
            bundle->vlan = s->vlan;

            bundle->is_vlan_routing_enabled = true;

            return 0;
        }

        /* If the bundle name is same as ofproto name, then it is internal
         * bundle for that bridge. We don't need to handle such bundles. */
        if ((type != NULL)
            && (strcmp(type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0)
            && (strcmp(bundle->name, ofproto->up.name) == 0)
            && (ofproto->vrf == false)) {

            bundle->is_bridge_bundle = true;

            return 0;
        }
    }

    /* If this bundle is attached to VRF, and it is a VLAN based internal
     * bundle, then there is no need to do any special handling. Kernel will
     * take care of routing. */
    if (ofproto->vrf == true) {
        VLOG_DBG("bundle is attached to VRF, Kernel will take care of routing\n");
        return 0;
    }

    /* If it is a bond, and there are less than two ports added to it, then
     * postpone creating this bond in ASIC OVS until at least two ports are
     * added to it. */
    if ((s->slaves_entered > 1) && s->n_slaves <= 1) {
        VLOG_INFO
            ("LAG Doesn't have enough interfaces to enable. So delaying the creation.");
        return 0;
    }

    /* Copy the data from 's' into bundle structure. */

    bundle->vlan_mode = s->vlan_mode;

    if ((s->vlan > 0) && (s->vlan < 4095)) {
        bundle->vlan = s->vlan;
    } else {
        bundle->vlan = -1;
    }

    if (s->trunks) {
        trunks = CONST_CAST(unsigned long *, s->trunks);
        if (!vlan_bitmap_equal(trunks, bundle->trunks)) {
            free(bundle->trunks);
            bundle->trunks = vlan_bitmap_clone(trunks);
        }
    } else {
        if (bundle->trunks) {
            free(bundle->trunks);
            bundle->trunks = NULL;
        }
    }

    /* Configure the bundle */
    bundle_configure(bundle);

    return 0;
}

static int
bundle_get(struct ofproto *ofproto_, void *aux, int *bundle_handle)
{
    return 0;
}

static int
bundle_set_reconfigure(struct ofproto *ofproto_, int vid)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    struct ofbundle *bundle;

    HMAP_FOR_EACH(bundle, hmap_node, &ofproto->bundles) {

        /* bundles with VLAN routing enabled doesn't need re-configuration
         * when L2 VLan config is changed. */
        if ((bundle->is_vlan_routing_enabled == true) ||
            (bundle->is_bridge_bundle == true)) {

            continue;
        }

        /* Reconfiguring bundle with original configuration when VLAN is added
         * and deleting the bundle when corresponding VLAN gets deleted */
        bundle_configure(bundle);
    }
    return 0;
}

static void
bundle_remove(struct ofport *port_)
{
    struct sim_provider_ofport *port = sim_provider_ofport_cast(port_);
    struct ofbundle *bundle = port->bundle;

    if (bundle) {
        bundle_del_port(port);

        /* If there are no ports left, delete the bunble. */
        if (list_is_empty(&bundle->ports)) {
            bundle_destroy(bundle);
        } else {
            /* Re-configure the bundle with new config. */
            bundle_configure(bundle);
        }
    }
}

static int
set_vlan(struct ofproto *ofproto_, int vid, bool add)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);

    /* MAKE TO DBG */
    VLOG_DBG("%s: vid=%d, oper=%s", __FUNCTION__, vid, (add ? "add" : "del"));

    if (add) {

        /* If the vlan is already added to the list. */
        if (bitmap_is_set(ofproto->vlans_bmp, vid)) {
            return 0;
        }

        bitmap_set1(ofproto->vlans_bmp, vid);

    } else {

        /* If the vlan is already unset in the list. */
        if (!bitmap_is_set(ofproto->vlans_bmp, vid)) {
            return 0;
        }

        bitmap_set0(ofproto->vlans_bmp, vid);
    }

    if (ofproto->vrf == false) {
        bundle_set_reconfigure(ofproto_, vid);
    }

    return 0;
}

/* Mirrors. */
static int
mirror_set(struct ofproto *ofproto_, void *aux,
                      const struct ofproto_mirror_settings *s)
{
	struct mbundle *mbundle, *out;
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    char cmd_str[MAX_CMD_LEN];
    int i = 0, n = 0, retval = 0;
    struct mirror *mirror;
    mirror_mask_t mirror_bit;
    struct mbridge *mbridge;
    struct ofbundle **srcs = NULL, **dsts = NULL;
    struct hmapx srcs_map; /* Contains "struct ofbundle *"s. */
    struct hmapx dsts_map; /* Contains "struct ofbundle *"s. */
    struct ofbundle *out_bundle;
    bool mirrorModify = false;

    VLOG_DBG("%s:Entry()", __FUNCTION__);

    hmapx_init(&srcs_map);
    hmapx_init(&dsts_map);

    mbridge = ofproto->mbridge;
    mirror = mirror_lookup(mbridge, aux);

    if (NULL != s) {
        /*This is a mirror create/modify. Save a copy of the mirror config in mbridge */
        if (!mirror) {
            int idx;

            VLOG_DBG("%s:Existing mirror not found", __FUNCTION__);

            idx = mirror_scan(mbridge);
            if (idx < 0) {
                VLOG_ERR(
                        "maximum of %d port mirrors reached, cannot create %s",
                        MAX_MIRRORS, s->name);
                return EFBIG;
            }

            VLOG_DBG("%s:Allocating new mirror", __FUNCTION__);
            mirror = mbridge->mirrors[idx] = xzalloc(sizeof *mirror);
            mirror->mbridge = mbridge;
            mirror->idx = idx;
            mirror->aux = aux;
            mirror->out_vlan = -1;
            mirror->name = xzalloc(MAX_MIRROR_NAME_LEN);
            strncpy(mirror->name, s->name, MAX_MIRROR_NAME_LEN);
            mirror->name[MAX_MIRROR_NAME_LEN] = '\0';
        } else {
            VLOG_DBG("%s:Modifying existing mirror", __FUNCTION__);
            mirrorModify = true;
        }

        if (s->out_bundle) {
            out_bundle = bundle_lookup(ofproto, s->out_bundle);
            /* Get the new configuration. */
            if (out_bundle) {
                VLOG_DBG("%s:Mirror output %s", __FUNCTION__,
                        out_bundle->name);
                out = mbundle_lookup(mbridge, out_bundle);
                if (!out) {
                    mirror_destroy(mbridge, mirror->aux);
                    return EINVAL;
                }
                /* s->out_vlan = -1;*/
            } else {
                out = NULL;
            }
        } else {
            out = NULL;
        }

        if (s->n_srcs > 0) {
            srcs = xmalloc(s->n_srcs * sizeof *srcs);
            for (i = 0; i < s->n_srcs; i++) {
                srcs[i] = bundle_lookup(ofproto, s->srcs[i]);
            }
            mbundle_lookup_multiple(mbridge, srcs, s->n_srcs, &srcs_map);
        }
        if (s->n_dsts > 0) {
            dsts = xmalloc(s->n_dsts * sizeof *dsts);
            for (i = 0; i < s->n_dsts; i++) {
                dsts[i] = bundle_lookup(ofproto, s->dsts[i]);
            }
            mbundle_lookup_multiple(mbridge, dsts, s->n_dsts, &dsts_map);
        }

        if (hmapx_equals(&srcs_map, &mirror->srcs)
                && hmapx_equals(&dsts_map, &mirror->dsts)
                && vlan_bitmap_equal(mirror->vlans, s->src_vlans)
                && mirror->out == out && mirror->out_vlan == s->out_vlan) {
            /* If the configuration has not changed, do nothing. */
            hmapx_destroy(&srcs_map);
            hmapx_destroy(&dsts_map);
            if (srcs) {
                free(srcs);
            }
            if (dsts) {
                free(dsts);
            }
            return 0;
        }

        hmapx_swap(&srcs_map, &mirror->srcs);
        hmapx_destroy(&srcs_map);

        hmapx_swap(&dsts_map, &mirror->dsts);
        hmapx_destroy(&dsts_map);

        free(mirror->vlans);
        mirror->vlans = vlan_bitmap_clone(s->src_vlans);

        mirror->out = out;
        mirror->out_vlan = s->out_vlan;

        mbridge->has_mirrors = true;

        /* TODO: vLANs aren't supported yet */

        /************************************************************/
        /* delete the mirror in openvswitch before creating new one */
        /************************************************************/
        if (mirrorModify == true) {
            n = snprintf(cmd_str, MAX_CMD_LEN,
                         "%s -- --id=@m get mirror %s -- remove bridge bridge_normal mirrors @m",
                         OVS_VSCTL, mirror->name);

            VLOG_DBG("%s:Constructed cmd:'%s'", __FUNCTION__, cmd_str);

            if (system(cmd_str) != 0) {
                VLOG_ERR("Failed to delete mirror %s for modify: %s", mirror->name,
                        strerror(errno));
                retval = errno;
            }
        }

        /************************************************************/
        /* Build the command to construct the mirror in openvswitch */
        /************************************************************/
        n = snprintf(cmd_str, MAX_CMD_LEN,
                     "%s -- --id=@m create mirror name=%s -- add bridge bridge_normal mirrors @m ",
                     OVS_VSCTL, s->name);

        /***********************************/
        /* Add the ingress ports           */
        for (i = 0; i < s->n_srcs; i++) {
            n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                    "-- --id=@srx%s get port %s ", srcs[i]->name,
                    srcs[i]->name);
            n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                    "-- set mirror %s select-src-port=@srx%s ", s->name,
                    srcs[i]->name);
        }

        /***********************************/
        /* Add the egress ports            */
        for (i = 0; i < s->n_dsts; i++) {
            n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                    "-- --id=@stx%s get port %s ", dsts[i]->name,
                    dsts[i]->name);
            n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                    "-- set mirror %s select-dst-port=@stx%s ", s->name,
                    dsts[i]->name);
        }

        /* Check if the buffer has enough space for the remaining command
         * buffer to be added */
        if ((MAX_CMD_LEN - n)
                < (MIRROR_OUTPUT_PORT_CMD_MIN_LEN + strlen(out_bundle->name)
                        + strlen(s->name))) {
            VLOG_ERR(
                    "Failed to create mirror '%s'. Command length would exceed buffer size %d",
                    s->name, MAX_CMD_LEN);
            mirror_destroy(mbridge, mirror->aux);

            retval = EMSGSIZE;
        } else {
            /***********************************/
            /* Set the output port             */
            if (out_bundle) {
                /* strings in here are 55 chars without the variable parameters
                 * This is captured in MIRROR_OUTPUT_PORT_CMD_MIN_LEN which accounts for
                 * this size NULL.
                 * If you update the command strings below, update the size of
                 * MIROR_OUTPUT_PORT_CMD_MIN_LEN
                 */
                n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                "-- --id=@out get port %s ", out_bundle->name);
                n += snprintf(&cmd_str[n], MAX_CMD_LEN - n,
                "-- set mirror %s output-port=@out ", s->name);
            }

            VLOG_DBG("%s:Constructed cmd:'%s'", __FUNCTION__, cmd_str);

            if (system(cmd_str) != 0) {
                VLOG_ERR("Failed to create mirror %s. %s", s->name,
                strerror(errno));
                mirror_destroy(mbridge, mirror->aux);
                retval = errno;
            }
            else {
                /* regardless of what errors we had before, if the create succeeds we'll go with it */
                retval = 0;
            }
        }

    } else {
        /* This is a mirror delete */

        if (mirror == NULL) {
            VLOG_ERR("No mirror to delete");
            return 0;
        }
        /************************************************************/
        /* Build the command to delete the mirror in openvswitch */
        /************************************************************/
        n = snprintf(cmd_str, MAX_CMD_LEN,
                     "%s -- --id=@m get mirror %s -- remove bridge bridge_normal mirrors @m",
                     OVS_VSCTL, mirror->name);

        VLOG_DBG("%s:Constructed cmd:'%s'", __FUNCTION__, cmd_str);

        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to delete mirror %s. %s", mirror->name,
                    strerror(errno));
            retval = errno;
        }

        /* Now we can delete our copy of the mirror config */
        mirror_destroy(mbridge, aux);
    }

    /* Clean up from create/modify */
    if (srcs) {
        free(srcs);
    }
    if (dsts) {
        free(dsts);
    }

    return retval;
}

static struct mirror *
mirror_lookup(struct mbridge *mbridge, void *aux)
{
    int i;

    for (i = 0; i < MAX_MIRRORS; i++) {
        struct mirror *mirror = mbridge->mirrors[i];
        if (mirror && mirror->aux == aux) {
            return mirror;
        }
    }

    return NULL;
}

static struct mbundle *
mbundle_lookup(const struct mbridge *mbridge, struct ofbundle *ofbundle)
{
    struct mbundle *mbundle;

    HMAP_FOR_EACH_IN_BUCKET (mbundle, hmap_node, hash_pointer(ofbundle, 0),
                             &mbridge->mbundles) {
        if (mbundle->ofbundle == ofbundle) {
            return mbundle;
        }
    }
    return NULL;
}


void
mirror_destroy(struct mbridge *mbridge, void *aux)
{
    struct mirror *mirror = mirror_lookup(mbridge, aux);
    mirror_mask_t mirror_bit;
    struct mbundle *mbundle;
    int i;

    if (!mirror) {
        VLOG_DBG("%s:Existing mirror not found, nothing to delete", __FUNCTION__);
        return;
    }

    VLOG_DBG("%s:Deleting mirror %s", __FUNCTION__, mirror->name);
    i = mirror->idx;

    hmapx_destroy(&mirror->srcs);
    hmapx_destroy(&mirror->dsts);

    free(mirror->name);
    free(mirror->vlans);
    free(mirror);

    mbridge->mirrors[i] = NULL;

    mbridge->has_mirrors = false;
    for (i = 0; i < MAX_MIRRORS; i++) {
        if (mbridge->mirrors[i]) {
            mbridge->has_mirrors = true;
            break;
        }
    }
}

static int
mirror_scan(struct mbridge *mbridge)
{
    int idx;

    for (idx = 0; idx < MAX_MIRRORS; idx++) {
        if (!mbridge->mirrors[idx]) {
            return idx;
        }
    }
    return -1;
}

static void
mbundle_lookup_multiple(const struct mbridge *mbridge,
                        struct ofbundle **ofbundles, size_t n_ofbundles,
                        struct hmapx *mbundles)
{
    size_t i;

    hmapx_init(mbundles);
    for (i = 0; i < n_ofbundles; i++) {
        struct mbundle *mbundle = mbundle_lookup(mbridge, ofbundles[i]);
        if (mbundle) {
            hmapx_add(mbundles, mbundle);
        }
    }
}

static int
mirror_get_stats__(struct ofproto *ofproto, void *aux,
                   uint64_t * packets, uint64_t * bytes)
{
    return 0;
}

static bool
is_mirror_output_bundle(const struct ofproto *ofproto_ OVS_UNUSED,
                        void *aux OVS_UNUSED)
{
    return false;
}

static void
forward_bpdu_changed(struct ofproto *ofproto_ OVS_UNUSED)
{
    return;
}

/* Ports. */

static struct sim_provider_ofport *
get_ofp_port(const struct sim_provider_node *ofproto, ofp_port_t ofp_port)
{
    struct ofport *ofport = ofproto_get_port(&ofproto->up, ofp_port);

    return ofport ? sim_provider_ofport_cast(ofport) : NULL;
}

static int
port_query_by_name(const struct ofproto *ofproto_, const char *devname,
                   struct ofproto_port *ofproto_port)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    const char *type = netdev_get_type_from_name(devname);

    VLOG_DBG("port_query_by_name - %s", devname);

    /* We must get the name and type from the netdev layer directly. */
    if (type) {
        const struct ofport *ofport;

        ofport = shash_find_data(&ofproto->up.port_by_name, devname);
        ofproto_port->ofp_port = ofport ? ofport->ofp_port : OFPP_NONE;
        ofproto_port->name = xstrdup(devname);
        ofproto_port->type = xstrdup(type);
        VLOG_DBG("get_ofp_port name= %s type= %s flow# %d",
                 ofproto_port->name, ofproto_port->type,
                 ofproto_port->ofp_port);
        return 0;
    }
    return ENODEV;

}

static int
port_add(struct ofproto *ofproto_, struct netdev *netdev)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    const char *devname = netdev_get_name(netdev);

    sset_add(&ofproto->ports, devname);
    return 0;
}

static int
port_del(struct ofproto *ofproto_, ofp_port_t ofp_port)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    struct sim_provider_ofport *ofport = get_ofp_port(ofproto, ofp_port);
    int error = 0;

    return error;
}

static int
port_get_stats(const struct ofport *ofport_, struct netdev_stats *stats)
{
    struct sim_provider_ofport *ofport = sim_provider_ofport_cast(ofport_);
    int error;

    error = netdev_get_stats(ofport->up.netdev, stats);

    if (!error && ofport_->ofp_port == OFPP_LOCAL) {
        struct sim_provider_node *ofproto =
            sim_provider_node_cast(ofport->up.ofproto);

        ovs_mutex_lock(&ofproto->stats_mutex);
        /* ofproto->stats.tx_packets represents packets that we created
         * internally and sent to some port Account for them as if they had
         * come from OFPP_LOCAL and got forwarded. */

        if (stats->rx_packets != UINT64_MAX) {
            stats->rx_packets += ofproto->stats.tx_packets;
        }

        if (stats->rx_bytes != UINT64_MAX) {
            stats->rx_bytes += ofproto->stats.tx_bytes;
        }

        /* ofproto->stats.rx_packets represents packets that were received on
         * some port and we processed internally and dropped (e.g. STP).
         * Account for them as if they had been forwarded to OFPP_LOCAL. */

        if (stats->tx_packets != UINT64_MAX) {
            stats->tx_packets += ofproto->stats.rx_packets;
        }

        if (stats->tx_bytes != UINT64_MAX) {
            stats->tx_bytes += ofproto->stats.rx_bytes;
        }
        ovs_mutex_unlock(&ofproto->stats_mutex);
    }

    return error;
}

static int
port_dump_start(const struct ofproto *ofproto_ OVS_UNUSED, void **statep)
{
    VLOG_DBG("%s", __FUNCTION__);
    *statep = xzalloc(sizeof (struct sim_provider_port_dump_state));
    return 0;
}

static int
port_dump_next(const struct ofproto *ofproto_, void *state_,
               struct ofproto_port *port)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    struct sim_provider_port_dump_state *state = state_;
    const struct sset *sset;
    struct sset_node *node;

    if (state->has_port) {
        ofproto_port_destroy(&state->port);
        state->has_port = false;
    }
    sset = state->ghost ? &ofproto->ghost_ports : &ofproto->ports;
    while ((node = sset_at_position(sset, &state->bucket, &state->offset))) {
        int error;

        VLOG_DBG("port dump loop detecting port %s", node->name);

        error = port_query_by_name(ofproto_, node->name, &state->port);
        if (!error) {
            VLOG_DBG("port dump loop reporting port struct %s",
                     state->port.name);
            *port = state->port;
            state->has_port = true;
            return 0;
        } else if (error != ENODEV) {
            return error;
        }
    }

    if (!state->ghost) {
        state->ghost = true;
        state->bucket = 0;
        state->offset = 0;
        return port_dump_next(ofproto_, state_, port);
    }

    return EOF;
}

static int
port_dump_done(const struct ofproto *ofproto_ OVS_UNUSED, void *state_)
{
    struct sim_provider_port_dump_state *state = state_;

    if (state->has_port) {
        ofproto_port_destroy(&state->port);
    }
    free(state);
    return 0;
}

static struct sim_provider_rule
*
sim_provider_rule_cast(const struct rule *rule)
{
    return NULL;
}

static struct rule *
rule_alloc(void)
{
    struct sim_provider_rule *rule = xmalloc(sizeof *rule);

    return &rule->up;
}

static void
rule_dealloc(struct rule *rule_)
{
    struct sim_provider_rule *rule = sim_provider_rule_cast(rule_);

    free(rule);
}

static enum ofperr
rule_construct(struct rule *rule_ OVS_UNUSED)
    OVS_NO_THREAD_SAFETY_ANALYSIS
{
    return 0;
}

static void rule_insert(struct rule *rule, struct rule *old_rule,
                    bool forward_stats)
OVS_REQUIRES(ofproto_mutex)
{
    return;
}

static void
rule_delete(struct rule *rule_ OVS_UNUSED)
OVS_REQUIRES(ofproto_mutex)
{
    return;
}

static void
rule_destruct(struct rule *rule_ OVS_UNUSED)
{
    return;
}

static void
rule_get_stats(struct rule *rule_ OVS_UNUSED, uint64_t * packets OVS_UNUSED,
               uint64_t * bytes OVS_UNUSED, long long int *used OVS_UNUSED)
{
    return;
}

static enum ofperr
rule_execute(struct rule *rule OVS_UNUSED, const struct flow *flow OVS_UNUSED,
             struct dp_packet *packet OVS_UNUSED)
{
    return 0;
}

static void
rule_modify_actions(struct rule *rule_ OVS_UNUSED,
                    bool reset_counters OVS_UNUSED)
OVS_REQUIRES(ofproto_mutex)
{
    return;
}

static struct sim_provider_group
*
sim_provider_group_cast(const struct ofgroup *group)
{
    return group ? CONTAINER_OF(group, struct sim_provider_group, up) : NULL;
}

static struct ofgroup *
group_alloc(void)
{
    struct sim_provider_group *group = xzalloc(sizeof *group);

    return &group->up;
}

static void
group_dealloc(struct ofgroup *group_)
{
    struct sim_provider_group *group = sim_provider_group_cast(group_);

    free(group);
}

static enum ofperr
group_construct(struct ofgroup *group_ OVS_UNUSED)
{
    return 0;
}

static void
group_destruct(struct ofgroup *group_ OVS_UNUSED)
{
    return;
}

static enum ofperr
group_modify(struct ofgroup *group_ OVS_UNUSED)
{
    return 0;
}

static enum ofperr
group_get_stats(const struct ofgroup *group_ OVS_UNUSED,
                struct ofputil_group_stats *ogs OVS_UNUSED)
{
    return 0;
}

static const char *
get_datapath_version(const struct ofproto *ofproto_ OVS_UNUSED)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);

    return VERSION;
}

static bool
set_frag_handling(struct ofproto *ofproto_ OVS_UNUSED,
                  enum ofp_config_flags frag_handling OVS_UNUSED)
{
    return false;
}

static enum ofperr
packet_out(struct ofproto *ofproto_ OVS_UNUSED,
           struct dp_packet *packet OVS_UNUSED,
           const struct flow *flow OVS_UNUSED,
           const struct ofpact *ofpacts OVS_UNUSED,
           size_t ofpacts_len OVS_UNUSED)
{
    return 0;
}

static void
get_netflow_ids(const struct ofproto *ofproto_ OVS_UNUSED,
                uint8_t * engine_type OVS_UNUSED,
                uint8_t * engine_id OVS_UNUSED)
{
    return;
}

#if 0
static enum ofperr
set_config(struct ofproto *ofproto_,
           ofp_port_t ofp_port, const struct smap *args)
{
    const struct ofport *ofport;
    const char *vlan_arg;

    ofproto_sim_provider_class;
    ofport = ofproto_get_port(ofproto_, ofp_port);

    if (ofport) {
        vlan_arg = smap_get(args, "vlan_default");
        VLOG_DBG("set_config port= %s ofp= %d option_arg= %s",
                 netdev_get_name(ofport->netdev), ofport->ofp_port, vlan_arg);
        return 0;
    } else {
        return ENODEV;
    }
}
#endif

const struct ofproto_class ofproto_sim_provider_class = {
    init,
    enumerate_types,
    enumerate_names,
    del,
    port_open_type,
    NULL,                       /* may implement type_run */
    NULL,                       /* may implement type_wait */
    alloc,
    construct,
    destruct,
    dealloc,
    run,
    wait,
    NULL,                       /* get_memory_usage */
    NULL,                       /* may implement type_get_memory_usage */
    NULL,                       /* may implement flush */
    query_tables,
    set_tables_version,
    port_alloc,
    port_construct,
    port_destruct,
    port_dealloc,
    NULL,                       /* may implement port_modified */
    port_reconfigured,
    port_query_by_name,
    port_add,
    port_del,
    port_get_stats,
    port_dump_start,
    port_dump_next,
    port_dump_done,
    NULL,                       /* may implement port_poll */
    NULL,                       /* may implement port_poll_wait */
    NULL,                       /* may implement port_is_lacp_current */
    NULL,                       /* may implement port_get_lacp_stats */
    NULL,                       /* rule_choose_table */
    rule_alloc,
    rule_construct,
    rule_insert,
    rule_delete,
    rule_destruct,
    rule_dealloc,
    rule_get_stats,
    rule_execute,
    set_frag_handling,
    packet_out,
    NULL,                       /* may implement set_netflow */
    get_netflow_ids,
    NULL,                       /* may implement set_sflow */
    NULL,                       /* may implement set_ipfix */
    NULL,                       /* may implement set_cfm */
    cfm_status_changed,
    NULL,                       /* may implement get_cfm_status */
    NULL,                       /* may implement set_lldp */
    NULL,                       /* may implement get_lldp_status */
    NULL,                       /* may implement set_aa */
    NULL,                       /* may implement aa_mapping_set */
    NULL,                       /* may implement aa_mapping_unset */
    NULL,                       /* may implement aa_vlan_get_queued */
    NULL,                       /* may implement aa_vlan_get_queue_size */
    NULL,                       /* may implement set_bfd */
    bfd_status_changed,
    NULL,                       /* may implement get_bfd_status */
    NULL,                       /* may implement set_stp */
    NULL,                       /* may implement get_stp_status */
    NULL,                       /* may implement set_stp_port */
    NULL,                       /* may implement get_stp_port_status */
    NULL,                       /* may implement get_stp_port_stats */
    NULL,                       /* may implement set_rstp */
    NULL,                       /* may implement get_rstp_status */
    NULL,                       /* may implement set_rstp_port */
    NULL,                       /* may implement get_rstp_port_status */
    NULL,                       /* may implement set_queues */
    bundle_set,
    bundle_remove,
    bundle_get,
    set_vlan,
    mirror_set,
    mirror_get_stats__,
    NULL,                       /* may implement set_flood_vlans */
    is_mirror_output_bundle,
    forward_bpdu_changed,
    NULL,                       /* may implement set_mac_table_config */
    NULL,                       /* may implement set_mcast_snooping */
    NULL,                       /* may implement set_mcast_snooping_port */
    NULL,                       /* set_realdev, is unused */
    NULL,                       /* meter_get_features */
    NULL,                       /* meter_set */
    NULL,                       /* meter_get */
    NULL,                       /* meter_del */
    group_alloc,                /* group_alloc */
    group_construct,            /* group_construct */
    group_destruct,             /* group_destruct */
    group_dealloc,              /* group_dealloc */
    group_modify,               /* group_modify */
    group_get_stats,            /* group_get_stats */
    get_datapath_version,       /* get_datapath_version */
    NULL,                       /* Add l3 host entry */
    NULL,                       /* Delete l3 host entry */
    NULL,                       /* Get l3 host entry hit bits */
    NULL,                       /* l3 route action - install, update, delete */
    NULL,                       /* enable/disable ECMP globally */
    NULL,                       /* enable/disable ECMP hash configs */
};
