/*
 * Copyright (c) 2010, 2011, 2012, 2013 Nicira, Inc.
 * Copyright (C) 2015, 2016 Hewlett-Packard Development Company, L.P.
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

#include <openswitch-idl.h>

#include "openvswitch/vlog.h"
#include "netdev-sim.h"
#include "ovs-atomic.h"

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
    struct netdev_stats stats OVS_GUARDED;
    enum netdev_flags flags OVS_GUARDED;

    char linux_intf_name[16];
    int link_state;
    uint32_t hw_info_link_speed;
    uint32_t link_speed;
    uint32_t mtu;
    bool autoneg;
    bool pause_tx;
    bool pause_rx;
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
    netdev->link_state = 0;

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
    const char *max_speed = smap_get(args, INTERFACE_HW_INTF_INFO_MAP_MAX_SPEED);
    const char *mac_addr = smap_get(args, INTERFACE_HW_INTF_INFO_MAP_MAC_ADDR);
    char cmd[1024];

    ovs_mutex_lock(&netdev->mutex);

    strncpy(netdev->linux_intf_name, netdev->up.name, sizeof(netdev->linux_intf_name));

    /* In simulator it is assumed that interfaces always
     * link up at max_speed listed in hardware info. */
    if(max_speed)
        netdev->hw_info_link_speed = atoi(max_speed);

    if(mac_addr != NULL) {
        strncpy(netdev->hw_addr_str, mac_addr, sizeof(netdev->hw_addr_str));
    } else {
        VLOG_ERR("Invalid mac address %s", mac_addr);
    }

    sprintf(cmd, "%s /sbin/ip link set dev %s down",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s", cmd);
    }

    sprintf(cmd, "%s /sbin/ip link set %s address %s",
            SWNS_EXEC, netdev->up.name, netdev->hw_addr_str);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s", cmd);
    }

    sprintf(cmd, "%s /sbin/ip link set dev %s up",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s", cmd);
    }

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static void
get_interface_pause_config(const char *pause_cfg, bool *pause_rx, bool *pause_tx)
{
    *pause_rx = false;
    *pause_tx = false;

        /* Pause configuration. */
    if (STR_EQ(pause_cfg, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_RX)) {
        *pause_rx = true;
    } else if (STR_EQ(pause_cfg, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_TX)) {
        *pause_tx = true;
    } else if (STR_EQ(pause_cfg, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE_RXTX)) {
        *pause_rx = true;
        *pause_tx = true;
    }
}

static int
netdev_sim_set_hw_intf_config(struct netdev *netdev_, const struct smap *args)
{
    char cmd[80];
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    const bool hw_enable = smap_get_bool(args, INTERFACE_HW_INTF_CONFIG_MAP_ENABLE, false);
    const bool autoneg = smap_get_bool(args, INTERFACE_HW_INTF_CONFIG_MAP_AUTONEG, false);
    const char *pause = smap_get(args, INTERFACE_HW_INTF_CONFIG_MAP_PAUSE);
    const int mtu = smap_get_int(args, INTERFACE_HW_INTF_CONFIG_MAP_MTU, 0);

    VLOG_DBG("Setting up physical interfaces, %s", netdev->linux_intf_name);

    ovs_mutex_lock(&netdev->mutex);

    VLOG_DBG("Interface=%s hw_enable=%d ", netdev->linux_intf_name, hw_enable);

    memset(cmd, 0, sizeof(cmd));

    if (hw_enable) {
        netdev->flags |= NETDEV_UP;
        netdev->link_state = 1;

        /* In simulator Links always come up at its max speed. */
        netdev->link_speed = netdev->hw_info_link_speed;
        netdev->mtu = mtu;
        netdev->autoneg = autoneg;
        if(pause)
            get_interface_pause_config(pause, &(netdev->pause_rx), &(netdev->pause_tx));

        sprintf(cmd, "%s /sbin/ip link set dev %s up",
                SWNS_EXEC, netdev->linux_intf_name);
    } else {
        netdev->flags &= ~NETDEV_UP;
        netdev->link_state = 0;
        netdev->link_speed = 0;
        netdev->mtu = 0;
        netdev->autoneg = false;
        netdev->pause_tx = false;
        netdev->pause_rx = false;

        sprintf(cmd, "%s /sbin/ip link set dev %s down",
                SWNS_EXEC, netdev->linux_intf_name);
    }
    if (system(cmd) != 0) {
        VLOG_ERR("system command failure: cmd=%s",cmd);
    }

    netdev_change_seq_changed(netdev_);

    ovs_mutex_unlock(&netdev->mutex);

    return 0;
}

static int
netdev_sim_set_etheraddr(struct netdev *netdev,
                           const struct eth_addr mac)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    ovs_mutex_lock(&dev->mutex);
    if (memcmp(dev->hwaddr, mac.ea, ETH_ADDR_LEN)) {
        memcpy(dev->hwaddr, mac.ea, ETH_ADDR_LEN);
        netdev_change_seq_changed(netdev);
    }
    ovs_mutex_unlock(&dev->mutex);

    return 0;
}

static int
netdev_sim_get_etheraddr(const struct netdev *netdev,
                           struct eth_addr *mac)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    ovs_mutex_lock(&dev->mutex);
    memcpy(mac->ea, dev->hwaddr, ETH_ADDR_LEN);
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
netdev_sim_get_features(const struct netdev *netdev_,
                        enum netdev_features *current,
                        enum netdev_features *advertised,
                        enum netdev_features *supported,
                        enum netdev_features *peer)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);

    ovs_mutex_lock(&netdev->mutex);

    *current = 0;

    /* Current settings. */
    if (netdev->link_speed == SPEED_10) {
        *current |= NETDEV_F_10MB_FD;
    } else if (netdev->link_speed == SPEED_100) {
        *current |= NETDEV_F_100MB_FD;
    } else if (netdev->link_speed == SPEED_1000) {
        *current |= NETDEV_F_1GB_FD;
    } else if (netdev->link_speed == SPEED_10000) {
        *current |= NETDEV_F_10GB_FD;
    } else if (netdev->link_speed == 40000) {
        *current |= NETDEV_F_40GB_FD;
    } else if (netdev->link_speed == 100000) {
        *current |= NETDEV_F_100GB_FD;
    }

    if (netdev->autoneg) {
        *current |= NETDEV_F_AUTONEG;
    }

    if (netdev->pause_tx && netdev->pause_rx) {
        *current |= NETDEV_F_PAUSE;
    } else if (netdev->pause_rx) {
        *current |= NETDEV_F_PAUSE;
        *current |= NETDEV_F_PAUSE_ASYM;
    } else if (netdev->pause_tx) {
        *current |= NETDEV_F_PAUSE_ASYM;
    }

    ovs_mutex_unlock(&netdev->mutex);

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
    *carrier = netdev->link_state;
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

    netdev_sim_get_features,    /* get_features */
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

static const struct netdev_class sim_internal_class = {
    "internal",
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

    netdev_sim_get_features,    /* get_features */
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
    netdev_register_provider(&sim_internal_class);
}
