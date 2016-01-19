#!/usr/bin/python

#    Copyright (C) {2016} Hewlett Packard Enterprise Development LP
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


class myTopo(Topo):
    '''
        Custom Topology Example
        H1[h1-eth0]<--->[1]S1[2]<--->[2]S2[1]<--->[h2-eth0]H2
    '''

    def build(self, hsts=2, sws=2, **_opts):
        self.hsts = hsts
        self.sws = sws

        # Add list of hosts
        for h in irange(1, hsts):
            host = self.addHost('h%s' % h)

        # Add list of switches
        for s in irange(1, sws):
            switch = self.addSwitch('s%s' % s)

        # Add links between nodes based on custom topo
        self.addLink('h1', 's1')
        self.addLink('h2', 's2')
        self.addLink('s1', 's2')
        self.addLink('s1', 's2')


class lagAccessTest(OpsVsiTest):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lag_topo = myTopo(hsts=2, sws=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(lag_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def lag_ping(self):

        s1 = self.net.switches[0]
        s2 = self.net.switches[1]

        info("\n#### Test Case 1 - Ping between two LAGs  #####\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        time.sleep(1)
        s2.cmd("/usr/bin/ovs-vsctl add-br br0")
        time.sleep(1)
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access \
               tag=100 -- set interface 1 user_config:admin=up")
        s2.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access \
               tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=access \
               tag=100 -- set interface 2 user_config:admin=up -- set \
               interface 3 user_config:admin=up")
        time.sleep(2)
        s2.cmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=access \
               tag=100 -- set interface 2 user_config:admin=up -- set \
               interface 3 user_config:admin=up")
        time.sleep(2)

        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        time.sleep(9)
        out = h1.cmd("ping -c9 %s" % h2.IP())
        status = checkPing(out)
        if status:
            info("Ping Success\n")
        else:
            info("Ping Failed\n")
            assert 0, "Failed to transmit packets"


class Test_ovs_sim_lag_access:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_ovs_sim_lag_access.test = lagAccessTest()

    def teardown_class(cls):
        Test_ovs_sim_lag_access.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    def test1(self):
        self.test.lag_ping()
