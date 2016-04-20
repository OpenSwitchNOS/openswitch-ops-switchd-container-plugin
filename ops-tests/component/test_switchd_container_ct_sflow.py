# -*- coding: utf-8 -*-
#
# Copyright (C) 2015-2016 Hewlett Packard Enterprise Development LP
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

"""
Component Test to Verify sFlow Configuration in container plugin (VSI).
"""

import time
from pytest import mark


TOPOLOGY = """
#
# +-------+
# |  sw1  |
# +-------+
#

# Nodes
[type=openswitch name="OpenSwitch"] ops1
"""


@mark.platform_incompatible(['ostl'])
def test_container_ct_sflow(topology, step):
    ops1 = topology.get('ops1')
    assert ops1 is not None

    # sflow configuration values
    sampling_rate = 100
    polling_interval = 10
    agent_interface = '1'
    collector_ip = '10.10.10.2'

    # Configure interfaces on the switch
    step("Configuring interface 1 of switch")
    with ops1.libs.vtysh.ConfigInterface('1') as ctx:
        ctx.ip_address('10.10.10.1/24')
        ctx.no_shutdown()

    # Configuring sflow globally
    step("### Configuring sFlow ###")
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.sflow_enable()
        ctx.sflow_sampling(sampling_rate)
        ctx.sflow_agent_interface(agent_interface)
        ctx.sflow_collector(collector_ip)
        ctx.sflow_polling(polling_interval)

    collector = {}
    collector['ip'] = collector_ip
    collector['port'] = '6343'
    collector['vrf'] = 'vrf_default'

    # Comparing values stored in DB with expected values
    sflow_config = ops1.libs.vtysh.show_sflow()
    assert sflow_config['sflow'] == 'enabled'
    assert sflow_config['sampling_rate'] == sampling_rate
    assert sflow_config['collector'][0] == collector
    assert sflow_config['agent_interface'] == agent_interface
    assert sflow_config['polling_interval'] == polling_interval

    # Creating a dict of expected sFlow configuration
    expected_cfg = {}
    expected_cfg['sampling'] = sampling_rate
    expected_cfg['polling'] = polling_interval
    expected_cfg['collector'] = collector_ip
    expected_cfg['agent'] = agent_interface

    step("### Verifying sFlow configuration in Sim OVS ###")
    # Wait until sFlow row is created in SIM ovsdb
    uuid_found = False
    while not uuid_found:
        uuid_sflow = ops1("/opt/openvswitch/bin/ovs-vsctl get bridge "
                          "bridge_normal sflow", shell="bash")
        if uuid_sflow != '[]':
            uuid_found = True
        else:
            time.sleep(1)

    sflow_sim_cfg = ops1("/opt/openvswitch/bin/ovs-vsctl list sFlow",
                         shell="bash").splitlines()
    # Parsing the sFlow table output into key,value pairs
    sflow_dict = {}
    for cfg in sflow_sim_cfg:
        cfg = cfg.split(":")
        sflow_dict[cfg[0].strip()] = cfg[1].strip()

    # Create a dict from Sim OVS output
    sim_ovs_cfg = {}
    sim_ovs_cfg['sampling'] = int(sflow_dict['sampling'])
    sim_ovs_cfg['agent'] = sflow_dict['agent'].replace('"', '')
    sim_ovs_cfg['polling'] = int(sflow_dict['polling'])
    sim_ovs_cfg['collector'] = sflow_dict['targets'][2:-2]

    # Check Sim OVS sFlow configuration
    step("Expected sFlow configuration: " + str(expected_cfg))
    step("Sim OVS sFlow configuration: " + str(sim_ovs_cfg))
    assert expected_cfg == sim_ovs_cfg

    step("### Verifying sFlow configuration set in hsflowd.conf file ###")
    # Wait until hsflowd has started
    hsflowd_started = False
    while not hsflowd_started:
        ps_cmd = ops1("ps -aef | grep hsflowd", shell="bash").splitlines()
        if len(ps_cmd) > 1:
            hsflowd_started = True
        else:
            time.sleep(1)
    hsflowd_conf = ops1("cat /etc/hsflowd.conf", shell="bash").splitlines()
    # Create a dict from hsflowd.conf file
    hsflowd_cfg = {}
    for cfg in hsflowd_conf:
        if 'agent' in cfg:
            cfg = cfg.split("=")
            hsflowd_cfg['agent'] = cfg[1].strip()
        if 'polling' in cfg:
            cfg = cfg.split("=")
            hsflowd_cfg['polling'] = int(cfg[1].strip())
        if 'sampling' in cfg:
            cfg = cfg.split("=")
            hsflowd_cfg['sampling'] = int(cfg[1].strip())
        if 'ip' in cfg:
            cfg = cfg.split("=")
            hsflowd_cfg['collector'] = cfg[1].strip()

    # Check hsflowd configuration
    step("Expected sFlow configuration: " + str(expected_cfg))
    step("hsflowd configuration: " + str(hsflowd_cfg))
    assert expected_cfg == hsflowd_cfg
    step("### sFlow configuration in hsflowd.conf successfully verified ###")
