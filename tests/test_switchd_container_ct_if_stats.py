#!/usr/bin/env python

# (c) Copyright 2015-2016 Hewlett Packard Enterprise Development LP
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
from opsvsi.docker import *
from opsvsi.opsvsitest import *
from opsvsiutils.systemutil import *

class simIntfStatsTests( OpsVsiTest ):

  def setupNet(self):
    host_opts = self.getHostOpts()
    switch_opts = self.getSwitchOpts()
    topo = SingleSwitchTopo(k=2, hopts=host_opts, sopts=switch_opts)
    self.net = Mininet(topo, switch=VsiOpenSwitch,
                       host=Host, link=OpsVsiLink,
                       controller=None, build=True)

  def interface_stats(self):
    info("########## Verify interface stats on VSI##########\n")
    s1 = self.net.switches[0]
    h1 = self.net.hosts[0]
    h2 = self.net.hosts[1]
    # Configure switch s1
    s1.cmdCLI("configure terminal")

    # Configure L3 on interface 1 and 2 on siwtch
    s1.cmdCLI("interface 1")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("ip address 100.0.0.1/24")
    s1.cmdCLI("exit")

    s1.cmdCLI("interface 2")
    s1.cmdCLI("no shutdown")
    s1.cmdCLI("ip address 200.0.0.1/24")
    s1.cmdCLI("exit")

    # Configure host 1
    info("Configuring host 1 with 100.0.0.2/24\n")
    h1.cmd("ip addr add 100.0.0.2/24 dev h1-eth0")
    h1.cmd("ip route add 200.0.0.0/24 via 100.0.0.1");

    # Configure host 2
    info("Configuring host 2 with 200.0.0.2/24\n")
    h2.cmd("ip addr add 200.0.0.2/24 dev h2-eth0")
    h2.cmd("ip route add 100.0.0.0/24 via 200.0.0.1");

    time.sleep(5)
    # Ping from host 1 to host2
    info("Ping h2 from h1\n")
    output = h1.cmd("ping 200.0.0.2 -c6")
    status = parsePing(output)
    assert status, "Ping Failed\n"
    info("Ping Success\n")

    time.sleep(5)
    out = s1.cmdCLI("do show interface 1")
    for line in out.split('\n'):
        if 'input packets' in line:
            rx = line.split('input packets')[0]
            assert int(rx) > 8, "\n##### Failed stats #####"
            info("Stats test Success\n")
        if 'output packets' in line:
            tx = line.split('output packets')[0]
            assert int(tx) > 8, "\n##### Failed stats #####"
            info("Stats test Success\n")


@pytest.mark.skipif(True, reason="Disabling old tests")
class Test_switchd_container_intf_stats:

  def setup_class(cls):
    Test_switchd_container_intf_stats.test = simIntfStatsTests()

  # Test for slow routing between directly connected hosts
  def test_switchd_container_interface_stats(self):
    self.test.interface_stats()

  def teardown_class(cls):
    Test_switchd_container_intf_stats.test.net.stop()

  def __del__(self):
    del self.test
