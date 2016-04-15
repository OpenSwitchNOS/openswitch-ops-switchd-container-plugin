
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

#ifndef OFPROTO_SIM_PROVIDER_H
#define OFPROTO_SIM_PROVIDER_H 1

#include "ofproto/ofproto-provider.h"
#include "hmapx.h"

#define MAX_CLI                 1024
#define OVS_VSCTL               "/opt/openvswitch/bin/ovs-vsctl"
#define ASIC_OVSDB_PATH         "/var/run/openvswitch-sim/ovsdb.db"
#define APPCTL                  "/opt/openvswitch/bin/ovs-appctl"
#define OVS_SIM                 "ovs-vswitchd-sim"

#define MAX_MIRRORS 32
#define MAX_MIRROR_NAME_LEN 64

typedef uint32_t mirror_mask_t;

struct mbundle {
    struct hmap_node hmap_node; /* In parent 'mbridge' map. */
    struct ofbundle *ofbundle;

    mirror_mask_t src_mirrors;  /* Mirrors triggered when packet received. */
    mirror_mask_t dst_mirrors;  /* Mirrors triggered when packet sent. */
    mirror_mask_t mirror_out;   /* Mirrors that output to this mbundle. */
};

struct mirror {
    struct mbridge *mbridge;    /* Owning ofproto. */
    size_t idx;                 /* In ofproto's "mirrors" array. */
    void *aux;                  /* Key supplied by ofproto's client. */

    /* Selection criteria. */
    struct hmapx srcs;          /* Contains "struct mbundle*"s. */
    struct hmapx dsts;          /* Contains "struct mbundle*"s. */
    unsigned long *vlans;       /* Bitmap of chosen VLANs, NULL selects all. */

    /* Output (exactly one of out == NULL and out_vlan == -1 is true). */
    struct mbundle *out;        /* Output port or NULL. */
    int out_vlan;               /* Output VLAN or -1. */
    mirror_mask_t dup_mirrors;  /* Bitmap of mirrors with the same output. */

    /* Counters. */
    int64_t packet_count;       /* Number of packets sent. */
    int64_t byte_count;         /* Number of bytes sent. */

    char *name; /* Mirror name for logging */
};

struct mbridge {
    struct mirror *mirrors[MAX_MIRRORS];
    struct hmap mbundles;

    bool need_revalidate;
    bool has_mirrors;

    struct ovs_refcount ref_cnt;
};

struct sim_provider_rule {
    struct rule up;
    struct ovs_mutex stats_mutex;
    uint32_t recirc_id;
};

struct sim_provider_group {
    struct ofgroup up;
    struct ovs_mutex stats_mutex;
    uint64_t packet_count OVS_GUARDED;  /* Number of packets received. */
    uint64_t byte_count OVS_GUARDED;    /* Number of bytes received. */
};

struct ofbundle {
    struct hmap_node hmap_node; /* In struct ofproto's "bundles" hmap. */
    struct sim_provider_node *ofproto;  /* Owning ofproto. */

    void *aux;                  /* Key supplied by ofproto's client. */
    char *name;                 /* Identifier for log messages. */

    /* Configuration. */
    struct ovs_list ports;      /* Contains "struct ofport"s. */

    enum port_vlan_mode vlan_mode;      /* VLAN mode */
    int vlan;                   /* -1=trunk port, else a 12-bit VLAN ID. */
    unsigned long *trunks;      /* Bitmap of trunked VLANs, if 'vlan' == -1.
                                 * NULL if all VLANs are trunked. */
    struct lacp *lacp;          /* LACP if LACP is enabled, otherwise NULL. */
    struct bond *bond;          /* Nonnull iff more than one port. */
    bool use_priority_tags;     /* Use 802.1p tag for frames in VLAN 0? */

    /* Status. */
    bool floodable;             /* True if no port has OFPUTIL_PC_NO_FLOOD
                                 * set. */

    bool is_added_to_sim_ovs;   /* If this bundle is added to ASIC simulating
                                 * OVS. */

    bool is_vlan_routing_enabled;       /* If VLAN routing is enabled on this
                                         * bundle. */
    bool is_bridge_bundle;      /* If the bundle is internal for the bridge. */
    bool is_sflow_enabled;      /* If slow is enabled for this bundle */
};

struct sim_provider_ofport {
    struct hmap_node odp_port_node;
    struct ofport up;

    odp_port_t odp_port;
    struct ofbundle *bundle;    /* Bundle that contains this port, if any. */
    struct ovs_list bundle_node;        /* In struct ofbundle's "ports" list. */
    struct cfm *cfm;            /* Connectivity Fault Management, if any. */
    struct bfd *bfd;            /* BFD, if any. */
    bool may_enable;            /* May be enabled in bonds. */
    bool is_tunnel;             /* This port is a tunnel. */
    bool is_layer3;             /* This is a layer 3 port. */
    long long int carrier_seq;  /* Carrier status changes. */
    struct sim_provider_ofport_node *peer;      /* Peer if patch port. */

