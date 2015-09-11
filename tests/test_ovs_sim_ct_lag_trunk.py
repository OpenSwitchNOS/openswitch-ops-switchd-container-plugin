#!/usr/bin/python

#    Copyright (C) {2015} Hewlett Packard Enterprise Development LP
#    All Rights Reserved.

#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at

#        http://www.apache.org/licenses/LICENSE-2.0

#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.


import os
import sys
import time
import pytest
import subprocess
from halonvsi.docker import *
from halonvsi.halon import *
from halonutils.halonutil import *

class myTopo( Topo ):
    '''
        Custom Topology Example
        H1[h1-eth0]<--->[1]S1[2]<--->[2]S2[1]<--->[h2-eth0]H2
    '''

    def build(self, hsts=2, sws=2, **_opts):
        self.hsts = hsts
        self.sws = sws

        # Add list of hosts
        for h in irange( 1, hsts):
            host = self.addHost( 'h%s' % h)

        # Add list of switches
        for s in irange(1, sws):
            switch = self.addSwitch( 's%s' %s)

        # Add links between nodes based on custom topo
        self.addLink('h1', 's1')
        self.addLink('h2', 's2')
        self.addLink('s1', 's2')
        self.addLink('s1', 's2')

class lagTrunkTest( HalonTest ):

    def setupNet(self):
        # if you override this function, make sure to
        # either pass getNodeOpts() into hopts/sopts of the topology that
        # you build or into addHost/addSwitch calls
        self.net = Mininet(topo=myTopo(hsts=2, sws=2,
                                       hopts=self.getHostOpts(),
                                       sopts=self.getSwitchOpts()),
                                       switch=HalonSwitch,
                                       host=HalonHost,
                                       link=HalonLink, controller=None,
                                       build=True)

    def config_check(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on S1 and S2
            1.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            1.3 Add port lag0(2,3) with trunk 100 on S1 and S2 (trunk mode)
            1.4 Check the bridge, VLAN, port and interface tables for both the OVS' and verify
                correct setting up of the configuration of the OpenSource OVS by Halon OVS
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test To Verify Correct Driving of OpenSource OVS by Halon OVS  ###################\n")
        info("\n### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access, Port lag0(2,3):trunk ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")

        # HALON_TODO: Once pm_info is fixed through the code, remove the additional setting of pm_info paramters throughout the script
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)

        s1_getbrname_halon = s1.cmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        s1_getbrname_openovs, s1_getbrdp_openovs = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get br br0 name datapath_type").splitlines()

        s1_getbrname_halon = s1.cmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        s1_getbrname_openovs, s1_getbrdp_openovs = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get br br0 name datapath_type").splitlines()

        assert s1_getbrname_halon == s1_getbrname_openovs and s1_getbrdp_openovs == 'netdev', \
            "Switch1: Bridge configuration mismatch for Dual OVS"
        info("\nSwitch1: Dual OVS' bridge configuration correctly set\n")

        s1_getvlanadmin_halon, s1_getvlanid_halon, s1_getvlanname_halon = s1.cmd("/usr/bin/ovs-vsctl get vlan VLAN100 admin id name").splitlines()

        assert s1_getvlanadmin_halon == 'up' and s1_getvlanid_halon == '100' and s1_getvlanname_halon == '"VLAN100"', \
            "Switch1: HALON OVS VLAN configuration mismatch"
        info("Switch1: Halon OVS' bridge configuration correctly set")

        s1_getportname_halon, s1_getporttrunk_halon, s1_getportvlanmode_halon = s1.cmd("/usr/bin/ovs-vsctl get port lag0 name trunks vlan_mode").splitlines()
        s1_getportname_openovs, s1_getporttrunk_openovs, s1_getportvlanmode_openovs = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get port lag0 name trunks vlan_mode").splitlines()

        assert s1_getportname_halon == s1_getportname_openovs and s1_getporttrunk_halon == s1_getporttrunk_openovs and s1_getportvlanmode_halon == s1_getportvlanmode_openovsi, \
            "Switch1: Dual OVS' bond configuration mismatch"
        info("\nSwitch1: Dual OVS' bond configuration correctly set\n")

        s1_getintfadmin_halon, s1_getintfmac_halon, s1_getintflink_halon = s1.cmd("/usr/bin/ovs-vsctl get interface 2 admin_state hw_intf_info:mac_addr link_state ").splitlines()
        s1_getintfadmin_openovs, s1_getintfmac_openovs, s1_getintflink_openovs = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get interface 2 admin_state mac_in_use link_state").splitlines()

        assert s1_getintfadmin_halon == s1_getintfadmin_openovs and s1_getintfmac_halon == s1_getintfmac_openovs and s1_getintflink_halon == s1_getintflink_openovs, \
            "Switch1: Dual OVS' interface configuration mismatch"
        info("\nSwitch1: Dual OVS' interface configuration correctly set\n")

        # HALON_TODO: LAG tests require extra test cases where we set the bond configuration on the fly. For eg: Bringing one of the
        # interfaces link down from the bond and verify the bond configuration. Currently, the bond configuration does not support
        # change of port/bond configuration dynamically. Once, the VSI provider codes have this functionality, more test cases will be added

        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

    def add_ports_and_vlan(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on S1 and S2
            1.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            1.3 Add port lag0(2,3) with trunk 100 on S1 and S2 (trunk mode)
            1.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 1 - Adding VLAN and Ports  ###################\n")
        info("\n### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access, Port lag0(2,3):trunk ###\n")
        info("### Adding VLAN100 to the VLAN table and adding ports1, lag(2,3) to bridge: br0\n")
        info("    in access mode and running ping traffic between the hosts: h1, h2 ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)
        status = parsePing(out)
        if status:
            info("Ping Success\n")
        else:
            info("Ping Failed\n")
            assert 0, "Failed to transmit packets when LAG is in trunk mode and port is in access mode"

        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

    def del_and_readd_vlan(self):
        '''
            2.1 Dont add VLAN 100 to global VLAN table of S1 and S2
            2.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            2.3 Add port lag0(2,3) with trunk 100 on S1 and S2 (trunk mode)
            2.4 Test Ping - should not work
            2.5 Add VLAN 100 to global VLAN table of S1 and S2
            2.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 2 - Without Global VLAN ###################\n")
        info("\n### Deleting VLAN100 from the VLAN table. Ports get deleted from the OpenSource OVS ###\n")
        info("### Ping traffic is expected to fail as the ports get deleted from the OpenSource OVS ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")

        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        time.sleep(2)

        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)
        status = parsePing(out)
        if status:
            info("Ping Success\n")
            assert 0, "After deleting VLAN, OpenSource OVS ports are expected to get reconfigured/deleted. Successful ping implies test failure"
        else:
            info("Ping Failed\n")
            info("Failure in transmitting packets implies successful test execution")


        info("\n################### Adding Global VLAN ###################\n")
        info("\n### Adding VLAN100. Ports 1,lag0(2,3) get reconfigured with trunks=100 ###\n")
        info("### Ping traffic is expected to pass as the ports get re-added in the OpenSource OVS ###\n\n")

        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(2)

        # HALON_TODO: Remove the explicit driving of OpenSource OVs once the LAG state info is taken care of
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100")
        time.sleep(2)
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)
        status = parsePing(out)
        if status:
            info("Ping Success\n")
        else:
            info("Ping Failed\n")
            assert 0, "Failed to transmit packets even after readdition of VLAN. Port and LAG are expected to get reconfigured"

        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")


    def diff_and_same_tags(self):
        '''
            3.1 Add VLAN 100 to global VLAN table in S1 and S2
            3.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            3.3 Add port lag0(2,3) with trunk 200 on S1 and S2 (trunk mode)
            3.4 Test Ping - should not work
            3.5 Delete port lag0(2,3) and add port lag0(2,3) with trunk 100 on S1 and S2 (trunk mode)
            3.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 3 - With Different Trunks ###################\n")
        info("\n### Adding S1->Port 1:trunks=100. S2->Port lag0(2,3):trunks=200 ###\n")
        info("### Ping traffic is expected to fail as the ports get added with different trunks ###\n\n")

        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 3 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=200 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=200")
        time.sleep(2)
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)
        status = parsePing(out)
        if status:
            info("Ping Success\n")
            assert 0, "LAG ports on both the switch have different trunks. Successful ping implies test failure"
        else:
            info("Ping Failed\n")
            info("Failure in transmitting packets implies successful test execution")


        info("\n################### Changing Tags Back to Original ###################\n")
        info("\n### Adding Port lag0 in S1, S2 with trunks=100. Ports get reconfigured ###\n")
        info("### Ping traffic is expected to pass as the ports get re-added to the OpenSource OVS with same trunks ###\n\n")

        s1.cmd("/usr/bin/ovs-vsctl del-port lag0")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl del-port lag0")
        time.sleep(2)
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl del-port br0 lag0")
        time.sleep(2)
        s1.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100")
        time.sleep(2)
        s2.cmd("/opt/openvswitch/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)
        status = parsePing(out)
        if status:
            info("Ping Success\n")
        else:
            info("Ping Failed\n")
            assert 0, "LAG ports on both the switch have same trunks. Packets should get transmitted. Test executuon failure"

        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

@pytest.mark.skipif(True, reason="does not work")
class Test_ovs_sim_lag_trunk:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_ovs_sim_lag_trunk.test = lagTrunkTest()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_ovs_sim_lag_trunk.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    def test1(self):
        self.test.config_check()
        #CLI(self.test.net)        # Uncomment the line if you want to get to mininet prompt for debugging

    def test2(self):
        self.test.add_ports_and_vlan()
        #CLI(self.test.net)        # Uncomment the line if you want to get to mininet prompt for debugging

    def test3(self):
        self.test.del_and_readd_vlan()
        #CLI(self.test.net)        # Uncomment the line if you want to get to mininet prompt for debugging

    def test4(self):
        self.test.diff_and_same_tags()
        #CLI(self.test.net)        # Uncomment the line if you want to get to mininet prompt for debugging
