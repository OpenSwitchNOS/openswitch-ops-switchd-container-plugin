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
[type=openswitch name="OpenSwitch 2"] ops2

# Links
ops1:if01 -- hs1:if01
ops2:if01 -- hs2:if01
ops1:if02 -- ops2:if02
"""


def test_switchd_container_ct_vlan_trunk(topology, step):
    ops1 = topology.get("ops1")
    ops2 = topology.get("ops2")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")
    assert ops1 is not None
    assert ops2 is not None
    assert hs1 is not None
    assert hs2 is not None

    s1p1 = ops1.ports["if01"]
    s1p2 = ops1.ports["if02"]
    s2p1 = ops2.ports["if01"]
    s2p2 = ops2.ports["if02"]

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
        ops_vlan_id == '100'and
        ops_vlan_name == '"VLAN100"'
    )

    step("Add port 1 with tag 100 and port  2 with trunk 100 on Switch ")
    ops1("add-port br0 1 vlan_mode=access tag=100", shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p1),
         shell="vsctl")
    ops1("add-port br0 2 vlan_mode=trunk trunks=100", shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p2),
         shell="vsctl")
    sleep(1)

    step("Verify whether same configuration gets set on the ASIC"
         " simulating InternalOVS")
    out = ops1("get port {int} name tag vlan_mode".format(int=s1p1),
               shell="vsctl")
    ops_port_name, ops_tag, ops_vlan_mode = out.splitlines()
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get port {int} name tag "
               "vlan_mode".format(int=s1p1),
               shell="bash")
    ovs_port_name, ovs_tag, ovs_vlan_mode = out.splitlines()
    assert (
        ops_port_name == ovs_port_name and
        ops_tag == ovs_tag and ops_vlan_mode == ovs_vlan_mode
    )

    out = ops1("get port {int} name trunks vlan_mode".format(int=s1p2),
               shell="vsctl")
    ops_port_name, ops_trunks, ops_vlan_mode = out.splitlines()
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get port {int} name "
               "trunks vlan_mode".format(int=s1p2), shell="bash")
    ovs_port_name, ovs_trunks, ovs_vlan_mode = out.splitlines()
    assert (
        ops_port_name == ovs_port_name and
        ops_tag == ovs_tag and ops_vlan_mode == ovs_vlan_mode
    )

    x, ops_admin_state, ops_link_state, ops_user_config, ops_mac_addr = (
        ops1("get interface {int} admin_state link_state user_config:admin "
             "hw_intf_info:mac_addr".format(int=s1p1),
             shell="vsctl").splitlines()
    )
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get interface {int} "
               "admin_state mac_in_use".format(int=s1p1), shell="bash")
    x, ovs_admin_state, ovs_mac_addr = out.splitlines()
    assert (
        ops_admin_state == ovs_admin_state and ops_link_state == 'up'and
        ops_user_config == 'up' and ops_mac_addr == ovs_mac_addr
    )

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port br0 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Add VLAN 100 to global VLAN table on Switch1 and Switch2")
    ops1("add-br br0", shell="vsctl")
    ops2("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops2("add-vlan br0 100 admin=up", shell="vsctl")

    step("Add port 1 with tag 100 on Switch1 and Switch2 (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=s1p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p1),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=access tag=100".format(int=s2p1),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p1),
         shell="vsctl")

    step("Add port 2 with trunk 100 on Switch1 and Switch2 (trunk mode)")
    ops1("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s1p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p2),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s2p2),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p2),
         shell="vsctl")

    step("Test Ping - should work")
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Dont add VLAN 100 to global VLAN table of Switch1 and Switch2")
    ops1("add-br br0", shell="vsctl")
    ops2("add-br br0", shell="vsctl")

    step("Add port 1 with tag 100 on Switch1 and Switch2 (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=s1p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p1),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=access tag=100".format(int=s2p1),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p1),
         shell="vsctl")

    step("Add port 2 with trunk 100 on Switch1 and Switch2 (trunk mode)")
    ops1("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s1p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p2),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s2p2),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p2),
         shell="vsctl")

    step("Test Ping - should not work")
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["errors"] == 5

    step("Add VLAN 100 to global VLAN table of Switch1 and Switch2, "
         "ports should get reconfigured")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops2("add-vlan br0 100 admin=up", shell="vsctl")

    step("Test Ping - should work")
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Add VLAN 100 to global VLAN table in Switch1 and Switch2")
    ops1("add-br br0", shell="vsctl")
    ops2("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops2("add-vlan br0 100 admin=up", shell="vsctl")

    step("Add port 1 with tag 100 on Switch1 and Switch2 (access mode)")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=s1p1),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p1),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=access tag=100".format(int=s2p1),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p1),
         shell="vsctl")

    step("Add port 2 with trunk 100 on Switch1 and Switch2 (trunk mode)")
    ops1("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s1p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=s1p2),
         shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=trunk trunks=200".format(int=s2p2),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p2),
         shell="vsctl")

    step("Test Ping - should not work")
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["errors"] == 5

    step("Delete port 2 and add port 2 with trunk 100 on Switch1 and Switch2 "
         "(trunk mode)")
    ops2("del-port {int}".format(int=s2p2), shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=trunk trunks=100".format(int=s2p2),
         shell="vsctl")
    ops2("set interface {int} user_config:admin=up".format(int=s2p2),
         shell="vsctl")

    step("Test Ping - should work")
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 2", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")
