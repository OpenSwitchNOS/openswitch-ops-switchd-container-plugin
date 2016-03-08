# -*- coding: utf-8 -*-
# (C) Copyright 2015 Hewlett Packard Enterprise Development LP
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


def test_switchd_ct_vlan_trunk_invalid_trunks(topology, step):
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
