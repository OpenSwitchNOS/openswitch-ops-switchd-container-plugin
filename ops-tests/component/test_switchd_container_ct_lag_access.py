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
[type=host name="Host 1"] hs1
[type=host name="Host 2"] hs2

ops1:if01 -- hs1:if01
ops2:if01 -- hs2:if01
ops1:if02 -- ops2:if02
ops1:if03 -- ops2:if03
"""


def test_switchd_container_ct_lag_access(topology, step):
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
    s1p3 = ops1.ports["if03"]

    s2p1 = ops2.ports["if01"]
    s2p2 = ops2.ports["if02"]
    s2p3 = ops2.ports["if03"]

    step("Adding ports 1,lag0(2,3) to VLAN100. Port 1:access,"
         "Port lag0(2,3):access")
    ops1("add-br br0", shell="vsctl")
    ops2("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops2("add-vlan br0 100 admin=up", shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100 -- set interface {int} "
         "user_config:admin=up".format(int=s1p1), shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=access tag=100 -- set interface {int} "
         "user_config:admin=up".format(int=s2p1), shell="vsctl")
    ops1("add-bond br0 lag0 {int1} {int2} vlan_mode=access tag=100 -- set "
         "interface {int1} user_config:admin=up -- set interface {int2} "
         "user_config:admin=up".format(int1=s1p2, int2=s1p3), shell="vsctl")
    sleep(2)
    ops2("add-bond br0 lag0 {int1} {int2} vlan_mode=access tag=100 -- set "
         "interface {int1} user_config:admin=up -- set interface {int2} "
         "user_config:admin=up".format(int1=s2p2, int2=s2p3), shell="vsctl")
    sleep(2)

    s1_getbrname_ops = ops1("get br br0 name", shell="vsctl").strip()
    s1_getbrname_openovs, s1_getbrdp_openovs = ops1(
        "/opt/openvswitch/bin/ovs-vsctl get br br0 name datapath_type",
        shell="bash"
    ).splitlines()
    assert (
        s1_getbrname_ops == s1_getbrname_openovs and
        s1_getbrdp_openovs == "netdev"
    )

    s1_getvlanadmin_ops, s1_getvlanid_ops, s1_getvlanname_ops = ops1(
        "get vlan VLAN100 admin id name", shell="vsctl"
    ).splitlines()
    assert (
        s1_getvlanadmin_ops == 'up' and s1_getvlanid_ops == '100' and
        s1_getvlanname_ops == '"VLAN100"'
    )

    s1_getportname_ops, s1_getporttag_ops, s1_getportvlanmode_ops = ops1(
        "get port lag0 name tag vlan_mode", shell="vsctl"
    ).splitlines()
    s1_getportname_openovs, s1_getporttag_openovs, s1_getportvlanmode_openovs = ops1(  # noqa
        "/opt/openvswitch/bin/ovs-vsctl get port lag0 name tag vlan_mode",
        shell="bash"
    ).splitlines()
    assert(
        s1_getportname_ops == s1_getportname_openovs and
        s1_getporttag_ops == s1_getporttag_openovs and
        s1_getportvlanmode_ops == s1_getportvlanmode_openovs
    )

    s1_getintfadmin_ops, s1_getintfmac_ops, s1_getintflink_ops = ops1(
        "get interface {int} admin_state hw_intf_info:mac_addr "
        "link_state".format(int=s1p2),
        shell="vsctl"
    ).splitlines()
    s1_getintfadmin_openovs, s1_getintfmac_openovs, s1_getintflink_openovs = ops1(  # noqa
        "/opt/openvswitch/bin/ovs-vsctl get interface {int} admin_state"
        " mac_in_use link_state".format(int=s1p2), shell="bash"
    ).splitlines()
    assert (
        s1_getintfadmin_ops == s1_getintfadmin_openovs and
        s1_getintfmac_ops == s1_getintfmac_openovs and
        s1_getintflink_ops == s1_getintflink_openovs
    )

    ops1("del-vlan br0 100", shell="vsctl")
    out = ops1("get interface {int} admin_state".format(int=s1p2),
               shell="vsctl").strip()
    assert "no row {int} in table Interface".format(int=s1p2) in out

    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    s1_getportname_openovs, s1_getporttag_openovs, s1_getportvlanmode_openovs = ops1(  # noqa
        "/opt/openvswitch/bin/ovs-vsctl get port lag0 name tag vlan_mode",
        shell="bash"
    ).splitlines()
    assert (
        s1_getportname_ops == s1_getportname_openovs and
        s1_getporttag_ops == s1_getporttag_openovs and
        s1_getportvlanmode_ops == s1_getportvlanmode_openovs
    )

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("#### Test Case 2 - Ping between two LAGs  #####\n"
         "### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access,"
         "Port lag0(2,3):access ###\n")

    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {s1p1} vlan_mode=access "
         "tag=100 -- set interface {s1p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {s2p1} vlan_mode=access "
         "tag=100 -- set interface {s2p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {s1p2} {s1p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s1p2} user_config:admin=up "
         "-- set interface {s1p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {s2p2} {s2p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s2p2} user_config:admin=up "
         "-- set interface {s2p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(11)

    hs1.libs.ip.interface('if01', addr='192.168.1.2/24', up=True)
    hs2.libs.ip.interface('if01', addr='192.168.1.3/24', up=True)
    ping = hs1.libs.ping.ping(10, '192.168.1.3')
    assert ping["received"] >= 7

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("\n## Test Case 3 - Ping between two LAGs "
         "with and without global VLAN  ##\n")

    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {s1p1} vlan_mode=access "
         "tag=100 -- set interface {s1p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {s2p1} vlan_mode=access "
         "tag=100 -- set interface {s2p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {s1p2} {s1p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s1p2} user_config:admin=up "
         "-- set interface {s1p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {s2p2} {s2p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s2p2} user_config:admin=up "
         "-- set interface {s2p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(5, '192.168.1.3')
    assert ping["errors"] == 5

    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(10, '192.168.1.3')
    assert ping["received"] >= 7

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("\n## Test Case 4 - Ping between two LAGs"
         " - Different and same tags  ##\n")

    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {s1p1} vlan_mode=access "
         "tag=100 -- set interface {s1p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {s2p1} vlan_mode=access "
         "tag=100 -- set interface {s2p1} "
         "user_config:admin=up".format(**locals()), shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {s1p2} {s1p3} "
         "vlan_mode=access tag=200 "
         "-- set interface {s1p2} user_config:admin=up "
         "-- set interface {s1p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {s2p2} {s2p3} "
         "vlan_mode=access tag=200 "
         "-- set interface {s2p2} user_config:admin=up "
         "-- set interface {s2p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(5, '192.168.1.3')
    assert ping["errors"] == 5

    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    sleep(2)
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {s1p2} {s1p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s1p2} user_config:admin=up "
         "-- set interface {s1p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {s2p2} {s2p3} "
         "vlan_mode=access tag=100 "
         "-- set interface {s2p2} user_config:admin=up "
         "-- set interface {s2p3} "
         "user_config:admin=up".format(**locals()), shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(10, '192.168.1.3')
    assert ping["received"] >= 7

    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")
