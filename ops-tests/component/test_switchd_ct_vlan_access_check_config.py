# -*- coding: utf-8 -*-
# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
##########################################################################

"""
OpenSwitch Test for switchd related configurations.
"""

from time import sleep

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
"""


def test_switchd_ct_vlan_access_check_config(topology, step):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    step("Add br0 on switch")
    ops1("add-br br0", shell="vsctl")

    ops_br_name = ops1("get br br0 name", shell="vsctl").strip()
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get br br0 datapath_type name",
               shell="bash")
    ovs_datapath_type, ovs_br_name = out.splitlines()
    assert (
        ovs_datapath_type == 'netdev' and
        ops_br_name == ovs_br_name
    )

    step("Add VLAN100 to global VLAN table on Switch")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    out = ops1("get vlan VLAN100 admin id name", shell="vsctl")
    ops_admin_state, ops_vlan_id, ops_vlan_name = out.splitlines()
    assert (
        ops_admin_state == 'up' and
        ops_vlan_id == '100' and
        ops_vlan_name == '"VLAN100"'
    )

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    ops1("add-port br0 1 vlan_mode=access tag=100", shell="vsctl")
    ops1("set interface 1 user_config:admin=up", shell="vsctl")
    sleep(1)

    out = ops1("get port 1 name tag vlan_mode", shell="vsctl")
    ops_port_name, ops_tag, ops_vlan_mode = out.splitlines()
    ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 name tag vlan_mode",
         shell="bash")
    ovs_port_name, ovs_tag, ovs_vlan_mode = out.splitlines()
    assert (
        ops_port_name == ovs_port_name and
        ops_tag == ovs_tag and
        ops_vlan_mode == ovs_vlan_mode
    )

    step("Verify whether same configuration gets set on the ASIC"
         " simulating InternalOVS")
    out = ops1("get interface 1 admin_state link_state user_config:admin "
               "hw_intf_info:mac_addr", shell="vsctl")
    x, ops_admin_state, ops_link_state, ops_user_config, ops_mac_addr = (
        out.splitlines()
    )
    print(x)
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get interface 1 "
               "admin_state mac_in_use", shell="bash")
    x, ovs_admin_state, ovs_mac_addr = out.splitlines()
    print(x)
    assert (
        ops_admin_state == ovs_admin_state and
        ops_link_state == 'up' and
        ops_user_config == 'up' and
        ops_mac_addr == ovs_mac_addr
    )
