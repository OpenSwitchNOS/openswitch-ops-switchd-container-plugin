/*
 * Copyright (c) 2009, 2010, 2011, 2012, 2013, 2014 Nicira, Inc.

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
 * Hewlett-Packard Company Confidential (C) Copyright 2015 Hewlett-Packard Development Company, L.P.
 */

#include <config.h>
#include "ofproto/ofproto-provider.h"
#include <errno.h>
#include "bfd.h"
#include "ofproto/bond.h"
#include "bundle.h"
#include "byte-order.h"
#include "coverage.h"
#include "dynamic-string.h"
#include "ofproto/fail-open.h"
#include "ofproto/tunnel.h"
#include "guarded-list.h"
#include "hmapx.h"
#include "lacp.h"
#include "netdev.h"
#include "poll-loop.h"
#include "seq.h"
#include "simap.h"
#include "smap.h"
#include "timer.h"
#include "unaligned.h"
#include "vlan-bitmap.h"
#include "openvswitch/vlog.h"
#include "ofproto-sim-provider.h"
#include <vswitch-idl.h>

VLOG_DEFINE_THIS_MODULE(ofproto_provider_sim);

COVERAGE_DEFINE(ofproto_sim_provider_expired);
COVERAGE_DEFINE(rev_reconfigure_sim);
COVERAGE_DEFINE(rev_bond_sim);
COVERAGE_DEFINE(rev_port_toggled_sim);

#define MAX_CMD_LEN 128
#define SWNS_EXEC "/sbin/ip netns exec swns"

static int
bundle_set_reconfigure(struct ofproto *ofproto_, int vid);

static void bundle_remove(struct ofport *);

