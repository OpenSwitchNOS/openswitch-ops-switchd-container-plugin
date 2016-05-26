/*
 * Copyright (c) 2010, 2011, 2012, 2013 Nicira, Inc.
 * Copyright (c) 2015-2016 Hewlett Packard Enterprise Development LP
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

#include "netdev-sim.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/ethernet.h>
#include <linux/ethtool.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>

#include "openswitch-idl.h"
#include "openvswitch/vlog.h"
#include "ovs-atomic.h"

VLOG_DEFINE_THIS_MODULE(netdev_sim);

#define NUM_QUEUES 8

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

    /* used for maintaining L3 sflow stats */
    bool     sflow_stats_enabled;
    uint32_t sflow_resets;
    uint64_t sflow_prev_ingress_pkts;
    uint64_t sflow_prev_ingress_bytes;
    uint64_t sflow_prev_egress_pkts;
    uint64_t sflow_prev_egress_bytes;

    /* used for maintaining general L3 stats */
    bool l3_stats_enabled;
};

struct kernel_l3_stats {
    uint64_t uc_packets;
    uint64_t uc_bytes;
    uint64_t mc_packets;
    uint64_t mc_bytes;
};

static int netdev_sim_construct(struct netdev *);

static void
netdev_parse_netlink_msg(struct nlmsghdr *h, struct netdev_stats *stats);

static int
netdev_get_kernel_stats(const char *if_name, struct netdev_stats *stats);

