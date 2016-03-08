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


def test_container_config_check_trunk(topology, step):
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

    step("Config check")
    ops1("add-br br0", shell="vsctl")
    ops2("add-br br0", shell="vsctl")
    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops2("add-vlan br0 100 admin=up", shell="vsctl")
    ops1("add-port br0 {int} vlan_mode=access tag=100 -- set interface {int}"
         " user_config:admin=up".format(int=s1p1), shell="vsctl")
    ops2("add-port br0 {int} vlan_mode=access tag=100 -- set interface {int}"
         " user_config:admin=up".format(int=s2p1), shell="vsctl")
    ops1("add-bond br0 lag0 {int1} {int2} vlan_mode=trunk trunks=100 -- set "
         "interface {int1} user_config:admin=up -- set interface {int2} "
         "user_config:admin=up".format(int1=s1p2, int2=s1p3), shell="vsctl")
    sleep(2)
    ops2("add-bond br0 lag0 {int1} {int2} vlan_mode=trunk trunks=100 -- set "
         "interface {int1} user_config:admin=up -- set interface {int2} "
         "user_config:admin=up".format(int1=s2p2, int2=s2p3), shell="vsctl")
    sleep(2)

    s1_getbrname_ops = ops1("get br br0 name", shell="vsctl").strip()
    s1_getbrname_openovs, s1_getbrdp_openovs = ops1(
        "/opt/openvswitch/bin/ovs-vsctl get br br0 name datapath_type",
        shell="bash").splitlines()
    assert (
        s1_getbrname_ops == s1_getbrname_openovs and
        s1_getbrdp_openovs == 'netdev'
    )

    s1_getvlanadmin_ops, s1_getvlanid_ops, s1_getvlanname_ops = ops1(
        "get vlan VLAN100 admin id name", shell="vsctl").splitlines()

    assert (
        s1_getvlanadmin_ops == 'up' and
        s1_getvlanid_ops == '100' and
        s1_getvlanname_ops == '"VLAN100"'
    )

    s1_getportname_ops, s1_getporttrunk_ops, s1_getportvlanmode_ops = ops1(
        "get port lag0 name trunks vlan_mode", shell="vsctl").splitlines()
    s1_getportname_openovs, s1_getporttrunk_openovs, s1_getportvlanmode_openovs = ops1(  # noqa
        "/opt/openvswitch/bin/ovs-vsctl get port lag0 name trunks vlan_mode",
        shell="bash").splitlines()

    assert (
        s1_getportname_ops == s1_getportname_openovs and
        s1_getporttrunk_ops == s1_getporttrunk_openovs and
        s1_getportvlanmode_ops == s1_getportvlanmode_openovs
    )

    x, s1_getintfadmin_ops, s1_getintfmac_ops, s1_getintflink_ops = ops1(
        "get interface 2 admin_state hw_intf_info:mac_addr link_state",
        shell="vsctl").splitlines()
    x, s1_getintfadmin_openovs, s1_getintfmac_openovs, s1_getintflink_openovs = ops1(  # noqa
        "/opt/openvswitch/bin/ovs-vsctl get interface {int} admin_state "
        "mac_in_use link_state".format(int=s1p2), shell="bash").splitlines()

    assert (
        s1_getintfadmin_ops == s1_getintfadmin_openovs and
        s1_getintfmac_ops == s1_getintfmac_openovs and
        s1_getintflink_ops == s1_getintflink_openovs
    )

    ops1("del-vlan br0 100", shell="vsctl")
    out = ops1("/opt/openvswitch/bin/ovs-vsctl get interface {int} "
               "admin_state".format(int=s1p2), shell="bash").strip()
    assert "no row \"2\" in table Interface" in out

    ops1("add-vlan br0 100 admin=up", shell="vsctl")
    ops1("/opt/openvswitch/bin/ovs-vsctl get port lag0 name tag "
         "vlan_mode", shell="bash").splitlines()

    assert (
        s1_getportname_ops == s1_getportname_openovs and
        s1_getporttrunk_ops == s1_getporttrunk_openovs and
        s1_getportvlanmode_ops == s1_getportvlanmode_openovs
    )

    step("Clean up")
    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Lag normal")
    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100"
         " -- set interface {int} user_config:admin=up".format(int=s1p1),
         shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100"
         " -- set interface {int} user_config:admin=up".format(int=s2p1),
         shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} "
         "vlan_mode=trunk trunks=100 -- set interface {int2} "
         "user_config:admin=up -- set interface {int3} "
         "user_config:admin=up".format(int2=s1p2, int3=s1p3),
         shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} "
         "vlan_mode=trunk trunks=100 -- set interface {int2} "
         "user_config:admin=up -- set interface {int3} "
         "user_config:admin=up".format(int2=s2p2, int3=s2p3),
         shell="bash")
    sleep(11)

    hs1.libs.ip.interface('if01', addr='192.168.1.2/24', up=True)
    hs2.libs.ip.interface('if01', addr='192.168.1.3/24', up=True)
    ping = hs1.libs.ping.ping(10, '192.168.1.3')
    assert ping["received"] >= 7

    step("Clean up")
    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Lag missing")
    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100 -- "
         "set interface {int} user_config:admin=up".format(int=s1p1),
         shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100 -- "
         "set interface 1 user_config:admin=up".format(int=s2p1),
         shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=100 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s1p2, int3=s1p3),
         shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=100 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s2p2, int3=s2p3),
         shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(5, '192.168.1.3')
    assert ping["errors"] == 5

    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(5, '192.168.1.3')
    assert ping["received"] >= 3

    step("Clean up")
    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")

    step("Lag invalid trunk")
    ops1("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-br br0", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops2("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up", shell="bash")
    ops1("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100 -- "
         "set interface {int} user_config:admin=up".format(int=s1p1),
         shell="bash")
    ops2("/usr/bin/ovs-vsctl add-port br0 {int} vlan_mode=access tag=100 -- "
         "set interface {int} user_config:admin=up".format(int=s2p1),
         shell="bash")
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=200 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s1p2, int3=s1p3),
         shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=200 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s2p2, int3=s2p3),
         shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(5, '192.168.1.3')
    assert ping["errors"] == 5

    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    sleep(2)
    ops1("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=100 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s1p2, int3=s1p3),
         shell="bash")
    sleep(2)
    ops2("/usr/bin/ovs-vsctl add-bond br0 lag0 {int2} {int3} vlan_mode=trunk "
         "trunks=100 -- set interface {int2} user_config:admin=up -- set "
         "interface {int3} user_config:admin=up".format(int2=s2p2, int3=s2p3),
         shell="bash")
    sleep(11)

    ping = hs1.libs.ping.ping(10, '192.168.1.3')
    assert ping["received"] >= 5

    step("Clean up")
    ops1("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-vlan br0 100", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port 1", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-port lag0", shell="bash")
    ops1("/usr/bin/ovs-vsctl del-br br0", shell="bash")
    ops2("/usr/bin/ovs-vsctl del-br br0", shell="bash")
