#!/usr/bin/python

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
        '''Function to build the custom topology of two hosts and two switches'''
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

class vlanTrunkTest( HalonTest ):

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

    def vlan_normal(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on S1 and S2
            1.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            1.3 Add port 2 with trunk 100 on S1 and S2 (trunk mode)
            1.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 1 - Normal VLAN trunk mode operation  ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")

        # HALON_TODO: Soon we will have a mechanism to tell PMD via ovs-appctl to simulate a Pluggable module.
        # Once we have that mechanism we need to remove this hard wired modifications to Interface table shown in the 4 lines below.
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")

        info("\nThis test will test the normal VLAN trunk mode operation by making required configurations\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port 1")
        s2.cmd("/usr/bin/ovs-vsctl del-port 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port 2")
        s2.cmd("/usr/bin/ovs-vsctl del-port 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though VLAN was configured correctly"

    def vlan_missing(self):
        '''
            2.1 Dont add VLAN 100 to global VLAN table of S1 and S2
            2.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            2.3 Add port 2 with trunk 100 on S1 and S2 (trunk mode)
            2.4 Test Ping - should not work
            2.5 Add VLAN 100 to global VLAN table of S1 and S2
            2.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 2 - Without Global VLAN ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")

        info("\nTesting if ping fails when the VLAN is not present in the global VLAN table\n\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            assert 0, "Ping Success even though global VLAN was missing"
        else:
            info("\nPing Failed\n")

        info("\n################### Adding Global VLAN ###################\n")
        info("\n### Adding VLAN100. Ports 1,2 get reconfigured for trunks=100 ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port 1")
        s2.cmd("/usr/bin/ovs-vsctl del-port 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port 2")
        s2.cmd("/usr/bin/ovs-vsctl del-port 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though global VLAN was configured properly"

    def invalid_trunks(self):
        '''
            3.1 Add VLAN 100 to global VLAN table in S1 and S2
            3.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            3.3 Add port 2 with trunk 200 on S1 and S2 (trunk mode)
            3.4 Test Ping - should not work
            3.5 Delete port 2 and add port 2 with trunk 100 on S1 and S2 (trunk mode)
            3.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 3 - VLAN trunk ports With Different Trunks ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=200 -- set interface 2 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 1 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s1.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")
        s2.cmd("/usr/bin/ovs-vsctl set interface 2 pm_info:connector=SFP_RJ45 pm_info:connector_status=supported")

        info("\n\nTesting if ping fails when the VLAN trunk ports have different trunks ex: 100 & 200\n\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            assert 0, "Ping Success even though different trunks on VLAN trunk ports"
        else:
            info("Ping Failed\n")

        info("\n################### Changing Trunks Back to Original ###################\n")
        info("\n### Adding Ports 2 on S1, S2 with trunks=100. Ports get reconfigured ###\n\n")
        s2.cmd("/usr/bin/ovs-vsctl del-port 2")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100 -- set interface 2 user_config:admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port 1")
        s2.cmd("/usr/bin/ovs-vsctl del-port 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port 2")
        s2.cmd("/usr/bin/ovs-vsctl del-port 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")
        s2.cmd("/usr/bin/ovs-vsctl del-br br0")

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though VLAN trunk ports had the same trunk"

class Test_ovs_sim_vlan_trunk:

    # Create the Mininet topology based on mininet.
    test = vlanTrunkTest()

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        pass

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_ovs_sim_vlan_trunk.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    # TC_1
    def test_ovs_sim_vlan_normal(self):
        self.test.vlan_normal()

    # TC_2
    def test_ovs_sim_vlan_missing(self):
        self.test.vlan_missing()

    # TC_3
    def test_ovs_sim_invalid_trunks(self):
        self.test.invalid_trunks()