static int
netdev_sim_get_kernel_l3_stats(const char *if_name, struct netdev_stats *stats);

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
    netdev->sflow_resets = 0;
    netdev->sflow_stats_enabled = false;
    netdev->sflow_prev_ingress_pkts = 0;
    netdev->sflow_prev_ingress_bytes = 0;
    netdev->sflow_prev_egress_pkts = 0;
    netdev->sflow_prev_egress_bytes = 0;
    netdev->l3_stats_enabled = false;

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

    snprintf(cmd, sizeof(cmd), "%s /sbin/ip link set dev %s down",
            SWNS_EXEC, netdev->linux_intf_name);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s", cmd);
    }

    snprintf(cmd, sizeof(cmd), "%s /sbin/ip link set %s address %s",
            SWNS_EXEC, netdev->up.name, netdev->hw_addr_str);
    if (system(cmd) != 0) {
        VLOG_ERR("NETDEV-SIM | system command failure cmd=%s", cmd);
    }

    snprintf(cmd, sizeof(cmd), "%s /sbin/ip link set dev %s up",
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
netdev_sim_set_config(struct netdev *netdev_, const struct smap *args)
{

    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    int   vlan = smap_get_int(args, "vlan", 0);
    const char * parent = smap_get(args, "parent_intf_name");

    ovs_mutex_lock(&netdev->mutex);
    VLOG_DBG("vlan %d\n", vlan);
    VLOG_DBG("parent_intf_name %s\n", parent ? parent : NULL );

    if (0 != vlan) {
        netdev->flags |= NETDEV_UP;
    } else {
        netdev->flags &= ~NETDEV_UP;
    }

    ovs_mutex_unlock(&netdev->mutex);
    return 0;
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

/* Gets the sflow counters from iptable rules. They get reset to zero when
 * sflow gets disabled and re-enabled.
 */
static void
netdev_sim_get_iptable_stats(char *port, bool ingress, uint64_t *pkts,
                             uint64_t *bytes)
{
    FILE *fp;
    char cmd_str[MAX_CMD_LEN];
    char buffer[16] = {0};

    *pkts = 0;
    *bytes = 0;
    snprintf(cmd_str, sizeof(cmd_str), "%s iptables -S -v | grep SFLOW | grep '%c %s' "
             "| awk -F ' ' '{print $12}' | awk '{ sum+=$1} END {print sum}'",
             SWNS_EXEC, ingress ? 'i' : 'o', port);
    fp = popen(cmd_str, "r");
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        VLOG_ERR("sflow counters: cannot read iptable rules");
        pclose(fp);
        return;
    }
    *pkts = atoll(buffer);
    pclose(fp);

    snprintf(cmd_str, sizeof(cmd_str), "%s iptables -S -v | grep SFLOW | grep '%c %s' "
             "| awk -F ' ' '{print $13}' | awk '{ sum+=$1} END {print sum}'",
             SWNS_EXEC, ingress ? 'i' : 'o', port);
    fp = popen(cmd_str, "r");
    if (fgets(buffer, sizeof(buffer), fp) == NULL) {
        VLOG_ERR("sflow counters: cannot read iptable rules");
        pclose(fp);
        return;
    }
    *bytes = atoll(buffer);
    pclose(fp);

}

static void
netdev_sim_update_sflow_stats(struct netdev_sim *netdev)
{
    uint64_t in_pkts = 0, in_bytes = 0;
    uint64_t out_pkts = 0, out_bytes = 0;

    if (netdev->sflow_stats_enabled) {
        netdev_sim_get_iptable_stats(netdev->up.name, true,
                                     &in_pkts, &in_bytes);
        netdev_sim_get_iptable_stats(netdev->up.name, false,
                                     &out_pkts, &out_bytes);
    }

    ovs_mutex_lock(&netdev->mutex);

    /* Note: sFlow stats is only supported for L3 interfaces because sampling in
       L2 interfaces is done in sim OVS which does not offer statistics. */

    if (netdev->sflow_resets > 0 ||
        netdev->sflow_prev_ingress_pkts > in_pkts ||
        netdev->sflow_prev_egress_pkts > out_pkts) {
        /* This part of the code is run when sflow is enabled/disabled
           on an interface(s). When sflow is disabled, the iptable rules
           are removed. So the next time the interface shows up (sFlow was enabled
           on it again), the number of packets from iptables must be added
           directly with the existing count. */
        netdev->stats.sflow_ingress_packets += in_pkts;
        netdev->stats.sflow_ingress_bytes += in_bytes;
        netdev->stats.sflow_egress_packets += out_pkts;
        netdev->stats.sflow_egress_bytes += out_bytes;
    } else {
        /* This is the normal flow. After getting the stats from iptable
           rules, we need to get the delta (i.e. Number of packets
           from the iptable rules - Number of packets previously written
           to the DB) */
        netdev->stats.sflow_ingress_packets += in_pkts -
                                            netdev->sflow_prev_ingress_pkts;
        netdev->stats.sflow_ingress_bytes += in_bytes -
                                            netdev->sflow_prev_ingress_bytes;
        netdev->stats.sflow_egress_packets += out_pkts -
                                            netdev->sflow_prev_egress_pkts;
        netdev->stats.sflow_egress_bytes += out_bytes -
                                            netdev->sflow_prev_egress_bytes;
    }

    /* Update the previous counters so that we can find delta in next run */
    netdev->sflow_prev_ingress_pkts = in_pkts;
    netdev->sflow_prev_ingress_bytes = in_bytes;
    netdev->sflow_prev_egress_pkts = out_pkts;
    netdev->sflow_prev_egress_bytes = out_bytes;
    netdev->sflow_resets = 0;

    ovs_mutex_unlock(&netdev->mutex);
}

int
netdev_sim_dump_queue_stats(const struct netdev* netdev,
                            netdev_dump_queue_stats_cb* cb,
                            void* aux)
{
    /* static struct that mimics stateful hardware stats data */
    static struct netdev_queue_stats lstats[NUM_QUEUES];

    struct netdev_sim *dev = netdev_sim_cast(netdev);
    int q = 0;

    ovs_mutex_lock(&dev->mutex);

    for (q=0; q<NUM_QUEUES; q++) {

        /* fake queue stats increase */
        if (lstats[q].tx_bytes == 0)
            lstats[q].tx_bytes = q+1;
        else
            lstats[q].tx_bytes += 10;

        if (lstats[q].tx_packets == 0)
            lstats[q].tx_packets = q+1;
        else
            lstats[q].tx_packets += 10;

        if (lstats[q].tx_errors == 0)
            lstats[q].tx_errors = q+1;
        else
            lstats[q].tx_errors += 10;

        /* punt our PI qstats array on through aux */
        (*cb)(q, &lstats[q], aux);
    }

    ovs_mutex_unlock(&dev->mutex);

    return 0;
}

static int
netdev_sim_get_stats(const struct netdev *netdev, struct netdev_stats *stats)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);

    int rc = 0;
    rc = netdev_get_kernel_stats(dev->linux_intf_name, &dev->stats);
    if (rc < 0)
    {
        VLOG_ERR("Failed to get interface statistics for interface %s", dev->linux_intf_name);
        return -1;
    }

    /* If L3 stats are enabled fetch statistics from iptables*/
    if (dev->l3_stats_enabled) {
        rc = netdev_sim_get_kernel_l3_stats(dev->linux_intf_name, &dev->stats);
        if (rc < 0) {
            VLOG_ERR("Failed to get L3 interface statistics for interface %s", dev->linux_intf_name);
            return -1;
        }
    }

    netdev_sim_update_sflow_stats(dev);
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
    netdev_sim_dump_queue_stats,/* dump_queue_stats */

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
    netdev_sim_dump_queue_stats,/* dump_queue_stats */

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

