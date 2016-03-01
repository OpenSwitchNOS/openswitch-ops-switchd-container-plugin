/*
 *  (c) Copyright 2015 Hewlett Packard Enterprise Development LP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License. You may obtain
 *  a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */

#include <unistd.h>
#include "openvswitch/vlog.h"
#include "netdev-provider.h"
#include "ofproto/ofproto-provider.h"
#include "netdev-sim.h"
#include "ofproto-sim-provider.h"
#include "qos.h"
#include "plugin-extensions.h"
#include "asic-plugin.h"

#define init libovs_sim_plugin_LTX_init
#define run libovs_sim_plugin_LTX_run
#define wait libovs_sim_plugin_LTX_wait
#define destroy libovs_sim_plugin_LTX_destroy
#define netdev_register libovs_sim_plugin_LTX_netdev_register
#define ofproto_register libovs_sim_plugin_LTX_ofproto_register

#define MAX_CMD_LEN             50

VLOG_DEFINE_THIS_MODULE(sim_plugin);

struct asic_plugin_interface container_interface ={
    /* The new functions that need to be exported, can be declared here*/
    .set_port_qos_cfg = &set_port_qos_cfg,
    .set_cos_map = &set_cos_map,
    .set_dscp_map = &set_dscp_map,
};

void
init(void)
{
    struct plugin_extension_interface container_extension;
    container_extension.plugin_name = ASIC_PLUGIN_INTERFACE_NAME;
    container_extension.major = ASIC_PLUGIN_INTERFACE_MAJOR;
    container_extension.minor = ASIC_PLUGIN_INTERFACE_MINOR;
    container_extension.plugin_interface = (void *)&container_interface;

    register_plugin_extension(&container_extension);
    VLOG_INFO("The %s asic plugin interface was registered", ASIC_PLUGIN_INTERFACE_NAME);

    char cmd_str[MAX_CMD_LEN];
    memset(cmd_str, 0, sizeof(cmd_str));
    /* Cleaning up the Internal "ASIC" OVS everytime ops-switchd daemon is
    * started or restarted or killed to keep the "ASIC" OVS database in sync
    * with the OpenSwitch OVS database.
    * Here, we initially stop the "ASIC" ovs-vswitchd-sim and ovsdb-server
    * daemons, then delete the "ASIC" database file and start the
    * ovsdb-server again which recreates the "ASIC" database file and finally
    * start the ovs-vswitchd-sim daemon before ops-switchd daemon gets
    * restarted.*/
    if (system("systemctl stop openvswitch-sim") != 0) {
        VLOG_ERR("Failed to stop Internal 'ASIC' OVS openvswitch.service");
    }

    if (system("systemctl stop ovsdb-server-sim") != 0) {
        VLOG_ERR("Failed to stop Internal 'ASIC' OVS ovsdb-server-sim.service");
    }

    if (access(ASIC_OVSDB_PATH, F_OK) != -1) {
        snprintf(cmd_str, MAX_CMD_LEN, "sudo rm -rf %s", ASIC_OVSDB_PATH);
        if (system(cmd_str) != 0) {
            VLOG_ERR("Failed to delete Internal 'ASIC' OVS ovsdb.db file");
        }
    } else {
        VLOG_DBG("Internal 'ASIC' OVS ovsdb.db file does not exist");
    }

    if (system("systemctl start ovsdb-server-sim") != 0) {
        VLOG_ERR("Failed to start Internal 'ASIC' OVS ovsdb-server-sim.service");
    }

    if (system("systemctl start openvswitch-sim") != 0) {
        VLOG_ERR("Failed to start Internal 'ASIC' OVS openvswitch.service");
    }
}

void
run(void)
{
}

void
wait(void)
{
}

void
destroy(void)
{
}

void
netdev_register(void)
{
    netdev_sim_register();
}

void
ofproto_register(void)
{
    ofproto_class_register(&ofproto_sim_provider_class);
}
