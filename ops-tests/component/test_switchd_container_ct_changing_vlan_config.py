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
OpenSwitch Test for interface related configurations.
"""

from time import sleep
from pytest import mark

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
"""


@mark.platform_incompatible(['ostl'])
def test_switchd_container_ct_changing_vlan_config(topology, step):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    step("Adding port 1 with a DEFAULT VLAN configuration and "
         "verify the trunking capability of the port ")

    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
    port_name = ops1("show running-config interface 1", shell="vtysh")

    assert "access 1" in port_name, "Port 1 did not get added in " \
        "'ASIC' OVS when a default VLAN is present"

    step("Enabling VLAN100 in the global VLAN table")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()
    sleep(2)
    port_name = ops1("show running-config interface 1", shell="vtysh")
    assert "access 1" in port_name, "Port 1 did not get added in " \
        "'ASIC' OVS even after global VLAN100 was enabled"

    step("Now deleting port 1")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.routing()

    step("Specifying just the tag and vlan_mode field while adding port 1 "
         "considering the presence of DEFAULT_VLAN_1")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_routing()
        ctx.vlan_access("100")
    sleep(1)

    port_mode_tag = ops1("show running-config interface 1", shell="vtysh")
    assert "access 100" in port_mode_tag

    step("Changing vlan mode to 'trunk' using ovs-vsctl set feature")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.vlan_trunk_allowed("100")
    port_mode_trunk = ops1("show running-config interface 1", shell="vtysh")
    assert "trunk allowed 100" in port_mode_trunk

    step("Changing vlan mode to 'native-tagged' using ovs-vsctl set "
         "feature")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.vlan_trunk_native("100")
        ctx.vlan_trunk_native_tag()
    port_buf = ops1("show running-config interface 1", shell="vtysh")

    assert (
        "native tag" in port_buf and "native 100" in port_buf)

    step("Changing vlan mode to 'native untagged' using vtysh lib ")
    with ops1.libs.vtysh.ConfigInterface("1") as ctx:
        ctx.no_vlan_trunk_native_tag()

    port_buf = ops1("show running-config interface 1", shell="vtysh")
    assert (
        "native tag" not in port_buf and "trunk allowed 100" in port_buf)

    step("Setting tag=200 and vlan_mode=access for port 1 which "
         "is not enabled in the global VLAN table")
    # with ops1.libs.vtysh.ConfigInterface("1") as ctx:
    #    ctx.vlan_access("200")

    port_buf = ops1("show running-config interface 1", shell="vtysh")
    assert "access 200" not in port_buf

    step("Deleting VLAN100 from the global VLAN table")
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(100)

    step("Adding VLAN100 and VLAN200 in the global VLAN table")
    with ops1.libs.vtysh.ConfigVlan("100") as ctx:
        ctx.no_shutdown()

    sleep(1)

    with ops1.libs.vtysh.ConfigVlan("200") as ctx:
        ctx.no_shutdown()

    sleep(1)

    step("Adding port 2 without an VLAN configuration specified")
    with ops1.libs.vtysh.ConfigInterface("2") as ctx:
        ctx.no_routing()
        ctx.no_shutdown()
        ctx.vlan_trunk_native("200")

    port_2_buf = ops1("show running-config interface 2", shell="vtysh")
    assert "trunk native 200" in port_2_buf

    step("Deleting VLAN200 from the global VLAN table")
    with ops1.libs.vtysh.Configure() as ctx:
        ctx.no_vlan(200)
    port_mode_1 = ops1("show running-config interface 1", shell="vtysh")
    port_mode_2 = ops1("show running-config interface 2", shell="vtysh")
    assert (
        "access 100" not in port_mode_1 and "trunk 200" not in port_mode_2
    )
