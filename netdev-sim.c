/*
 * Copyright (c) 2010, 2011, 2012, 2013 Nicira, Inc.
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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
 */

#include <config.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/ethtool.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include <openhalon-idl.h>

#include "openvswitch/vlog.h"
#include "netdev-sim.h"

#define SWNS_EXEC       "/sbin/ip netns exec swns"

VLOG_DEFINE_THIS_MODULE(netdev_sim);

/* Protects 'sim_list'. */
static struct ovs_mutex sim_list_mutex = OVS_MUTEX_INITIALIZER;

/* Contains all 'struct sim_dev's. */
static struct ovs_list sim_list OVS_GUARDED_BY(sim_list_mutex)
    = OVS_LIST_INITIALIZER(&sim_list);

struct netdev_sim {
    struct netdev up;

    /* In sim_list. */
    struct ovs_list list_node OVS_GUARDED_BY(sim_list_mutex);

    /* Protects all members below. */
    struct ovs_mutex mutex OVS_ACQ_AFTER(sim_list_mutex);

    uint8_t hwaddr[ETH_ADDR_LEN] OVS_GUARDED;
    char hw_addr_str[18];
    int mtu OVS_GUARDED;
    struct netdev_stats stats OVS_GUARDED;
    enum netdev_flags flags OVS_GUARDED;

    int linux_intf_state;
    char linux_intf_name[16];
    bool is_layer3;
    /* Indicate if rules are configured */
    bool iptable_drop_rule_inserted;
    bool iptable_accept_rule_inserted;
};

static int netdev_sim_construct(struct netdev *);

static bool
is_sim_class(const struct netdev_class *class)
{
    return class->construct == netdev_sim_construct;
}

static struct netdev_sim *
netdev_sim_cast(const struct netdev *netdev)
{
    ovs_assert(is_sim_class(netdev_get_class(netdev)));
    return CONTAINER_OF(netdev, struct netdev_sim, up);
}

static struct netdev *
netdev_sim_alloc(void)
{
    struct netdev_sim *netdev = xzalloc(sizeof *netdev);
    return &netdev->up;
}

static int
netdev_sim_construct(struct netdev *netdev_)
{
    static atomic_count next_n = ATOMIC_COUNT_INIT(0xaa550000);
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    unsigned int n;

    n = atomic_count_inc(&next_n);

    ovs_mutex_init(&netdev->mutex);
    ovs_mutex_lock(&netdev->mutex);
    netdev->hwaddr[0] = 0xaa;
    netdev->hwaddr[1] = 0x55;
    netdev->hwaddr[2] = n >> 24;
    netdev->hwaddr[3] = n >> 16;
    netdev->hwaddr[4] = n >> 8;
    netdev->hwaddr[5] = n;
    netdev->mtu = 1500;
    netdev->flags = 0;
    netdev->linux_intf_state = 0;

    ovs_mutex_unlock(&netdev->mutex);

    ovs_mutex_lock(&sim_list_mutex);
    list_push_back(&sim_list, &netdev->list_node);
    ovs_mutex_unlock(&sim_list_mutex);

    return 0;
}

static void
netdev_sim_destruct(struct netdev *netdev_)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    ovs_mutex_lock(&sim_list_mutex);
    list_remove(&netdev->list_node);
    ovs_mutex_unlock(&sim_list_mutex);
}

static void
netdev_sim_dealloc(struct netdev *netdev_)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    free(netdev);
}

static void
netdev_sim_run(void)
{
    /* HALON_TODO: Currently we are not supporting to change the
     * link state on the go. So ignoring this function. */
}

