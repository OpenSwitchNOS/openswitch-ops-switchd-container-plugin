#!/usr/bin/python
#
# (c) Copyright 2015 Hewlett Packard Enterprise Development LP
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

class myTopo( Topo ):
    '''
                    Custom Topology Example
        H1[h1-eth0]<--->[1]S1[2]<--->[2]S2[1]<--->[h2-eth0]H2
    '''

    def build(self, hsts=2, sws=2, **_opts):
        '''Function to build the custom topology of ''' \
        '''two hosts and two switches'''
        self.hsts = hsts
        self.sws = sws
        #Add list of hosts
        for h in irange( 1, hsts):
            host = self.addHost('h%s' % h)
        #Add list of switches
        for s in irange(1, sws):
            switch = self.addSwitch('s%s' % s)
        #Add links between nodes based on custom topo
        self.addLink('h1', 's1')
        self.addLink('h2', 's2')
        self.addLink('s1', 's2')

class vlanTrunkTest( OpsVsiTest ):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        vlan_topo = myTopo(hsts=2, sws=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(vlan_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def check_config(self):
        '''
            0.1 Add Bridge br0 on Switch1
            0.2 Add VLAN100 to global VLAN table on Switch1
            0.2 Add port 1 with tag 100 on Switch1(access mode)
            0.3 Add port 2 with trunk 100 on Switch1(trunk mode)
            0.4 Verify whether same configuration gets set on the ASIC
                simulating InternalOVS
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n########## Test Case 1 - Verify Correct Driving of",
             "Internal ASIC OVS by OpenSwitch OVS ##########\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        ops_br_name = s1.ovscmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get br " \
                     "br0 datapath_type name")
        ovs_datapath_type, ovs_br_name = out.splitlines()
        assert ovs_datapath_type == 'netdev' \
               and ops_br_name == ovs_br_name, \
               "Bridge configuration mismatch for Dual OVS"
        info("### Dual OVS bridge configuration correctly set ###\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        out = s1.ovscmd("/usr/bin/ovs-vsctl get vlan VLAN100 admin " \
                     "id name")
        ops_admin_state, ops_vlan_id, ops_vlan_name = out.splitlines()
        assert ops_admin_state == 'up' \
               and ops_vlan_id == '100' \
               and ops_vlan_name == 'VLAN100', \
               "VLAN configuration mismatch in OpenSwitch OVS"
        info("### OpenSwitch OVS VLAN configuration correctly set ###\n")


        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 " \
               "vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 " \
               "vlan_mode=trunk trunks=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        time.sleep(1)

        out = s1.ovscmd("/usr/bin/ovs-vsctl get port 1 name tag vlan_mode")
        ops_port_name, ops_tag, ops_vlan_mode = out.splitlines()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 " \
                     "name tag vlan_mode")
        ovs_port_name, ovs_tag, ovs_vlan_mode = out.splitlines()
        assert ops_port_name == ovs_port_name and ops_tag == ovs_tag \
               and ops_vlan_mode == ovs_vlan_mode, \
               "Access port1 configuration mismatch in Dual OVS"
        info("### Dual OVS access port1 configuration correctly set ###\n")

        out = s1.ovscmd("/usr/bin/ovs-vsctl get port 2 name trunks vlan_mode")
        ops_port_name, ops_trunks, ops_vlan_mode = out.splitlines()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 2 " \
                     "name trunks vlan_mode")
        ovs_port_name, ovs_trunks, ovs_vlan_mode = out.splitlines()
        assert ops_port_name == ovs_port_name and ops_tag == ovs_tag \
               and ops_vlan_mode == ovs_vlan_mode, \
               "Trunk port2 configuration mismatch in Dual OVS"
        info("### Dual OVS trunk port2 configuration correctly set ###\n")

        out = s1.ovscmd("/usr/bin/ovs-vsctl get interface 1 admin_state " \
                     "link_state user_config:admin hw_intf_info:mac_addr")
        ops_admin_state, ops_link_state, \
        ops_user_config, ops_mac_addr = out.splitlines()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get interface 1 " \
                     "admin_state mac_in_use")
        ovs_admin_state, ovs_mac_addr = out.splitlines()
        assert ops_admin_state == ovs_admin_state \
               and ops_link_state == 'up' \
               and ops_user_config == 'up' \
               and ops_mac_addr == ovs_mac_addr, \
               "Interface configuration mismatch in Dual OVS"
        info("### Dual OVS interface configuration correctly set ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port br0 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def vlan_normal(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on Switch1 and Switch2
            1.2 Add port 1 with tag 100 on Switch1 and Switch2 (access mode)
            1.3 Add port 2 with trunk 100 on Switch1 and Switch2 (trunk mode)
            1.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n\n\n########## Test Case 2 - Adding VLAN and Ports ##########\n")
        info("### Adding VLAN100 and ports 1,2 to bridge br0.",
             "Port1:access (tag=100), Port2:trunk (trunks=100) ###\n")
        info("### Running ping traffic between hosts: h1, h2 ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

        out = h1.cmd("ping -c1 %s" % h2.IP())

        status = parsePing(out)
        assert status, "Ping Failed even though VLAN and ports were configured correctly"
        info("\n### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def vlan_missing(self):
        '''
            2.1 Dont add VLAN 100 to global VLAN table of Switch1 and Switch2
            2.2 Add port 1 with tag 100 on Switch1 and Switch2 (access mode)
            2.3 Add port 2 with trunk 100 on Switch1 and Switch2 (trunk mode)
            2.4 Test Ping - should not work
            2.5 Add VLAN 100 to global VLAN table of Switch1 and Switch2,
                ports should get reconfigured
            2.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n\n\n########## Test Case 3 - With and Without Global VLAN ##########\n")
        info("### Not Adding Global VLAN ###\n")
        info("### Ports should not get added to the internal ASIC OVS ###\n")
        info("### Ping traffic is expected to fail as the ports",
             "do not get added in the ASIC OVS ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

        out = h1.cmd("ping -c1 %s" % h2.IP())

        status = parsePing(out)
        assert not status, "Ping Success even though global VLAN was missing"
        info("\n### Ping Failed ###\n")

        info("\n\n### Adding Global VLAN ###\n")
        info("### Adding VLAN100. Ports 1,2 should get reconfigured",
             "with tag=100 (port 1) and trunks=100 (port 2) ###\n")
        info("### Ping traffic is expected to pass as the ports",
             "get added in the ASIC OVS ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(1)
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(1)
        out = h1.cmd("ping -c1 %s" % h2.IP())

        status = parsePing(out)
        assert status, "Ping Failed even though global VLAN was added. \
                       Ports are expected to get reconfigured"
        info("\n### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def invalid_trunks(self):
        '''
            3.1 Add VLAN 100 to global VLAN table in Switch1 and Switch2
            3.2 Add port 1 with tag 100 on Switch1 and Switch2 (access mode)
            3.3 Add port 2 with trunk 100 on Switch1 and trunk 200 on Switch2 (trunk mode)
            3.4 Test Ping - should not work
            3.5 Delete port 2 and add port 2 with
                trunk 100 on Switch1 and Switch2 (trunk mode)
            3.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n\n\n########## Test Case 4 - Different and Same",
             "trunks ##########\n")
        info("### Adding Ports with Different Trunks ###\n")
        info("### Adding Switch1->Port2:trunks=100.",
             "Switch2->Port 2:trunks=200 ###\n")
        info("### Ping traffic is expected to fail as the ports",
             "get added with different trunks ###\n\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=200")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

        out = h1.cmd("ping -c1 %s" % h2.IP())

        status = parsePing(out)
        assert not status, \
               "Ping Success even though ports have different trunks"
        info("### Ping Failed ###\n")

        info("\n\n### Changing Trunks Back to Original ###\n")
        info("### Adding Port2 on Switch1 and Switch2 with trunks=100.",
             "Ports should get reconfigured ###\n")
        info("### Ping traffic is expected to pass as the ports",
             "get added with same trunks ###\n")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100")
        s2.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())

        status = parsePing(out)
        assert status, \
               "Ping Failed even though ports on both the switches \
               have same trunks"
        info("\n### Ping Success ###\n\n\n\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

class Test_switchd_container_vlan_trunk:

    def setup_class(cls):
        Test_switchd_container_vlan_trunk.test = vlanTrunkTest()

    # TC_1
    def test_switchd_container_check_config(self):
        self.test.check_config()

    # TC_2
    def test_switchd_container_vlan_normal(self):
        self.test.vlan_normal()

    # TC_3
    def test_switchd_container_vlan_missing(self):
        self.test.vlan_missing()

    # TC_4
    def test_switchd_container_invalid_trunks(self):
        self.test.invalid_trunks()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_vlan_trunk.test.net.stop()

    def __del__(self):
        del self.test
