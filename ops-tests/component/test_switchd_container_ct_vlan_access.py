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
from pytest import mark

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


@mark.platform_incompatible(['ostl'])
def test_switchd_container_ct_vlan_access(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")
    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    p1 = ops1.ports["if01"]
    p2 = ops1.ports["if02"]

    step("Add VLAN100 to global VLAN table on Switch")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()
    outbuf = ops1.libs.vtysh.show_running_config()
    vid_admin_up = outbuf['vlan']['100']['admin']

    assert( vid_admin_up == "up" , "VLAN 100 admin state is not up")

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")

    step("Verify whether same configuration gets set on the ASIC"
         " simulating InternalOVS")

    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(100)
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.routing()
        ctx.shutdown()

    step("Add VLAN 100 to global VLAN table on Switch")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")

    ops1.libs.vtysh.show_running_config()

    step("Test Ping - should work")
    hs1.libs.ip.interface('if01', addr="10.0.0.1/8", up=True)
    hs2.libs.ip.interface('if01', addr="10.0.0.2/8", up=True)
    sleep(2)
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.routing()
        ctx.shutdown()
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.routing()
        ctx.shutdown()

    step("Dont add VLAN 100 to global VLAN table of Switch")
    with ops1.libs.vtysh.ConfigVlan("200") as ctx:
        ctx.no_shutdown()

    step("Add port 1 and 2 with tag 100 on Switch (access mode)")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("200")

    ops1.libs.vtysh.show_running_config()

    step("Test Ping - should not work")
    sleep(2)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] == 0

    step("Add VLAN 100 to global VLAN table of Switch, ports should get "
         "reconfigured")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")

    ops1.libs.vtysh.show_running_config()

    step("Test Ping - should work")
    sleep(2)
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(100)
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(200)
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.routing()
        ctx.shutdown()
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.routing()
        ctx.shutdown()

    step("Add VLAN 100 to global VLAN table in Switch")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigVlan("200") as ctx:
        ctx.no_shutdown()

    step("Add port 1 and with tag 100 on Switch (access mode)")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("200")

    ops1.libs.vtysh.show_running_config()

    step("Test Ping - should not work")
    sleep(2)

    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] == 0

    step("Delete port 2 and add port 2 with tag 100 on Switch (access mode)")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.routing()
        ctx.shutdown()

    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")

    ops1.libs.vtysh.show_running_config()

    step("Test Ping - should work")
    sleep(2)
    ping4 = hs1.libs.ping.ping(5, "10.0.0.2")
    assert ping4["received"] >= 3

    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(100)
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(200)
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.routing()
        ctx.shutdown()
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.routing()
        ctx.shutdown()