static struct sim_provider_ofport_node *
sim_provider_ofport_node_cast(const struct ofport *ofport)
{
    return ofport ?
           CONTAINER_OF(ofport, struct sim_provider_ofport_node, up) : NULL;
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
    HMAP_FOR_EACH (ofproto,
                   all_sim_provider_node, &all_sim_provider_nodes) {
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
    if(port_type && (strcmp(port_type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0)) {
        return port_type;
    }
    return "system";
}

/* Basic life-cycle. */

static struct ofproto *
alloc(void)
{
    struct sim_provider_node *ofproto = xmalloc(sizeof *ofproto);
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
    int error=0;
    char ovs_addbr[80];

    if (strcmp(ofproto_->type, "vrf")) {
        sprintf(ovs_addbr, "%s add-br %s", OVS_VSCTL, ofproto->up.name);
        if (system(ovs_addbr) != 0) {
            VLOG_ERR("system command failure: %s, %s", ovs_addbr, strerror(errno));
            error = 1;
        }

        sprintf(ovs_addbr, "%s set br %s datapath_type=netdev", OVS_VSCTL, ofproto->up.name);
        if (system(ovs_addbr) != 0) {
            VLOG_ERR("system command failure: %s, %s", ovs_addbr, strerror(errno));
            error = 1;
        }

        sprintf(ovs_addbr, "%s set port %s trunks=0", OVS_VSCTL, ofproto->up.name);
        if (system(ovs_addbr) != 0) {
            VLOG_ERR("system command failure: %s, %s", ovs_addbr, strerror(errno));
            error = 1;
        }

        sprintf(ovs_addbr, "%s /sbin/ip link set dev %s up", SWNS_EXEC, ofproto->up.name);
        if (system(ovs_addbr) != 0) {
            VLOG_ERR("system command failure: %s, %s", ovs_addbr, strerror(errno));
            error = 1;
        }
    }
    ofproto->netflow = NULL;
    ofproto->stp = NULL;
    ofproto->rstp = NULL;
    ofproto->dump_seq = 0;
    hmap_init(&ofproto->bundles);
    ofproto->ms = NULL;
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

    if (strcmp(ofproto_->type, "vrf")) {
        sprintf(ovs_delbr, "%s del-br %s", OVS_VSCTL, ofproto->up.name);
        if (system(ovs_delbr) != 0) {
            VLOG_ERR("system command failure: %s, %s", ovs_delbr, strerror(errno));
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

static struct ofport *
port_alloc(void)
{
    struct sim_provider_ofport_node *port = xmalloc(sizeof *port);
    return &port->up;
}

static void
port_dealloc(struct ofport *port_)
{
    struct sim_provider_ofport_node *port =
           sim_provider_ofport_node_cast(port_);
    free(port);
}

static int
port_construct(struct ofport *port_)
{
    struct sim_provider_ofport_node *port =
           sim_provider_ofport_node_cast(port_);

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

    HMAP_FOR_EACH_IN_BUCKET (bundle, hmap_node, hash_pointer(aux, 0),
                             &ofproto->bundles) {
        if (bundle->aux == aux) {
            VLOG_DBG("returning bundle found");
            return bundle;
        }
    }
    return NULL;
}

static void
enable_l3(const char *intf_name)
{
    char cmd[MAX_CMD_LEN];

    snprintf(cmd, MAX_CMD_LEN, "%s /sbin/ip link set dev %s up",
            SWNS_EXEC, intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("system command failure: cmd=%s",cmd);
    }

    snprintf(cmd, MAX_CMD_LEN, "%s iptables -D INPUT -i %s -j DROP",
            SWNS_EXEC, intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("system command failure: cmd=%s",cmd);
    }

    snprintf(cmd, MAX_CMD_LEN, "%s iptables -D FORWARD -i %s -j DROP",
            SWNS_EXEC, intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("system command failure: cmd=%s",cmd);
    }
}

static void
disable_l3(const char *intf_name)
{
    char cmd[MAX_CMD_LEN];

    snprintf(cmd, MAX_CMD_LEN, "%s iptables -C INPUT -i %s -j DROP",
            SWNS_EXEC, intf_name);
    /*
     * Do not add drop rules if the "Check" command returned silently.
     * Silent return means that the check command passed and a rule exists
     */
    if (system(cmd) != 0) {
        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_LEN, "%s iptables -A INPUT -i %s -j DROP",
            SWNS_EXEC, intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }

        memset(cmd, 0, sizeof(cmd));
        snprintf(cmd, MAX_CMD_LEN, "%s iptables -A FORWARD -i %s -j DROP",
            SWNS_EXEC, intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
    }
}

/* Bundles. */
static int
bundle_set(struct ofproto *ofproto_, void *aux,
           const struct ofproto_bundle_settings *s)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    const struct ofport *ofport;
    int ofp_port, i, n;
    const char *opt_arg;
    char cmd_str[MAX_CLI];
    struct ofbundle *bundle;
    int vlan_count = 0;
    struct ofproto_bundle_settings *s_copy;
    struct sim_provider_ofport_node *port;
    int vlan_id;
    const char *type = NULL;

    if (s == NULL) {
        return ENODEV;
    }
    bundle = bundle_lookup(ofproto, aux);

    /* Cloning the necessary data members in s into s_copy for HMAP lookup
     * during reconfiguration of ports */
    if (!bundle) {
        bundle = xmalloc(sizeof(struct ofbundle));
        hmap_insert(&ofproto->bundles, &bundle->hmap_node,
                    hash_pointer(aux, 0));
        bundle->aux = aux;
        bundle->name = NULL;
        s_copy = &bundle->s_copy;
        memcpy(s_copy, s, sizeof(struct ofproto_bundle_settings));
        s_copy->name = xstrdup(s->name);
        s_copy->slaves= xmalloc(sizeof(ofp_port_t) * s->n_slaves);
        for (i=0; i < s->n_slaves; i++) {
            s_copy->slaves[i] = s->slaves[i];
        }
        if (s->trunks) {
            s_copy->trunks = bitmap_clone(s->trunks, VLAN_BITMAP_SIZE);
        } else {
            s_copy->trunks = NULL;
        }
        if (s->bond != NULL) {
            s_copy->bond->name = xstrdup(s->bond->name);
        }
    }

    int vlan = (s->vlan_mode == PORT_VLAN_TRUNK ? -1
            : s->vlan >= 0 && s->vlan <= 4095 ? s->vlan
            : 0);

    if (!bundle->name || strcmp(s->name, bundle->name)) {
        free(bundle->name);
        bundle->name = xstrdup(s->name);
    }

    VLOG_DBG("bridge/vrf name %s port name %s", ofproto->up.name, bundle->name);
    port = sim_provider_ofport_node_cast(ofproto_get_port(ofproto_, s->slaves[0]));
    if(port) {
       type = netdev_get_type(port->up.netdev);
    }

    vlan_id = s->vlan;

    /* Configure trunk for bridge interface so we receive vlan frames
     * on  internal vlan interfaces created on top of bridge.
     * Skip this for internal interface is bridge internal
     * interface with same name as bridge */
    if(type && (strcmp(type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0) && vlan_id &&
       (s->n_slaves == 1) && (strcmp(bundle->name, ofproto->up.name) != 0)) {

        if(strcmp(ofproto->up.type, "vrf")) {
            VLOG_INFO("bridge %s %s vland id %d", ofproto->up.name, type, vlan_id);
            bitmap_set1(ofproto->vlan_intf_bmp, vlan_id);
            n = snprintf(cmd_str, MAX_CLI - n, "%s set port %s ", OVS_VSCTL, ofproto->up.name);
            for (i=0; i < 4095; i++) {
                if (bitmap_is_set(ofproto->vlan_intf_bmp, i)) {
                    if (vlan_count == 0) {
                        n += snprintf(&cmd_str[n], MAX_CLI - n, " trunks=%d", i);
                    } else {
                        n += snprintf(&cmd_str[n], MAX_CLI - n, ",%d", i);
                    }

                    if (n > MAX_CLI - 1) {
                        VLOG_ERR("Command line string exceeds the buffer size");

                        return 0;
                    }
                    vlan_count += 1;
                }
            }

            if (system(cmd_str) != 0) {
                VLOG_ERR("system command failure: %s, %s", cmd_str, strerror(errno));
            }

            bundle->vlan = s->vlan;
        }
        goto done;
    }

    if (vlan != -1  && !bitmap_is_set(ofproto->vlans_bmp, vlan)) {
        return 0;
    }

    VLOG_DBG("bundle_set vlan= %d vlan_mode= %s VLAN port %s",
               vlan,
               s->vlan_mode == PORT_VLAN_ACCESS ? "ACCESS" :
               s->vlan_mode == PORT_VLAN_TRUNK  ? "TRUNK":
               s->vlan_mode == PORT_VLAN_NATIVE_UNTAGGED ? "NATIVE UNTAGGED" :
               s->vlan_mode == PORT_VLAN_NATIVE_TAGGED ? "NATIVE TAGGED" : "UNKNOWN",
               s->name);

    if (s->n_slaves == 1 && (s->n_slaves == s->slaves_entered)) {
        n = snprintf(cmd_str, MAX_CLI - n, "%s add-port %s", OVS_VSCTL, ofproto->up.name);
        VLOG_DBG("%s add-port %s", OVS_VSCTL, ofproto->up.name);
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    } else if (s->n_slaves == s->slaves_entered) {
        n = snprintf(cmd_str, MAX_CLI - n, "%s del-port %s", OVS_VSCTL, s->name);
        system(cmd_str);
        n = snprintf(cmd_str, MAX_CLI - n, "%s add-bond %s %s", OVS_VSCTL, ofproto->up.name, s->name);
        VLOG_DBG("%s add-bond %s %s", OVS_VSCTL, ofproto->up.name, s->name);
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    } else {
        return 0;
    }

    for (i=0; i < s->n_slaves; i++) {

        ofport = ofproto_get_port(ofproto_, s->slaves[i]);

        ofp_port = ofport ? ofport->ofp_port : OFPP_NONE;
        if (ofp_port == OFPP_NONE) {
            VLOG_WARN("Null ofport for bundle member# %d", i);
            continue;
        }

        if (s->n_slaves > 1) {
               VLOG_DBG("bundle_set member# %d port %s internal port# %d",
                            i, netdev_get_name(ofport->netdev) , ofp_port);
        } else {
                VLOG_DBG("port %s internal port# %d",
                            netdev_get_name(ofport->netdev) , ofp_port);
        }
        n += snprintf(&cmd_str[n], MAX_CLI - n, " %s", netdev_get_name(ofport->netdev));
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }

        if (strcmp(ofproto->up.type, "vrf")) {
            disable_l3(netdev_get_name(ofport->netdev));
        }
    }

    if (s->trunks) {
        uint32_t i;
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
        VLOG_DBG("trunk vlan list");
        for (i=0; i < 4095; i++) {
            if ((bitmap_is_set(s->trunks, i)) && (bitmap_is_set(ofproto->vlans_bmp, i))) {
                if (vlan_count == 0) {
                    n += snprintf(&cmd_str[n], MAX_CLI - n, " trunks=%d", i);
                } else {
                    n += snprintf(&cmd_str[n], MAX_CLI - n, ",%d", i);
                }

                if (n > MAX_CLI - 1) {
                    VLOG_ERR("Command line string exceeds the buffer size");

                    return 0;
                }
                vlan_count += 1;
            }
        }
        if (!vlan_count) {
            VLOG_DBG("No VLANs present ");
            return 0;
        }
    }

    if (vlan > 0) {
        n += snprintf(&cmd_str[n], MAX_CLI - n, " tag=%d", vlan);
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    }

    if (s->vlan_mode == PORT_VLAN_ACCESS) {
        n += snprintf(&cmd_str[n], MAX_CLI - n, " vlan_mode=access");
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    } else
    if (s->vlan_mode == PORT_VLAN_TRUNK) {
        n += snprintf(&cmd_str[n], MAX_CLI - n, " vlan_mode=trunk");
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    } else
    if (s->vlan_mode == PORT_VLAN_NATIVE_UNTAGGED) {
        n += snprintf(&cmd_str[n], MAX_CLI - n, " vlan_mode=native-untagged");
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    } else
    if (s->vlan_mode == PORT_VLAN_NATIVE_TAGGED) {
        n += snprintf(&cmd_str[n], MAX_CLI - n, " vlan_mode=native-tagged");
        if (n > MAX_CLI - 1) {
            VLOG_ERR("Command line string exceeds the buffer size");
            return 0;
        }
    }


    if (strcmp(ofproto_->type, "vrf")) {
        VLOG_DBG(cmd_str);
        if (system(cmd_str) != 0) {
            VLOG_ERR("system command failure: %s, %s", cmd_str, strerror(errno));
        }
    }

done:
    return 0;
}

/* Freeing up bundle and its members on heap */
static void
bundle_destroy(struct ofbundle *bundle, struct ofproto *ofproto_)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    char cmd_str[MAX_CLI];
    int n;

    if (!bundle) {
        return;
    }

    if (!ofproto) {
        return;
    }
    /* Remove vlan from bridge trunks for internal vlan interfaces*/
    if(strcmp(ofproto->up.type, "vrf") && bundle->s_copy.n_slaves == 1 &&
            strcmp(bundle->name, ofproto->up.name)) {
        struct sim_provider_ofport_node *port;
        int i, vlan_id, vlan_count = 0;
        const char *type = NULL;
        port = sim_provider_ofport_node_cast(ofproto_get_port(ofproto_, bundle->s_copy.slaves[0]));
        if(port) {
            type = netdev_get_type(port->up.netdev);
        }
        vlan_id = bundle->vlan;
        VLOG_DBG("bundle_destroy: bridge %s %s vland id %d", ofproto->up.name, type, vlan_id);
        if(type && (strcmp(type, OVSREC_INTERFACE_TYPE_INTERNAL) == 0) && vlan_id) {
            bitmap_set0(ofproto->vlan_intf_bmp, bundle->vlan);
            n = snprintf(cmd_str, MAX_CLI - n, "%s set port %s ", OVS_VSCTL, ofproto->up.name);
            for (i=0; i < 4095; i++) {
                if (bitmap_is_set(ofproto->vlan_intf_bmp, i)) {
                    if (vlan_count == 0) {
                        n += snprintf(&cmd_str[n], MAX_CLI - n, " trunks=%d", i);
                    } else {
                        n += snprintf(&cmd_str[n], MAX_CLI - n, ",%d", i);
                    }

                    if (n > MAX_CLI - 1) {
                        VLOG_ERR("Command line string exceeds the buffer size");
                        break;
                    }
                    vlan_count += 1;
                }
            }

            if (vlan_count == 0)
                n += snprintf(&cmd_str[n], MAX_CLI - n, " trunks=0");

            if (system(cmd_str) != 0) {
                VLOG_ERR("system command failure: %s, %s", cmd_str, strerror(errno));
            }
        }
    }

    hmap_remove(&ofproto->bundles, &bundle->hmap_node);

    if (bundle->s_copy.name) {
        free(bundle->s_copy.name);
    }

    if (bundle->s_copy.slaves) {
        free(bundle->s_copy.slaves);
    }

    if (bundle->s_copy.trunks) {
        bitmap_free(bundle->s_copy.trunks);
    }

    if (bundle->s_copy.bond != NULL ) {
        if (bundle->s_copy.bond->name) {
            free(bundle->s_copy.bond->name);
        }
    }

    done:
    free(bundle);
}

static void
bundle_delete(struct ofport *ofport)
{
    char cmd_str[80];
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofport->ofproto);
    struct ofbundle *bundle, *bundle_hmap;

    HMAP_FOR_EACH (bundle, hmap_node, &ofproto->bundles) {
        if (!strcmp(bundle->s_copy.name, netdev_get_name(ofport->netdev))) {
            if (strcmp(ofport->ofproto->type, "vrf")) {
                sprintf(cmd_str, "%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                VLOG_DBG("%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                system(cmd_str);
                enable_l3(bundle->s_copy.name);
            }
            bundle_destroy(bundle, ofport->ofproto);
            return;
        } else if (bundle->s_copy.bond != NULL) {
            if (!strcmp(bundle->s_copy.bond->name, bundle->s_copy.name)) {
                volatile struct ofport *ofport_loop;
                int i, j;
                if (strcmp(ofport->ofproto->type, "vrf")) {
                    sprintf(cmd_str, "%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                    VLOG_DBG("%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                    system(cmd_str);
                    enable_l3(bundle->s_copy.name);
                    for (i = 0; i < bundle->s_copy.slaves_entered; i++) {
                        ofport_loop = ofproto_get_port(ofport->ofproto, bundle->s_copy.slaves[i]);
                        enable_l3(netdev_get_name(ofport_loop->netdev));
                    }
                }
                bundle_destroy(bundle, ofport->ofproto);
                return;
            }
        }
    }
}

static void
bundle_remove(struct ofport *port)
{
    if (port) {
        if (port->netdev) {
            VLOG_DBG("%s delete port %s", __FUNCTION__,
                      netdev_get_name(port->netdev));
        }
        bundle_delete(port);
    }
    return;
}

static int
bundle_get(struct ofproto *ofproto_, void *aux, int *bundle_handle)
{
    return 0;
}


static int
set_vlan(struct ofproto *ofproto, int vid, bool add)
{
    struct sim_provider_node *sim_ofproto = sim_provider_node_cast(ofproto);

    VLOG_DBG("%s: entry, vid=%d, oper=%s", __FUNCTION__,
              vid, (add ? "add":"del"));
    if (add) {
        VLOG_DBG("VLAN add %d ", vid);
        bitmap_set1(sim_ofproto->vlans_bmp, vid);

    } else {
        VLOG_DBG("VLAN del %d ", vid);
        bitmap_set0(sim_ofproto->vlans_bmp, vid);
    }
    bundle_set_reconfigure(ofproto, vid);

    return 0;
}


static int
bundle_set_reconfigure(struct ofproto *ofproto_, int vid)
{
    struct sim_provider_node *ofproto = sim_provider_node_cast(ofproto_);
    char cmd_str[80];
    struct ofbundle *bundle;

    HMAP_FOR_EACH (bundle, hmap_node, &ofproto->bundles) {
        if ((bundle->s_copy.vlan == vid) || (bundle->s_copy.trunks &&
                                            bitmap_is_set(bundle->s_copy.trunks, vid))) {
            if (strcmp(ofproto_->type, "vrf")) {
                sprintf(cmd_str, "%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                VLOG_DBG("%s del-port %s", OVS_VSCTL, bundle->s_copy.name);
                system(cmd_str);
                enable_l3(bundle->s_copy.name);
            }
            bundle_set(ofproto_, bundle->aux, &bundle->s_copy);
        } else {
            VLOG_DBG("VLAN Id %d doesn't match port= %s",
            vid, bundle->s_copy.name);
        }
    }

    return 0;
}


/* Mirrors. */

static int
mirror_get_stats__(struct ofproto *ofproto OVS_UNUSED, void *aux OVS_UNUSED,
                   uint64_t *packets OVS_UNUSED, uint64_t *bytes OVS_UNUSED)
{
    return 0;
}

static bool
is_mirror_output_bundle(const struct ofproto *ofproto_ OVS_UNUSED, void *aux OVS_UNUSED)
{
    return false;
}

static void
forward_bpdu_changed(struct ofproto *ofproto_ OVS_UNUSED)
{
    return;
}

/* Ports. */

static struct sim_provider_ofport_node *
get_ofp_port(const struct sim_provider_node *ofproto, ofp_port_t ofp_port)
{
    struct ofport *ofport = ofproto_get_port(&ofproto->up, ofp_port);
    return ofport ? sim_provider_ofport_node_cast(ofport) : NULL;
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
                   ofproto_port->name, ofproto_port->type, ofproto_port->ofp_port);
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
    struct sim_provider_ofport_node *ofport =
           get_ofp_port(ofproto, ofp_port);
    int error = 0;

    return error;
}

static int
port_get_stats(const struct ofport *ofport_, struct netdev_stats *stats)
{
    struct sim_provider_ofport_node *ofport =
           sim_provider_ofport_node_cast(ofport_);
    int error;

    error = netdev_get_stats(ofport->up.netdev, stats);

    if (!error && ofport_->ofp_port == OFPP_LOCAL) {
        struct sim_provider_node *ofproto =
               sim_provider_node_cast(ofport->up.ofproto);

        ovs_mutex_lock(&ofproto->stats_mutex);
        /* ofproto->stats.tx_packets represents packets that we created
         * internally and sent to some port
         * Account for them as if they had come from OFPP_LOCAL and
         * got forwarded.
         */

        if (stats->rx_packets != UINT64_MAX) {
            stats->rx_packets += ofproto->stats.tx_packets;
        }

        if (stats->rx_bytes != UINT64_MAX) {
            stats->rx_bytes += ofproto->stats.tx_bytes;
        }

        /* ofproto->stats.rx_packets represents packets that were received on
         * some port and we processed internally and dropped (e.g. STP).
         * Account for them as if they had been forwarded to OFPP_LOCAL.
         */

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
    *statep = xzalloc(sizeof(struct sim_provider_port_dump_state));
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
              *sim_provider_rule_cast(const struct rule *rule)
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

static enum ofperr
rule_insert(struct rule *rule_ OVS_UNUSED)
    OVS_REQUIRES(ofproto_mutex)
{
    return 0;
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
rule_get_stats(struct rule *rule_ OVS_UNUSED, uint64_t *packets OVS_UNUSED,
               uint64_t *bytes OVS_UNUSED, long long int *used OVS_UNUSED)
{
    return;
}

static enum ofperr
rule_execute(struct rule *rule OVS_UNUSED, const struct flow *flow OVS_UNUSED,
             struct ofpbuf *packet OVS_UNUSED)
{
    return 0;
}

static void
rule_modify_actions(struct rule *rule_ OVS_UNUSED, bool reset_counters OVS_UNUSED)
    OVS_REQUIRES(ofproto_mutex)
{
    return;
}

static struct sim_provider_group
              *sim_provider_group_cast(const struct ofgroup *group)
{
    return group ?
           CONTAINER_OF(group, struct sim_provider_group, up) : NULL;
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
packet_out(struct ofproto *ofproto_ OVS_UNUSED, struct ofpbuf *packet OVS_UNUSED,
           const struct flow *flow OVS_UNUSED,
           const struct ofpact *ofpacts OVS_UNUSED, size_t ofpacts_len OVS_UNUSED)
{
    return 0;
}

static void
get_netflow_ids(const struct ofproto *ofproto_ OVS_UNUSED,
                uint8_t *engine_type OVS_UNUSED, uint8_t *engine_id OVS_UNUSED)
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
                   netdev_get_name(ofport->netdev),
                   ofport->ofp_port, vlan_arg);
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
    NULL,                       /* rule_premodify_actions */
    rule_modify_actions,
    set_frag_handling,
    packet_out,
    NULL,                       /* may implement set_netflow */
    get_netflow_ids,
    NULL,                       /* may implement set_sflow */
    NULL,                       /* may implement set_ipfix */
    NULL,                       /* may implement set_cfm */
    cfm_status_changed,
    NULL,                       /* may implement get_cfm_status */
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
    NULL,                       /* may implement mirror_set__ */
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
#if 0
    set_config,                  /* set_config options */
#endif
#ifdef HALON
    NULL,                       /* add_l3_host_entry */
    NULL,                       /* delete_l3_host_entry */
    NULL,                       /* get_l3_host_hit */
    NULL,                       /* l3_route_action */
    NULL,                       /* l3_ecmp_set */
    NULL,                       /* l3_ecmp_hash_set */
    NULL,                       /* l3_host_action */
#endif
};