    /* Spanning tree. */
    struct stp_port *stp_port;  /* Spanning Tree Protocol, if any. */
    enum stp_state stp_state;   /* Always STP_DISABLED if STP not in use. */
    long long int stp_state_entered;

    /* Rapid Spanning Tree. */
    struct rstp_port *rstp_port;        /* Rapid Spanning Tree Protocol, if
                                         * any. */
    enum rstp_state rstp_state; /* Always RSTP_DISABLED if RSTP not in use. */

    /* Queue to DSCP mapping. */
    struct ofproto_port_queue *qdscp;
    size_t n_qdscp;

    /* Linux VLAN device support (e.g. "eth0.10" for VLAN 10.) This is
     * deprecated.  It is only for compatibility with broken device */
    ofp_port_t realdev_ofp_port;
    int vlandev_vid;

    bool iptable_rules_added;   /* If IP table rules added to drop L2 traffic.
                                 */
};

struct sim_sflow_cfg {
    struct sset ports;         /* port names where sflow configuration is
                                  applied (for VRF) */
    struct sset targets;       /* sFlow Collectors information */
    uint32_t sampling_rate;    /* Rate at which packets are sampled */
    uint32_t polling_interval; /* Time interval for sending interface stats */
    uint32_t header_len;       /* Number of header bytes included
                                  from the sampled packets */
    uint32_t max_datagram;     /* Maximum size of sFlow datagram */
    char *agent_device;        /* Agent Interface (IP address that is used
                                  in sFlow datagram) */
    bool set;
};

struct sim_provider_node {
    struct hmap_node all_sim_provider_node;     /* In 'all_ofproto_provider'. */
    struct ofproto up;

    uint64_t dump_seq;          /* Last read of dump_seq(). */

    /* Special OpenFlow rules. */
    struct sim_provider_rule *miss_rule;        /* Sends flow table misses to
                                                 * controller. */
    struct sim_provider_rule *no_packet_in_rule;        /* Drops flow table
                                                         * misses. */
    struct sim_provider_rule *drop_frags_rule;  /* Used in OFPC_FRAG_DROP
                                                 * mode. */

    /* Bridging. */
    struct netflow *netflow;
    struct hmap bundles;        /* Contains "struct ofbundle"s. */
    struct mac_learning *ml;
    struct mcast_snooping *ms;
    bool has_bonded_bundles;
    bool lacp_enabled;
    struct mbridge *mbridge;

    struct ovs_mutex stats_mutex;
    struct netdev_stats stats OVS_GUARDED;      /* To account packets
                                                 * generated and consumed in
                                                 * userspace. */

    /* Spanning tree. */
    struct stp *stp;
    long long int stp_last_tick;

    /* Rapid Spanning Tree. */
    struct rstp *rstp;
    long long int rstp_last_tick;

    /* VLAN splinters. */
    struct ovs_mutex vsp_mutex;
    struct hmap realdev_vid_map OVS_GUARDED;    /* (realdev,vid) -> vlandev. */
    struct hmap vlandev_map OVS_GUARDED;        /* vlandev -> (realdev,vid). */

    /* Ports. */
    struct sset ports;          /* Set of standard port names. */
    struct sset ghost_ports;    /* Ports with no datapath port. */
    struct sset port_poll_set;  /* Queued names for port_poll() reply. */
    int port_poll_errno;        /* Last errno for port_poll() reply. */
    uint64_t change_seq;        /* Connectivity status changes. */

    /* Work queues. */
    struct guarded_list pins;   /* Contains "struct ofputil_packet_in"s. */
    struct seq *pins_seq;       /* For notifying 'pins' reception. */
    uint64_t pins_seqno;
    unsigned long *vlans_bmp;   /* 4096-bit bitmap of in-use VLANs. */
    unsigned long *vlan_intf_bmp;       /* 4096 bitmap of vlan interfaces */

    bool vrf;                   /* Specifies whether specific ofproto instance
                                 * is backing up VRF and not bridge */
    struct sim_sflow_cfg sflow; /* sflow configuration */
};

struct sim_provider_port_dump_state {
    uint32_t bucket;
    uint32_t offset;
    bool ghost;

    struct ofproto_port port;
    bool has_port;
};

enum { N_TABLES = 255 };
enum { TBL_INTERNAL = N_TABLES - 1 };   /* Used for internal hidden rules. */

static void rule_get_stats(struct rule *, uint64_t * packets,
                           uint64_t * bytes, long long int *used);
static void bundle_remove(struct ofport *);
static struct sim_provider_ofport *get_ofp_port(const struct sim_provider_node
                                                *ofproto, ofp_port_t ofp_port);

static struct mirror *mirror_lookup(struct mbridge *, void *aux);
static struct mbundle *mbundle_lookup(const struct mbridge *,
                                      struct ofbundle *);
static void mbundle_lookup_multiple(const struct mbridge *, struct ofbundle **,
                                  size_t n_bundles, struct hmapx *mbundles);
static int mirror_scan(struct mbridge *);
static void mirror_destroy(struct mbridge *mbridge, void *aux);

extern const struct ofproto_class ofproto_sim_provider_class;
#endif /* ofproto/ofproto-sim-provider.h */
