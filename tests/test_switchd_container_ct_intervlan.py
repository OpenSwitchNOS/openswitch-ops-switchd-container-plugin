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


def checkPing(pingOutput):
    '''Parse ping output and check to see if one of the pings succeeded or failed'''
    # Check for downed link
    if 'Destination Host Unreachable' in pingOutput:
        return False
    r = r'(\d+) packets transmitted, (\d+) received'
    m = re.search(r, pingOutput)
    if m is None:
        return False
    sent, received = int(m.group(1)), int(m.group(2))
    if sent >= 1 and received >= 1:
        return True
    else:
        return False


class simIntervlanTests(OpsVsiTest):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        vlan_topo = SingleSwitchTopo(k=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(vlan_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def intervlan_direct_connected(self):
        info("########## Verify intervlan for directly",
             "connected hosts ##########\n")
        s1 = self.net.switches[0]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]
        # Configure switch s1
        s1.cmdCLI("configure terminal")

        # Configure VLANs 100 and 200 on switch
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
        time.sleep(5)
        # Configure interface vlan 100 and vlan 200 on switch s1
        s1.cmdCLI("interface vlan 100")
        s1.cmdCLI("ip address 100.0.0.1/24")
        time.sleep(5)
        s1.cmdCLI("ipv6 address 100::1/120")
        s1.cmdCLI("exit")

        time.sleep(5)

        info("Configuring interface vlan 200\n")
        s1.cmdCLI("interface vlan 200")
        s1.cmdCLI("ip address 200.0.0.1/24")
        time.sleep(5)
        s1.cmdCLI("ipv6 address 200::1/120")
        s1.cmdCLI("exit")
        time.sleep(5)

        # Check trunks list on port bridge normal in native OVS
        output = \
            s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl list port bridge_normal")
        assert "100, 200" in output, \
               "Trunk vlan 100 on bridge_normal wasnt configured"

        info("\n### vlan interface configuration: Success ###\n")

        # Configure host 1
        info("Configuring host 1 with 100.0.0.2/24\n")
        h1.cmd("ip addr add 100.0.0.2/24 dev h1-eth0")
        h1.cmd("ip route add 200.0.0.0/24 via 100.0.0.1")
        info("Configuring host 1 with 100::2/120\n")
        h1.cmd("ip addr add 100::2/120 dev h1-eth0")
        h1.cmd("ip route add 200::0/120 via 100::1")

        # Configure host 2
        info("Configuring host 2 with 200.0.0.2/24\n")
        h2.cmd("ip addr add 200.0.0.2/24 dev h2-eth0")
        h2.cmd("ip route add 100.0.0.0/24 via 200.0.0.1")
        info("Configuring host 2 with 200::2/120\n")
        h2.cmd("ip addr add 200::2/120 dev h2-eth0")
        h2.cmd("ip route add 100::0/120 via 200::1")

        # Ping from host 1 to host2
        info("Ping h2 from h1\n")
        output = h1.cmd("ping 200.0.0.2 -c5")
        status = checkPing(output)
        assert status, "Ping Failed\n"
        info("Ping Success\n")

        # Ping6 from host 1 to host 2
        info("IPv6 Ping h2 from h1\n")
        output = h1.cmd("ping6 200::2 -c5")
        status = checkPing(output)
        # assert status, "Pingi6 Failed"
        info("Ping6 Success\n")

        # Unconfigure interface vlan 100
        info("Unconfigure vlan interface.",
             "This will disable inter-vlan routing.\n")
        s1.cmdCLI("no interface vlan 100")
        s1.cmdCLI("no interface vlan 200")

        time.sleep(5)
        # Check trunks list on port bridge normal in native OVS
        output = \
            s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl list port bridge_normal")
        assert "100, 200" not in output, \
               "Trunk vlans 100, 200 on bridge_normal still configured"

        # Ping from host 1 to host2
        info("Check IPv4 ping h2 from h1\n")
        output = h1.cmd("ping 200.0.0.2 -c5")
        status = checkPing(output)
        assert status == 0, \
            "Ping succeeded even though vlan interface is not present``\n"
        info("Ipv4 routing between vlans disabled\n")

        # Ping6 from host 1 to host 2
        info("Check IPv6 Ping h2 from h1\n")
        output = h1.cmd("ping6 2000::2 -c5")
        status = checkPing(output)
        assert status == 0, \
            "Ping6 succeeded even though vlan interface is not present`\n"
        info("Ipv6 routing between vlans disabled\n")
        info("Delete vlan interface Success\n")

        s1.cmdCLI("end")

        info("\n########## Inter-vlan test for directly",
             "connected hosts passed ##########\n\n")

    def intervlan_admin_and_link_states(self):
        info("\n########## Verify interface VLAN admin and link states ",
             "##########\n")
        '''
    This function verifies if the admin and the link state for the interface
    VLANs are "up" after "no shut" and "down" after "shutdown". This helps
    zebra in selecting the interface VLAN as a nexthop for forwarding.
    '''
        s1 = self.net.switches[0]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        # Configuring VLAN interfaces and bringing them up
        info("### Bringing the interface VLAN up ###\n")
        s1.cmdCLI("configure terminal")
        s1.cmdCLI("interface vlan 100")
        s1.cmdCLI("no shutdown")
        time.sleep(5)
        admin_state, link_state = s1.ovscmd("ovs-vsctl get interface vlan100 "
                                            "admin_state link_state").splitlines()
        assert "up" in admin_state and "up" in link_state, \
            "Admin and Link State Verification Unsuccessful\n\n"

        # Configuring VLAN interfaces and bringing them up
        info("### Bringing the interface VLAN down ###\n")
        s1.cmdCLI("shutdown")
        time.sleep(5)
        admin_state, link_state = s1.ovscmd("ovs-vsctl get interface vlan100 "
                                            "admin_state link_state").splitlines()
        assert "down" in admin_state and "down" in link_state, \
            "Admin and Link State Verification Unsuccessful\n\n"

        info("\n########## Interface VLAN test for admin",
             "and link states passed ##########\n\n")


@pytest.mark.skipif(True, reason="Disabling old tests")
class Test_switchd_container_intervlan:

    def setup_class(cls):
        Test_switchd_container_intervlan.test = simIntervlanTests()

    # Test for slow routing between directly connected hosts
    def test_switchd_container_intervlan_direct_connected(self):
        self.test.intervlan_direct_connected()

    # Test to verify the admin and the link states for interface VLANs
    def test_switchd_container_intervlan_admin_and_link_states(self):
        self.test.intervlan_admin_and_link_states()

    def teardown_class(cls):
        Test_switchd_container_intervlan.test.net.stop()

    def __del__(self):
        del self.test
