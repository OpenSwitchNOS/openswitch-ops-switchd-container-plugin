#!/usr/bin/python

#    Copyright (C) {2015} Hewlett Packard Enterprise Development LP
#    All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
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
import re
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
        if sent >= 1 and received >=1:
            return True
        else:
            return False

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

class lagAccessTest( OpsVsiTest ):

    def setupNet(self):
        # if you override this function, make sure to
        # either pass getNodeOpts() into hopts/sopts of the topology that
        # you build or into addHost/addSwitch calls
        #self.net = Mininet(topo=myTopo(hsts=2, sws=2,
        #                               hopts=self.getHostOpts(),
        #                               sopts=self.getSwitchOpts()),
        #                               switch=HalonSwitch,
        #                               host=HalonHost,
        #                               link=HalonLink, controller=None,
        #                               build=True)
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lag_topo = myTopo(hsts=2, sws=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(lag_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def config_check(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on S1 and S2
            1.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            1.3 Add port lag0(2,3) with tag 100 on S1 and S2 (access mode)
            1.4 Check the bridge, VLAN, port and interface tables for both the OVS' and verify
                correct setting up of the configuration of OpenVswitch by OpenSwitch
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]

        info("\n################### Test To Verify Correct Driving of OVS by OpenSwitch  ###################\n")
        info("\n### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access, Port lag0(2,3):access ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        time.sleep(1)
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        time.sleep(1)
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=access tag=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=access tag=100 -- set interface 2 user_config:admin=up -- set interface 3 user_config:admin=up")
        time.sleep(2)


    def add_ports_and_vlan(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on S1 and S2
            1.2 Add port 1 with tag 100 on S1 and S2 (access mode)
            1.3 Add port lag0(2,3) with tag 100 on S1 and S2 (access mode)
            1.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 1 - Adding VLAN and Ports  ###################\n")
        info("\n### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access, Port lag0(2,3):access ###\n")
        info("### Adding VLAN100 to the VLAN table and adding ports1, lag(2,3) to bridge: br0\n")
        info("    in access mode and running ping traffic between the hosts: h1, h2 ###\n\n")
        time.sleep(9)
        out = h1.cmd("ping -c9 %s" % h2.IP())
        status = checkPing(out)
        if status:
            info("Ping Success\n")
        else:
            info("Ping Failed\n")
            assert 0, "Failed to transmit packets when LAG and port are in access mode"
            CLI(self.net)

class Test_ovs_sim_lag_access:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_ovs_sim_lag_access.test = lagAccessTest()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_ovs_sim_lag_access.test.net.stop()

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
        print
