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

ops1:if01 -- hs1:eth0
ops1:if02 -- hs2:eth0
"""


@mark.platform_incompatible(['ostl'])
def test_switchd_container_ct_if_stats(topology, step):
    ops1 = topology.get("ops1")
    hs1 = topology.get("hs1")
    hs2 = topology.get("hs2")

    assert ops1 is not None
    assert hs1 is not None
    assert hs2 is not None

    ops1("configure terminal")

    ops1("interface 1")
    ops1("no shutdown")
    ops1("ip address 100.0.0.1/24")
    ops1("exit")

    ops1("interface 2")
    ops1("no shutdown")
    ops1("ip address 200.0.0.1/24")
    ops1("exit")

    hs1.libs.ip.interface('eth0', addr='100.0.0.2/24', up=True)
    hs1("ip route add 200.0.0.0/24 via 100.0.0.1")

    hs2.libs.ip.interface('eth0', addr='200.0.0.2/24', up=True)
    hs2("ip route add 100.0.0.0/24 via 200.0.0.1")

    sleep(5)

    ping = hs1.libs.ping.ping(6, '200.0.0.2')
    assert ping["received"] >= 4

    sleep(30)

    out = ops1("do show interface {int}".format(int=ops1.ports["if01"]))
    for line in out.splitlines():
        if 'input packets' in line:
            rx = line.split('input packets')[0]
            assert int(rx) > 8
        if 'output packets' in line:
            tx = line.split('output packets')[0]
            assert int(tx) > 8
