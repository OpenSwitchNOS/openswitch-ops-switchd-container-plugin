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
@mark.skipif(True, reason="skipping test case due to stability issues in CIT")
def test_switchd_container_ct_changing_vlan_config(topology, step):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    step("Adding port 1 without any VLAN configuration and no VLAN "
         "enabled in the global VLAN table")
    ops1("add-port bridge_normal 1", shell="vsctl")
    ops1("set interface 1 user_config:admin=up", shell="vsctl")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode",
                     shell="bash")
    assert "trunk" not in port_name

    step("Enabling VLAN100 in the global VLAN table")
    ops1("add-vlan bridge_normal 100 admin=up", shell="vsctl")
    sleep(2)
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode",
                     shell="bash")
    assert "trunk" in port_name

    step("Port 1 got added in trunk mode in 'ASIC' OVS as expected")
    port_trunk = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 trunks",
                      shell="bash")
    assert "100" in port_trunk

    step("Now deleting port 1")
    ops1("del-port bridge_normal 1", shell="vsctl")

    step("Specifying just the tag field while adding port 1")
    ops1("add-port bridge_normal 1 tag=100", shell="vsctl")

    port_mode, port_tag = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                               "vlan_mode tag", shell="vsctl").splitlines()
    assert "access" in port_mode and "100" in port_tag

    step("Changing vlan mode to 'trunk' using ovs-vsctl set feature")
    ops1("set port 1 vlan_mode=trunk", shell="vsctl")
    port_mode, port_trunk = ops1("get port 1 vlan_mode trunks",
                                 shell="vsctl").splitlines()
    assert "trunk" in port_mode and "100" in port_trunk

    step("Changing vlan mode to 'native-tagged' using ovs-vsctl set "
         "feature")
    ops1("set port 1 vlan_mode=native-tagged tag=100 trunks=100",
         shell="vsctl")
    port_mode, port_trunk, port_tag = \
        ops1("get port 1 vlan_mode trunks tag", shell="vsctl").splitlines()
    assert (
        "native-tagged" in port_mode and "100" in port_trunk and
        "100" in port_tag
    )

    step("Changing vlan mode to 'native-untagged' using ovs-vsctl "
         "set feature")
    ops1("set port 1 vlan_mode=native-untagged tag=100 trunks=100")
    port_mode, port_trunk, port_tag = ops1(
        "/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode trunks tag",
        shell="bash").splitlines()
    assert (
        "native-untagged" in port_mode and "100" in port_trunk and
        "100" in port_tag
    )

    step("Setting trunks=200 and vlan_mode=trunk for port 1 which "
         "is not enabled in the global VLAN table")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode",
                     shell="bash")
    assert "trunk" not in port_name

    step("Setting tag=200 and vlan_mode=access for port 1 which "
         "is not enabled in the global VLAN table")
    ops1("set port 1 vlan_mode=access tag=200", shell="vsctl")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode",
                     shell="bash")
    assert "access" not in port_name

    step("Deleting VLAN100 from the global VLAN table")
    ops1("del-vlan bridge_normal 100", shell="vsctl")

    step("Adding port 2 without an VLAN configuration specified")
    ops1("add-port bridge_normal 2", shell="vsctl")
    port_name = ops1("get port 2 vlan_mode", shell="vsctl")
    assert "trunk" not in port_name

    step("Adding VLAN100 and VLAN200 in the global VLAN table")
    ops1("add-vlan bridge_normal 100 admin=up", shell="vsctl")
    sleep(1)
    ops1("add-vlan bridge_normal 200 admin=up", shell="vsctl")
    sleep(1)

    port_mode_1, port_tag = ops1(
        "get port 1 vlan_mode tag", shell="vsctl").splitlines()
    port_mode_2, port_trunk = ops1(
        "get port 2 vlan_mode trunks", shell="vsctl").splitlines()
    assert (
        "access" in port_mode_1 and "200" in port_tag and
        "trunk" in port_mode_2 and "100, 200" in port_trunk
    )

    step("Deleting VLAN200 from the global VLAN table")
    ops1("del-vlan bridge_normal 200", shell="vsctl")
    port_name_1 = ops1("get port 1 vlan_mode", shell="vsctl")
    port_mode_2, port_trunk = ops1(
        "get port 2 vlan_mode trunks", shell="vsctl").splitlines()
    assert (
        "access" not in port_name_1 and "100" in port_trunk and
        "trunk" in port_mode_2 and "200" not in port_trunk
    )
