/*
 *    Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
 *    All Rights Reserved.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License"); you may
 *    not use this file except in compliance with the License. You may obtain
 *    a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *    License for the specific language governing permissions and limitations
 *    under the License.
 */

#include "openvswitch/vlog.h"
#include "netdev-provider.h"
#include "ofproto/ofproto-provider.h"
#include "netdev-sim.h"
#include "ofproto-sim-provider.h"

#define init libovs_sim_plugin_LTX_init
#define run libovs_sim_plugin_LTX_run
#define wait libovs_sim_plugin_LTX_wait
#define destroy libovs_sim_plugin_LTX_destroy
#define netdev_register libovs_sim_plugin_LTX_netdev_register
#define ofproto_register libovs_sim_plugin_LTX_ofproto_register


VLOG_DEFINE_THIS_MODULE(sim_plugin);

void
init(void){
    /* reset the "ASIC" */
    system("rm /run/openvswitch-sim/ovsdb.db && \
            kill -9 `cat /run/openvswitch-sim/ovsdb-server.pid` && \
            ip netns exec swns iptables -F && sleep 3");
}

void
run(void){
}

void
wait(void){
}

void
destroy(void){
}

void
netdev_register(void){
    netdev_sim_register();
}

void
ofproto_register(void){
    ofproto_class_register(&ofproto_sim_provider_class);
}
