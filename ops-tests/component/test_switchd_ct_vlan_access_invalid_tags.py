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


def test_switchd_ct_vlan_access_invalid_tags(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")
    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    p1 = ops1.ports["if01"]
    p2 = ops1.ports["if02"]

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
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["errors"] >= 4

    step("Delete port 2 and add port 2 with tag 100 on Switch (access mode)")
    ops1("del-port 2", shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100".format(int=p2),
         shell="vsctl")
    ops1("set interface {int} user_config:admin=up".format(int=p2),
         shell="vsctl")

    step("Test Ping - should work")
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3
