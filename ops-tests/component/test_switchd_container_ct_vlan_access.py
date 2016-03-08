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
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

ops1:if01 -- hs1:if01
ops1:if02 -- hs2:if01
"""


def test_switchd_container_ct_vlan_access(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")
    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    p1 = ops1.ports["if01"]
    p2 = ops1.ports["if02"]

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
    ops_admin_state, ops_link_state, ops_user_config, ops_mac_addr = (
        out.splitlines()
    )
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get interface 1 "
               "admin_state mac_in_use", shell="bash")
    ovs_admin_state, ovs_mac_addr = out.splitlines()
    assert (
        ops_admin_state == ovs_admin_state and
        ops_link_state == 'up' and
        ops_user_config == 'up' and
        ops_mac_addr == ovs_mac_addr
    )

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Add VLAN 100 to global VLAN table on Switch")
    ops1("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p1),
         shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p2),
         shell="vsctl")

    step("Test Ping - should work")
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Dont add VLAN 100 to global VLAN table of Switch")
    ops1("add-br br0", shell="vsctl")

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p1),
         shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p2),
         shell="vsctl")

    step("Test Ping - should not work")

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] == 0

    step("Add VLAN 100 to global VLAN table of Switch, ports should get "
         "reconfigured")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")

    step("Test Ping - should work")
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Add VLAN 100 to global VLAN table in Switch")
    ops1("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")

    step("Add port 1 and with tag 100 on Switch (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p1),
         shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=200".format(int=p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p2),
         shell="vsctl")

    step("Test Ping - should not work")

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] == 0

    step("Delete port 2 and add port 2 with tag 100 on Switch (access mode)")
    ops1("del-port 2", shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p2),
         shell="vsctl")

    step("Test Ping - should work")
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