static int
netdev_sim_set_hw_intf_info(struct netdev *netdev_, const struct smap *args)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    const char *hw_id = smap_get(args, "switch_intf_id");
    const char *mac_addr = smap_get(args, "mac_addr");
    struct ether_addr *ether_mac = NULL;
    char cmd[1024];

    ovs_mutex_lock(&netdev->mutex);

    if (hw_id != NULL) {
        strncpy(netdev->linux_intf_name, hw_id, sizeof(netdev->linux_intf_name));
    } else {
        VLOG_ERR("Invalid switch interface name %s", hw_id);
    }

    if(mac_addr != NULL) {
        strncpy(netdev->hw_addr_str, mac_addr, sizeof(netdev->hw_addr_str));
    } else {
        VLOG_ERR("Invalid mac address %s", mac_addr);
    }
    
    sprintf(cmd, "%s /sbin/ip link set dev %s down",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s",cmd);
    }
    
    sprintf(cmd, "%s /sbin/ip link set %s address %s",
            SWNS_EXEC, netdev->up.name, netdev->hw_addr_str);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s",cmd);
    }
    
    sprintf(cmd, "%s /sbin/ip link set dev %s up",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s",cmd);
    }

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_set_hw_intf_config(struct netdev *netdev_, const struct smap *args)
{
    char cmd[80], cmd_drop_input[80], cmd_drop_fwd[80];
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    const char *hw_enable = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE);

    VLOG_DBG("Setting up physical interfaces, %s", netdev->linux_intf_name);

    ovs_mutex_lock(&netdev->mutex);

    VLOG_DBG("hw_enable %s -- Interface %s ", hw_enable, netdev->linux_intf_name);

    if (hw_enable) {
        memset(cmd, 0, sizeof(cmd));
        memset(cmd_drop_input, 0, sizeof(cmd_drop_input));
        memset(cmd_drop_fwd, 0, sizeof(cmd_drop_fwd));

        if (!strcmp(hw_enable, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE_TRUE)) {
            netdev->flags |= NETDEV_UP;
            netdev->linux_intf_state = 1;
            sprintf(cmd, "%s /sbin/ip link set dev %s up",
                    SWNS_EXEC, netdev->linux_intf_name);
            if (!netdev->is_layer3 && !netdev->iptable_drop_rule_inserted) {
                sprintf(cmd_drop_input, "%s iptables -A INPUT -i %s -j DROP",
                        SWNS_EXEC, netdev->linux_intf_name);
                sprintf(cmd_drop_fwd, "%s iptables -A FORWARD -i %s -j DROP",
                        SWNS_EXEC, netdev->linux_intf_name);
            }
            netdev->iptable_drop_rule_inserted = 1;

        } else {
            netdev->flags &= ~NETDEV_UP;
            netdev->linux_intf_state = 0;
            sprintf(cmd, "%s /sbin/ip link set dev %s down",
                    SWNS_EXEC, netdev->linux_intf_name);
            if (!netdev->is_layer3 && netdev->iptable_drop_rule_inserted) {
                sprintf(cmd_drop_input, "%s iptables -D INPUT -i %s -j DROP",
                        SWNS_EXEC, netdev->linux_intf_name);
                sprintf(cmd_drop_fwd, "%s iptables -D FORWARD -i %s -j DROP",
                        SWNS_EXEC, netdev->linux_intf_name);
            }
            netdev->iptable_drop_rule_inserted = 0;
        }
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
        if (system(cmd_drop_input) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd_drop_input);
        }
        if (system(cmd_drop_fwd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd_drop_fwd);
        }
    }

    netdev_change_seq_changed(netdev_);

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_set_etheraddr(struct netdev *netdev,
                           const uint8_t mac[ETH_ADDR_LEN])
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    ovs_mutex_lock(&dev->mutex);
    if (!eth_addr_equals(dev->hwaddr, mac)) {
        memcpy(dev->hwaddr, mac, ETH_ADDR_LEN);
        netdev_change_seq_changed(netdev);
    }
    ovs_mutex_unlock(&dev->mutex);

    return 0;
}

static int
netdev_sim_get_etheraddr(const struct netdev *netdev,
                           uint8_t mac[ETH_ADDR_LEN])
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    ovs_mutex_lock(&dev->mutex);
    memcpy(mac, dev->hwaddr, ETH_ADDR_LEN);
    ovs_mutex_unlock(&dev->mutex);

    return 0;
}

static int
netdev_sim_get_stats(const struct netdev *netdev, struct netdev_stats *stats)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    ovs_mutex_lock(&dev->mutex);
    *stats = dev->stats;
    ovs_mutex_unlock(&dev->mutex);

    return 0;
}

