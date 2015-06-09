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


#include <errno.h>

#include "dpif-netdev.h"
#include "dynamic-string.h"
#include "flow.h"
#include "list.h"
#include "netdev-provider.h"
#include "netdev-vport.h"
#include "ofp-print.h"
#include "ofpbuf.h"
#include "packet-dpif.h"
#include "packets.h"
#include "pcap-file.h"
#include "poll-loop.h"
#include "shash.h"
#include "sset.h"
#include "stream.h"
#include "unaligned.h"
#include "timeval.h"
#include "unixctl.h"
#include "openvswitch/vlog.h"
#include <stdlib.h>

#include "netdev-sim.h"

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
    int mtu OVS_GUARDED;
    struct netdev_stats stats OVS_GUARDED;
    enum netdev_flags flags OVS_GUARDED;
    int hw_id;
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

int netdev_sim_get_hw_id(struct netdev *netdev)
{
    struct netdev_sim *nb = netdev_sim_cast(netdev);
    ovs_assert(is_sim_class(netdev_get_class(netdev)));

    return nb->hw_id;
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

static int
netdev_sim_set_config(struct netdev *netdev_, const struct smap *args)
{
    int i;
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    const char *hw_enable = smap_get(args, "hw_enable");
    const char *hw_id = smap_get(args, "hw_id");

    /* Setup physcial ports for simulation */
    const char *devname = netdev_get_name(netdev);
    char system_cmd[1024];

    VLOG_INFO("Setting up physical interfaces, %s", devname);

    /* If eXYZ iinterface doesn't exist, rename ethXYZ to eXYZ */
    sprintf(system_cmd, "ip link show %s > /dev/null 2>&1", devname);
    if (system(system_cmd) != 0) {
        /* i will point to start of numeric part of the interface */
        for (i=0; i < strlen(devname); i++) {
            if (devname[i] >= '0' && devname[i] <= '9') {
                break;
            }
        }
        sprintf(system_cmd, "ip link set dev eth%s down", &devname[i]);
        VLOG_INFO("ip link set dev eth down --%s ",  system_cmd);
        if (system(system_cmd) != 0) {
            VLOG_INFO("ERROR: NETDEV-SIM | system command execution failure");
        }
        sprintf(system_cmd, "ip link set dev eth%s name %s", &devname[i], devname);
        VLOG_INFO("ip link set dev eth name --%s ", system_cmd);
        if (system(system_cmd) != 0) {
            VLOG_INFO("ERROR: NETDEV-SIM | system command execution failure");
        }
        sprintf(system_cmd, "ip link set dev %s up", devname);
        VLOG_INFO("Rename interface cli - %s", system_cmd);
        if (system(system_cmd) != 0) {
            VLOG_INFO("ERROR: NETDEV-SIM | system command execution failure");
        }
    }


    ovs_mutex_lock(&netdev->mutex);

    if (hw_enable) {
        if (!strcmp(hw_enable, "true")) {
            sprintf(system_cmd, "ip link set dev %s up", devname);
        } else {
            sprintf(system_cmd, "ip link set dev %s down", devname);
        }
        if (system(system_cmd) != 0) {
            VLOG_INFO("ERROR: NETDEV-SIM | system command failure");
        }
    }

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
netdev_sim_update_flags__(struct netdev_sim *netdev,
                            enum netdev_flags off, enum netdev_flags on,
                            enum netdev_flags *old_flagsp)
    OVS_REQUIRES(netdev->mutex)
{
    if ((off | on) & ~(NETDEV_UP | NETDEV_PROMISC)) {
        return EINVAL;
    }

    *old_flagsp = netdev->flags;
    netdev->flags |= on;
    netdev->flags &= ~off;
    if (*old_flagsp != netdev->flags) {
        netdev_change_seq_changed(&netdev->up);
    }

    return 0;
}

static int
netdev_sim_update_flags(struct netdev *netdev_,
                          enum netdev_flags off, enum netdev_flags on,
                          enum netdev_flags *old_flagsp)
{
    struct netdev_sim *netdev = netdev_sim_cast(netdev_);
    int error;

    ovs_mutex_lock(&netdev->mutex);
    error = netdev_sim_update_flags__(netdev, off, on, old_flagsp);
    ovs_mutex_unlock(&netdev->mutex);

    return error;
}

/* Helper functions. */

static const struct netdev_class sim_class = {
    "system",
    NULL,                       /* init */
    NULL,                       /* run */
    NULL,                       /* wait */

    netdev_sim_alloc,
    netdev_sim_construct,
    netdev_sim_destruct,
    netdev_sim_dealloc,
    NULL,                       /* get_config */
    netdev_sim_set_config,
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
    NULL,                       /* get_carrier */
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
