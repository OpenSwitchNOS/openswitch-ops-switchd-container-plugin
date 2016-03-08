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


def test_intervlan_direct_connected(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")
    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    step("Configure VLANS 100 and 200 on switch")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()
    with ops1.libs.vtysh.ConfigVlan("200") as ctx:
        ctx.no_shutdown()

    step("Configure interface 1 and 2 on switch")
    with ops1.libs.vtysh.ConfigInterface("if01") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("100")
    with ops1.libs.vtysh.ConfigInterface("if02") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_access("200")
    sleep(5)

    step("Configure interface vlan 100 and vlan 200 on switch")
    with ops1.libs.vtysh.ConfigInterfaceVlan("100") as ctx:
        ctx.ip_address("100.0.0.1/24")
    sleep(5)
    with ops1.libs.vtysh.ConfigInterfaceVlan("100") as ctx:
        ctx.ipv6_address("100::1/64")
    sleep(5)
    with ops1.libs.vtysh.ConfigInterfaceVlan("200") as ctx:
        ctx.ip_address("200.0.0.1/24")
    sleep(5)
    with ops1.libs.vtysh.ConfigInterfaceVlan("200") as ctx:
        ctx.ipv6_address("200::1/64")

    step("Bring interface 1 and 2 up")
    ops1("set interface 1 user_config:admin=up", shell="vsctl")
    ops1("set interface 2 user_config:admin=up", shell="vsctl")

    step("Configure hosts")
    # Host 1
    hs1.libs.ip.interface('if01', addr="100.0.0.2/24", up=True)
    hs1.libs.ip.add_route('200.0.0.0/24', '100.0.0.1')
    hs1.libs.ip.interface('if01', addr="100::2/64", up=True)
    hs1.libs.ip.add_route('200::0/64', '100::1')
    # Host 2
    hs2.libs.ip.interface('if01', addr="200.0.0.2/24", up=True)
    hs2.libs.ip.add_route('100.0.0.0/24', '200.0.0.1')
    hs2.libs.ip.interface('if01', addr="200::2/64", up=True)
    hs2.libs.ip.add_route('100::0/64', '200::1')

    step("Ping IPv4")
    ping4 = hs1.libs.ping.ping(2, "200.0.0.2")
    assert ping4["received"] >= 1

    step("Ping IPv6")
    # FIXME when DUP! is not present in ping6
    # ping6 = hs1.libs.ping.ping(2, "200::2")
    # assert ping6["received"] == 2
    ping6 = hs1("ping6 -c 2 200::2")
    assert "2 packets transmitted, 2 received" in ping6

    step("Unconfigure interface vlans")
    ops1("conf t")
    ops1("no interface vlan 100")
    ops1("no interface vlan 200")
    ops1("end")
    sleep(5)

    step("Check trunks list on port bridge normal in native OVS")
    output = ops1("list port bridge_normal", shell="vsctl")
    assert "100, 200" not in output

    step("Ping IPv4")
    ping4 = hs1.libs.ping.ping(2, "200.0.0.2")
    assert ping4["transmitted"] == 2 and ping4["received"] == 0

    step("Ping IPv6")
    # FIXME when DUP! is not present in ping6
    # ping6 = hs1.libs.ping.ping(2, "200::2")
    # assert ping6["received"] == 2
    ping6 = hs1("ping6 -c 2 200::2")
    assert "2 packets transmitted, 0 received" in ping6

    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()
    sleep(5)

    admin_state, link_state = ops1("get interface vlan100 "
                                   "admin_state link_state",
                                   shell="vsctl").splitlines()

    assert "up" in admin_state and "up" in link_state

    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.shutdown()

    admin_state, link_state = ops1("get interface vlan100 "
                                   "admin_state link_state",
                                   shell="vsctl").splitlines()

    assert "down" in admin_state and "down" in link_state