static int
netdev_sim_update_flags(struct netdev *netdev_,
                          enum netdev_flags off, enum netdev_flags on,
                          enum netdev_flags *old_flagsp)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    /* HALON_TODO: Currently we are not supporting changing the
     * configuration using the FLAGS. So ignoring the
     * incoming on/off flags. */
    if ((off | on) & ~(NETDEV_UP | NETDEV_PROMISC)) {
        return EINVAL;
    }

    ovs_mutex_lock(&netdev->mutex);
    *old_flagsp = netdev->flags;
    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_get_carrier(const struct netdev *netdev_, bool *carrier)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    ovs_mutex_lock(&netdev->mutex);
    *carrier = netdev->linux_intf_state;
    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_enable_l3(const struct netdev *netdev_, int vrf_id)
{
    char cmd[120];
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    VLOG_DBG("Enabling l3 for interface %s",netdev->linux_intf_name);

    ovs_mutex_lock(&netdev->mutex);

    netdev->is_layer3 = 1;

    memset(cmd, 0, sizeof(cmd));
    sprintf(cmd, "%s /sbin/ip link set dev %s up",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("system command failure: cmd=%s",cmd);
    }

    if (netdev->iptable_drop_rule_inserted) {
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -D INPUT -i %s -j DROP",
                SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -D FORWARD -i %s -j DROP",
                SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
        netdev->iptable_drop_rule_inserted = 0;
    }

    if (!netdev->iptable_accept_rule_inserted) {
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -A INPUT -i %s -j ACCEPT",
                SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -A FORWARD -i %s -j ACCEPT",
            SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
        netdev->iptable_accept_rule_inserted = 1;
    }

    netdev_change_seq_changed(netdev_);

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_disable_l3(const struct netdev *netdev_, int vrf_id)
{
    char cmd[120];
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    VLOG_DBG("Disabling l3 for interface %s",netdev->linux_intf_name);

    ovs_mutex_lock(&netdev->mutex);

    netdev->is_layer3 = 0;

    if (netdev->iptable_accept_rule_inserted) {
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -D INPUT -i %s -j ACCEPT",
            SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -D FORWARD -i %s -j ACCEPT",
            SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
        netdev->iptable_accept_rule_inserted = 0;
    }

    if (!netdev->iptable_drop_rule_inserted) {
        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -A INPUT -i %s -j DROP",
                SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }

        memset(cmd, 0, sizeof(cmd));
        sprintf(cmd, "%s iptables -A FORWARD -i %s -j DROP",
                SWNS_EXEC, netdev->linux_intf_name);
        if (system(cmd) != 0) {
            VLOG_ERR("system command failure: cmd=%s",cmd);
        }
        netdev->iptable_drop_rule_inserted = 1;
    }

    netdev_change_seq_changed(netdev_);

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

/* Helper functions. */

static const struct netdev_class sim_class = {
    "system",
    NULL,                       /* init */
    netdev_sim_run,
    NULL,                       /* wait */

    netdev_sim_alloc,
    netdev_sim_construct,
    netdev_sim_destruct,
    netdev_sim_dealloc,
    NULL,                       /* get_config */
    NULL,                       /* set_config */
    netdev_sim_set_hw_intf_info,
    netdev_sim_set_hw_intf_config,
    NULL,                       /* get_tunnel_config */
    NULL,                       /* build header */
    NULL,                       /* push header */
    NULL,                       /* pop header */
    NULL,                       /* get_numa_id */
    NULL,                       /* set_multiq */

    NULL,                       /* send */
    NULL,                       /* send_wait */

    netdev_sim_set_etheraddr,
    netdev_sim_get_etheraddr,
    NULL,                       /* get_mtu */
    NULL,                       /* set_mtu */
    NULL,                       /* get_ifindex */
    netdev_sim_get_carrier,
    NULL,                       /* get_carrier_resets */
    NULL,                       /* get_miimon */
    netdev_sim_get_stats,

    NULL,                       /* get_features */
    NULL,                       /* set_advertisements */

    NULL,                       /* set_policing */
    NULL,                       /* get_qos_types */
    NULL,                       /* get_qos_capabilities */
    NULL,                       /* get_qos */
    NULL,                       /* set_qos */
    NULL,                       /* get_queue */
    NULL,                       /* set_queue */
    NULL,                       /* delete_queue */
    NULL,                       /* get_queue_stats */
    NULL,                       /* queue_dump_start */
    NULL,                       /* queue_dump_next */
    NULL,                       /* queue_dump_done */
    NULL,                       /* dump_queue_stats */

    netdev_sim_enable_l3,       /* enable_l3 */
    netdev_sim_disable_l3,      /* disable_l3 */
    NULL,                       /* get_in4 */
    NULL,                       /* set_in4 */
    NULL,                       /* get_in6 */
    NULL,                       /* add_router */
    NULL,                       /* get_next_hop */
    NULL,                       /* get_status */
    NULL,                       /* arp_lookup */

    netdev_sim_update_flags,

    NULL,                       /* rxq_alloc */
    NULL,                       /* rxq_construct */
    NULL,                       /* rxq_destruct */
    NULL,                       /* rxq_dealloc */
    NULL,                       /* rxq_recv */
    NULL,                       /* rxq_wait */
    NULL,                       /* rxq_drain */
};

void
netdev_sim_register(void)
{
    netdev_register_provider(&sim_class);
}