static const struct netdev_class sim_subinterface_class = {
    "vlansubint",
    NULL,                       /* init */
    netdev_sim_run,
    NULL,                       /* wait */

    netdev_sim_alloc,
    netdev_sim_construct,
    netdev_sim_destruct,
    netdev_sim_dealloc,
    NULL,                       /* get_config */
    netdev_sim_set_config,      /* set_config */
    NULL,
    NULL,
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

static const struct netdev_class sim_loopback_class = {
    "loopback",
    NULL,                       /* init */
    netdev_sim_run,
    NULL,                       /* wait */

    netdev_sim_alloc,
    netdev_sim_construct,
    netdev_sim_destruct,
    netdev_sim_dealloc,
    NULL,                       /* get_config */
    NULL,                       /* set_config */
    NULL,
    NULL,
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
    netdev_register_provider(&sim_subinterface_class);
    netdev_register_provider(&sim_loopback_class);
}

static bool
netdev_sim_l3stats_xtables_rule_installed(const char *if_name,
                                          const char *xtables_cmd,
                                          const char *chain,
                                          const char *pkttype)
{
    char cmd[256];
    int rc = 0;
    if (!strcmp(chain, "FORWARD")) {
        snprintf(cmd, sizeof(cmd), "%s %s -C %s -i %s -m pkttype --pkt-type %s",
                SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
        rc = system(cmd);
        if (rc != 0) {
            return false;
        }
        snprintf(cmd, sizeof(cmd), "%s %s -C %s -o %s -m pkttype --pkt-type %s",
                SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
        rc = system(cmd);
        if (rc != 0) {
            return false;
        }
        return true;
    }

    char filter = !strcmp(chain, "INPUT") ? 'i' : 'o';
    snprintf(cmd, sizeof(cmd), "%s %s -C %s -%c %s -m pkttype --pkt-type %s",
            SWNS_EXEC, xtables_cmd, chain, filter, if_name, pkttype);
    rc = system(cmd);
    if (rc != 0) {
        return false;
    }
    return true;
}

static int
netdev_sim_l3stats_add_rule(const char *if_name, const char *xtables_cmd,
                            const char *chain, const char *pkttype)
{
    char cmd[256];
    int rc = 0;

    if (!strcmp(chain, "FORWARD")) {
        if (!netdev_sim_l3stats_xtables_rule_installed(if_name,
                                                       xtables_cmd, chain,
                                                       pkttype)) {
            snprintf(cmd, sizeof(cmd), "%s %s -A %s -i %s -m pkttype --pkt-type %s",
                    SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
            rc = system(cmd);
            if (rc != 0) {
                VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
            }
            snprintf(cmd, sizeof(cmd), "%s %s -A %s -o %s -m pkttype --pkt-type %s",
                    SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
            rc = system(cmd);
            if (rc != 0) {
                VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
            }
        }
        return rc;
    }

    char filter = !strcmp(chain, "INPUT") ? 'i' : 'o';

    /* Add rule if it does not exist */
    if (!netdev_sim_l3stats_xtables_rule_installed(if_name,
                                                   xtables_cmd, chain,
                                                   pkttype)) {
        snprintf(cmd, sizeof(cmd), "%s %s -A %s -%c %s -m pkttype --pkt-type %s",
                SWNS_EXEC, xtables_cmd, chain, filter, if_name, pkttype);
        rc = system(cmd);
        if (rc != 0) {
            VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
        }
    }
    return rc;
}

static int
netdev_sim_l3stats_delete_rule(const char *if_name, const char *xtables_cmd,
                               const char *chain, const char *pkttype)
{
    char cmd[256];
    int rc = 0;

    if (!strcmp(chain, "FORWARD")) {
        if (netdev_sim_l3stats_xtables_rule_installed(if_name,
                                                      xtables_cmd,
                                                      chain, pkttype)) {
            snprintf(cmd, sizeof(cmd), "%s %s -D %s -i %s -m pkttype --pkt-type %s",
                    SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
            rc = system(cmd);
            if (rc != 0) {
                VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
            }
            snprintf(cmd, sizeof(cmd), "%s %s -D %s -o %s -m pkttype --pkt-type %s",
                    SWNS_EXEC, xtables_cmd, chain, if_name, pkttype);
            rc = system(cmd);
            if (rc != 0) {
                VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
            }
        }
        return rc;
    }
    char filter = !strcmp(chain, "INPUT") ? 'i' : 'o';

    /* Delete rule if it exists */
    if (netdev_sim_l3stats_xtables_rule_installed(if_name, xtables_cmd,
                                                  chain, pkttype)) {
        snprintf(cmd, sizeof(cmd), "%s %s -D %s -%c %s -m pkttype --pkt-type %s",
                SWNS_EXEC, xtables_cmd, chain, filter, if_name, pkttype);
        rc = system(cmd);
        if (rc != 0) {
            VLOG_DBG("Failed to execute - %s (rc=%d)", cmd, rc);
        }
    }
    return rc;
}

void
netdev_sim_l3stats_xtables_rules_create(struct netdev *netdev)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);
    if (dev->l3_stats_enabled) {
        return;
    }

    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "INPUT", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "INPUT", "multicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "INPUT", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "INPUT", "multicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "OUTPUT", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "OUTPUT", "multicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "OUTPUT", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "OUTPUT", "multicast");

    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "FORWARD", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "iptables", "FORWARD", "multicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "FORWARD", "unicast");
    netdev_sim_l3stats_add_rule(dev->linux_intf_name, "ip6tables", "FORWARD", "multicast");

    dev->l3_stats_enabled = true;
}

void
netdev_sim_l3stats_xtables_rules_delete(struct netdev *netdev)
{
    struct netdev_sim *dev = netdev_sim_cast(netdev);
    if (!dev->l3_stats_enabled) {
        return;
    }

    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "INPUT", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "INPUT", "multicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "INPUT", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "INPUT", "multicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "OUTPUT", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "OUTPUT", "multicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "OUTPUT", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "OUTPUT", "multicast");

    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "FORWARD", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "iptables", "FORWARD", "multicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "FORWARD", "unicast");
    netdev_sim_l3stats_delete_rule(dev->linux_intf_name, "ip6tables", "FORWARD", "multicast");

    dev->l3_stats_enabled = false;
}

static void
netdev_parse_netlink_msg(struct nlmsghdr *h, struct netdev_stats *stats)
{
    struct ifinfomsg *iface;
    struct rtattr *attribute;
    struct rtnl_link_stats *s;
    int len;

    iface = NLMSG_DATA(h);
    len = h->nlmsg_len - NLMSG_LENGTH(sizeof(*iface));
    for (attribute = IFLA_RTA(iface); RTA_OK(attribute, len);
         attribute = RTA_NEXT(attribute, len)) {
        switch(attribute->rta_type) {
        case IFLA_STATS:
            s = (struct rtnl_link_stats *) RTA_DATA(attribute);
            stats->rx_packets = s->rx_packets;
            stats->tx_packets = s->tx_packets;
            stats->rx_bytes = s->rx_bytes;
            stats->tx_bytes = s->tx_bytes;
            stats->rx_errors = s->rx_errors;
            stats->tx_errors = s->tx_errors;
            stats->rx_dropped = s->rx_dropped;
            stats->tx_dropped = s->tx_dropped;
            stats->multicast = s->multicast;
            stats->collisions = s->collisions;
            stats->rx_crc_errors = s->rx_crc_errors;
            break;
        default:
            break;
        }
    }
}

static int
netdev_get_kernel_stats(const char *if_name, struct netdev_stats *stats)
{
    int sock;
    struct sockaddr_nl s_addr;
    struct {
        struct nlmsghdr hdr;
        struct ifinfomsg iface;
    } req;
    struct sockaddr_nl kernel;
    struct msghdr rtnl_msg;
    struct iovec io;

    memset (&req, 0, sizeof(req));
    memset (&kernel, 0, sizeof(kernel));
    memset (&rtnl_msg, 0, sizeof(rtnl_msg));
    kernel.nl_family = AF_NETLINK;

    req.hdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.hdr.nlmsg_flags = NLM_F_REQUEST;
    req.hdr.nlmsg_type = RTM_GETLINK;

    req.iface.ifi_family = AF_UNSPEC;
    req.iface.ifi_type = IFLA_UNSPEC;
    req.iface.ifi_flags = 0;
    req.iface.ifi_change = 0xffffffff;
    req.iface.ifi_index = if_nametoindex(if_name);

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);

    if (sock < 0) {
        VLOG_ERR("Netlink socket creation failed during netlink \
                request for statistics (%s)",strerror(errno));
        return -1;
    }

    memset((void *) &s_addr, 0, sizeof(s_addr));
    s_addr.nl_family = AF_NETLINK;
    s_addr.nl_pid = getpid();
    s_addr.nl_groups = 0;

    if (bind(sock, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        VLOG_ERR("Socket bind failed during netlink \
                request for statistics");
        close(sock);
        return -1;
    }

    io.iov_base = &req;
    io.iov_len = req.hdr.nlmsg_len;
    rtnl_msg.msg_name = &kernel;
    rtnl_msg.msg_namelen = sizeof(kernel);
    rtnl_msg.msg_iov = &io;
    rtnl_msg.msg_iovlen = 1;

    if (sendmsg(sock, (struct msghdr *) &rtnl_msg, 0) == -1) {
        VLOG_ERR("Sendmsg failed during netlink \
                request for statistics");
        close(sock);
        return -1;
    }

    /* Prepare for reply from the kernel */
    bool multipart_msg_end = false;

    while (!multipart_msg_end) {
        struct sockaddr_nl nladdr;
        struct msghdr msg;
        struct iovec iov;
        struct nlmsghdr *nlh;
        char buffer[4096];
        int ret;

        iov.iov_base = (void *)buffer;
        iov.iov_len = sizeof(buffer);
        msg.msg_name = (void *)&(nladdr);
        msg.msg_namelen = sizeof(nladdr);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;

        ret = recvmsg(sock, &msg, 0);

        if (ret < 0) {
            VLOG_ERR("Reply error during netlink \
                     request for statistics\n");
            close(sock);
            return -1;
        }

        nlh = (struct nlmsghdr*) buffer;

        for (nlh = (struct nlmsghdr *) buffer;
             NLMSG_OK(nlh, ret);
             nlh = NLMSG_NEXT(nlh, ret)) {
            switch(nlh->nlmsg_type) {
            case RTM_NEWLINK:
                netdev_parse_netlink_msg(nlh, stats);
                break;

            case NLMSG_DONE:
                multipart_msg_end = true;
                break;

            default:
                break;
            }

            if (!(nlh->nlmsg_flags & NLM_F_MULTI)) {
                multipart_msg_end = true;
            }
        }
    }
    close(sock);

    return 0;
}

static int
netdev_sim_parse_iptables_stats_line(FILE *fp, const char *if_name,
                                     uint64_t *packets, uint64_t *bytes)
{
    char stats_str[64];
    *packets = 0;
    *bytes = 0;
    if (fgets(stats_str, sizeof(stats_str)-1, fp)) {
        if (sscanf(stats_str, "%"PRIu64"%"PRIu64, packets, bytes) < 2) {
            VLOG_DBG("Failed to parse from iptables command for L3 interface %s", if_name);
            *packets = 0;
            *bytes = 0;
            return -1;
        }
    }
    return 0;
}

static int
netdev_sim_parse_xtables_l3_stats(const char *if_name,
                                  struct kernel_l3_stats *kernel_stats,
                                  bool is_v6,
                                  bool is_ingress)
{
    FILE *fp;
    char cmd[256];
    uint64_t packets = 0;
    uint64_t bytes = 0;

    if (is_ingress) {
        /* Get L3 RX stats */
        snprintf(cmd, sizeof(cmd), "%s %s -S -v | awk '$3 == \"-i\" && $4 == \"%s\" && $6 == \"pkttype\"{print $10,$11}'",
                 SWNS_EXEC, is_v6 ? "ip6tables" : "iptables" , if_name);
    } else {
        /* Get L3 TX stats */
        snprintf(cmd, sizeof(cmd), "%s %s -S -v | awk '$3 == \"-o\" && $4 == \"%s\" && $6 == \"pkttype\"{print $10,$11}'",
                 SWNS_EXEC, is_v6 ? "ip6tables" : "iptables" , if_name);
    }

    /* Open the command for reading. */
    fp = popen(cmd, "r");
    if (fp == NULL) {
        VLOG_DBG("Failed to open pipe for command %s", cmd);
        return -1;
    }

    /* Read the output lines and parse the statistics */
    netdev_sim_parse_iptables_stats_line(fp, if_name, &packets, &bytes);
    kernel_stats->uc_packets = packets;
    kernel_stats->uc_bytes = bytes;

    netdev_sim_parse_iptables_stats_line(fp, if_name, &packets, &bytes);
    kernel_stats->mc_packets = packets;
    kernel_stats->mc_bytes = bytes;

    /* Adding packets and bytes for forwarded traffic */
    netdev_sim_parse_iptables_stats_line(fp, if_name, &packets, &bytes);
    kernel_stats->uc_packets += packets;
    kernel_stats->uc_bytes += bytes;

    netdev_sim_parse_iptables_stats_line(fp, if_name, &packets, &bytes);
    kernel_stats->mc_packets += packets;
    kernel_stats->mc_bytes += bytes;

    pclose(fp);
    return 0;
}

static int
netdev_sim_get_kernel_l3_stats(const char *if_name, struct netdev_stats *stats)
{
    int rc = 0;
    struct kernel_l3_stats kernel_stats;

    memset(&kernel_stats, 0, sizeof(kernel_stats));

    /* IPV4 stats */
    rc = netdev_sim_parse_xtables_l3_stats(if_name, &kernel_stats, false, true);
    if (rc < 0) {
        return rc;
    }
    stats->ipv4_uc_rx_packets = kernel_stats.uc_packets;
    stats->ipv4_mc_rx_packets = kernel_stats.mc_packets;
    stats->ipv4_uc_rx_bytes = kernel_stats.uc_bytes;
    stats->ipv4_mc_rx_bytes = kernel_stats.mc_bytes;


    rc = netdev_sim_parse_xtables_l3_stats(if_name, &kernel_stats, false, false);
    if (rc < 0) {
        return rc;
    }
    stats->ipv4_uc_tx_packets = kernel_stats.uc_packets;
    stats->ipv4_mc_tx_packets = kernel_stats.mc_packets;
    stats->ipv4_uc_tx_bytes = kernel_stats.uc_bytes;
    stats->ipv4_mc_tx_bytes = kernel_stats.mc_bytes;


    /* IPV6 stats */

    rc = netdev_sim_parse_xtables_l3_stats(if_name, &kernel_stats, true, true);
    if (rc < 0) {
        return rc;
    }
    stats->ipv6_uc_rx_packets = kernel_stats.uc_packets;
    stats->ipv6_mc_rx_packets = kernel_stats.mc_packets;
    stats->ipv6_uc_rx_bytes = kernel_stats.uc_bytes;
    stats->ipv6_mc_rx_bytes = kernel_stats.mc_bytes;


    rc = netdev_sim_parse_xtables_l3_stats(if_name, &kernel_stats, true, false);
    if (rc < 0) {
        return rc;
    }
    stats->ipv6_uc_tx_packets = kernel_stats.uc_packets;
    stats->ipv6_mc_tx_packets = kernel_stats.mc_packets;
    stats->ipv6_uc_tx_bytes = kernel_stats.uc_bytes;
    stats->ipv6_mc_tx_bytes = kernel_stats.mc_bytes;


    /* Global L3 stats */
    stats->l3_uc_tx_packets = stats->ipv4_uc_tx_packets + stats->ipv6_uc_tx_packets;
    stats->l3_mc_tx_packets = stats->ipv4_mc_tx_packets + stats->ipv6_mc_tx_packets;
    stats->l3_uc_tx_bytes = stats->ipv4_uc_tx_bytes + stats->ipv6_uc_tx_bytes;
    stats->l3_mc_tx_bytes = stats->ipv4_mc_tx_bytes + stats->ipv6_mc_tx_bytes;

    stats->l3_uc_rx_packets = stats->ipv4_uc_rx_packets + stats->ipv6_uc_rx_packets;
    stats->l3_mc_rx_packets = stats->ipv4_mc_rx_packets + stats->ipv6_mc_rx_packets;
    stats->l3_uc_rx_bytes = stats->ipv4_uc_rx_bytes + stats->ipv6_uc_rx_bytes;
    stats->l3_mc_rx_bytes = stats->ipv4_mc_rx_bytes + stats->ipv6_mc_rx_bytes;

    return rc;
}

/* Update iptable reconfiguration events so that netdev sflow statistics
 * can take care of the event. This has to be done because sflow
 * iptable rules clear the counters when sflow gets disabled, but
 * the db counters has to keep incrementing, so we need a way to
 * know when to add iptable counters to db and when to do a diff
 * before adding to the db.
 */
void
netdev_sflow_reset(struct netdev *netdev_)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    ovs_mutex_lock(&netdev->mutex);
    netdev->sflow_resets++;
    ovs_mutex_unlock(&netdev->mutex);
}

void
netdev_sflow_stats_enable(struct netdev *netdev_, bool enabled)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    ovs_mutex_lock(&netdev->mutex);
    netdev->sflow_stats_enabled = enabled;
    ovs_mutex_unlock(&netdev->mutex);
}
