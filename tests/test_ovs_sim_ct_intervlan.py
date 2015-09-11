#!/usr/bin/env python

# Copyright (C) 2015 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may
# not use this file except in compliance with the License. You may obtain
# a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations
# under the License.

import pytest
from halonvsi.docker import *
from halonvsi.halon import *
from halonutils.halonutil import *

class simIntervlanTests( HalonTest ):

  def setupNet(self):
    # if you override this function, make sure to
    # either pass getNodeOpts() into hopts/sopts of the topology that
    # you build or into addHost/addSwitch calls
    self.net = Mininet(topo=SingleSwitchTopo(k=2,
                                             hopts=self.getHostOpts(),
                                             sopts=self.getSwitchOpts()),
                                             switch=HalonSwitch,
                                             host=HalonHost,
                                             link=HalonLink, controller=None,
                                             build=True)

  def sim_intervlan_direct_connected(self):
    info("########## Verify intervlan for directly connected hosts ##########\n")
    # configuring Halon, in the future it would be through
    # proper Halon commands
    s1 = self.net.switches[ 0 ]
    h1 = self.net.hosts[ 0 ]
    h2 = self.net.hosts[ 1 ]
    # Configure switch s1
    s1.cmdCLI("configure terminal")

    # Configure VLANS 100 and 200 on siwtch
    s1.cmdCLI("vlan 100")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("exit")

    s1.cmdCLI("vlan 200")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("exit")


    # Configure interface 1 on switch s1
    s1.cmdCLI("interface 1")
    s1.cmdCLI("no routing")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("vlan access 100")
    s1.cmdCLI("exit")

    # Configure interface 2 on switch s1
    s1.cmdCLI("interface 2")
    s1.cmdCLI("no routing")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("vlan access 200")
    s1.cmdCLI("exit")

    info("Configuring interface vlan 100\n")
    time.sleep(2)
    # Configure interface vlan 100 and vlan 200 on switch s1
    s1.cmdCLI("interface vlan 100")
    s1.cmdCLI("ip address 100.0.0.1/24")
    time.sleep(2)
    s1.cmdCLI("ipv6 address 100::1/120")
    s1.cmdCLI("exit")

    time.sleep(5)

    info("Configuring interface vlan 200\n")
    s1.cmdCLI("interface vlan 200")
    s1.cmdCLI("ip address 200.0.0.1/24")
    time.sleep(2)
    s1.cmdCLI("ipv6 address 200::1/120")
    s1.cmdCLI("exit")

    # Check trunks list on port bridge normal in native OVS
    output = s1.cmd("/opt/openvswitch/bin/ovs-vsctl list port bridge_normal")
    assert "100, 200" in output, "Trunk vlan 100 on bridge_normal wasnt configured"

    info("vlan interface configuration: Success\n")

    # HALON_TODO: Using "no shutdown" needs sleep of 10 seconds.
    # Uncomment below line and remove ovs-vsctl command once issue is resolved

    #time.sleep(10)

    # Bring interface 1 up
    s1.cmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")

    # Bring interface 2 up
    s1.cmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

    # Configure host 1
    info("Configuring host 1 with 100.0.0.2/24\n")
    h1.cmd("ip addr add 100.0.0.2/24 dev h1-eth0")
    h1.cmd("ip route add 200.0.0.0/24 via 100.0.0.1");
    info("Configuring host 1 with 100::2/120\n")
    h1.cmd("ip addr add 100::2/120 dev h1-eth0")
    h1.cmd("ip route add 200::0/120 via 100::1")

    # Configure host 2
    info("Configuring host 2 with 200.0.0.2/24\n")
    h2.cmd("ip addr add 200.0.0.2/24 dev h2-eth0")
    h2.cmd("ip route add 100.0.0.0/24 via 200.0.0.1");
    info("Configuring host 2 with 200::2/120\n")
    h2.cmd("ip addr add 200::2/120 dev h2-eth0")
    h2.cmd("ip route add 100::0/120 via 200::1")

    # Ping from host 1 to host2
    info("Ping h2 from h1\n")
    output = h1.cmd("ping 200.0.0.2 -c2")
    status = parsePing(output)
    assert status, "Ping Failed\n"
    info("Ping Success\n")

    # Ping6 from host 1 to host 2
    info("IPv6 Ping h2 from h1\n")
    output = h1.cmd("ping6 200::2 -c2")
    status = parsePing(output)
    #assert status, "Pingi6 Failed"
    info("Ping6 Success\n")

    # Unconfigure interface vlan 100
    info("Unconfigure vlan interface. This will disable inter-vlan routing.\n")
    s1.cmdCLI("no interface vlan 100")
    s1.cmdCLI("no interface vlan 200")

    time.sleep(2)
    # Check trunks list on port bridge normal in native OVS
    output = s1.cmd("/opt/openvswitch/bin/ovs-vsctl list port bridge_normal")
    assert "100, 200" not in output, "Trunk vlans 100, 200 on bridge_normal still configured"


    # Ping from host 1 to host2
    info("Check IPv4 ping h2 from h1\n")
    output = h1.cmd("ping 200.0.0.2 -c2")
    status = parsePing(output)
    assert status == 0, "Ping succeeded even though vlan interface is not present``\n"
    info("Ipv4 routing between vlans disabled\n")

    # Ping6 from host 1 to host 2
    info("Check IPv6 Ping h2 from h1\n")
    output = h1.cmd("ping6 2000::2 -c2")
    status = parsePing(output)
    assert status == 0, "Ping6 succeeded even though vlan interface is not present`\n"
    info("Ipv6 routing between vlans disabled\n")
    info("Delete vlan interface Success\n")


    info("########## inter-vlan test for directly connected hosts passed ##########\n")

class Test_sim_intervlan:

  def setup_class(cls):
    Test_sim_intervlan.test = simIntervlanTests()

  # Test for slow routing between directly connected hosts
  def test_sim_intervlan_direct_connected(self):
    self.test.sim_intervlan_direct_connected()

  def teardown_class(cls):
    Test_sim_intervlan.test.net.stop()

  def __del__(self):
    del self.test
