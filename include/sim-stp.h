/* Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
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

#ifndef SIM_STP_H
#define SIM_STP_H 1

#include <netinet/in.h>
#include "hmap.h"
#include "vswitch-idl.h"
#include "dynamic-string.h"
#include "reconfigure-blocks.h"

#define SWITCHD_STP_PLUGIN_NAME "STP"
#define MSTP_CIST 0
#define MSTP_DEFAULT_STG_GROUP 1
#define MSTP_INST_MIN 1
#define MSTP_INST_MAX 64
#define MSTP_INST_VALID(v)  ((v)>=MSTP_INST_MIN && (v)<=MSTP_INST_MAX)
#define MSTP_CIST_INST_VALID(v)  ((v)>=MSTP_CIST && (v)<=MSTP_INST_MAX)
#define MSTP_STR_EQ(s1, s2) ((strlen((s1)) == strlen((s2))) && (!strncmp((s1), (s2), strlen((s2)))))

struct stp_blk_params{
    struct ovsdb_idl *idl;   /* OVSDB IDL handler */
    const struct ovsrec_bridge *cfg;
};

union mstp_cfg {
        const struct ovsrec_mstp_instance *msti_cfg;
        const struct ovsrec_mstp_common_instance *cist_cfg;
};

union mstp_port_cfg {
        const struct ovsrec_mstp_instance_port *msti_port_cfg;
        const struct ovsrec_mstp_common_instance_port *cist_port_cfg;
};


typedef enum mstp_instance_port_state {
    MSTP_INST_PORT_STATE_DISABLED = 0,
    MSTP_INST_PORT_STATE_LISTENING,
    MSTP_INST_PORT_STATE_LEARNING,
    MSTP_INST_PORT_STATE_FORWARDING,
    MSTP_INST_PORT_STATE_BLOCKED,
    MSTP_INST_PORT_STATE_INVALID,
}mstp_instance_port_state_t;

struct mstp_instance_port {
    struct hmap_node hmap_node; /* Element in struct mstp_instance's "ports" hmap. */
    char *name;
    int stp_state;
    union  mstp_port_cfg cfg;
};

struct mstp_instance_vlan {
    struct hmap_node hmap_node;  /* In struct mstp_instance's "vlans" hmap. */
    char *name;
    int vid;
};

struct mstp_instance {
    struct hmap_node node;
    int instance_id;
    struct hmap vlans;
    int nb_vlans;
    struct hmap ports;
    int nb_ports;
    union  mstp_cfg cfg;
    int hw_stg_id;
};

void register_stp_plugins(void);

void mstp_cist_and_instance_set_port_state(const struct stp_blk_params *br,
                                                 struct mstp_instance *msti,
                                         struct mstp_instance_port *mstp_port);
void mstp_cist_and_instance_port_delete(const struct stp_blk_params *br,
                                              struct mstp_instance *msti,
                                              struct mstp_instance_port *port);
void mstp_instance_port_add(const struct stp_blk_params *br,
                                 struct mstp_instance *msti,
                        const struct ovsrec_mstp_instance_port *inst_port_cfg);

void mstp_instance_add_del_ports(const struct stp_blk_params *br,
                                       struct mstp_instance *msti);
void mstp_cist_port_add(const struct stp_blk_params *br,
                            struct mstp_instance *msti,
                 const struct ovsrec_mstp_common_instance_port *cist_port_cfg);
void mstp_cist_configure_ports(const struct stp_blk_params *br,
                                 struct mstp_instance *msti);
void mstp_cist_create(const struct stp_blk_params *br,
                      const struct ovsrec_mstp_common_instance *msti_cist_cfg);
void mstp_cist_update(const struct stp_blk_params *br_blk_params);
void mstp_update_instances(struct stp_blk_params *br_blk_params);
void stp_reconfigure(struct blk_params*);
void stp_plugin_dump_data(struct ds *ds, int argc, const char *argv[]);

#endif /* sim-stp.h */
