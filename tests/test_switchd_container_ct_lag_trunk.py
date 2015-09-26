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


from os.path import basename
import time
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

class lagTrunkTest( OpsVsiTest ):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        lag_topo = myTopo(hsts=2, sws=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(lag_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def config_check(self):
        '''
            1.1 Add VLAN 100 to global VLAN table on Switch1 and Switch2
            1.2 Add port 1 with tag 100 on Switch1 and Switch2 (access mode)
            1.3 Add port lag0(2,3) with trunk 100 on Switch1 and Switch2
                (trunk mode)
            1.4 Check the bridge, VLAN, port and interface tables
                for both the OVS' and verify correct setting up of
                the configuration of the SIM OVS by OpenSwitch OVS
        '''
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n########## Test Case 1 - Verify Correct Driving of",
             "SIM OVS by OpenSwitch OVS ##########\n")
        info("### Adding ports 1,lag0(2,3) to VLAN100.",
             "Port 1:access, Port lag0(2,3):trunk ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access " \
            "tag=100 set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access " \
            "tag=100 set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 " \
               "vlan_mode=trunk trunks=100 " \
               "-- set interface 2 user_config:admin=up " \
               "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 " \
               "vlan_mode=trunk trunks=100 " \
               "-- set interface 2 user_config:admin=up " \
               "-- set interface 3 user_config:admin=up")
        time.sleep(2)

        s1_getbrname_ops = \
            s1.ovscmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        s1_getbrname_openovs, s1_getbrdp_openovs = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get br br0 " \
            "name datapath_type").splitlines()

        s1_getbrname_ops = \
            s1.ovscmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        s1_getbrname_openovs, s1_getbrdp_openovs = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get br br0 " \
            "name datapath_type").splitlines()

        assert s1_getbrname_ops == s1_getbrname_openovs \
               and s1_getbrdp_openovs == 'netdev', \
               "Switch1: Bridge configuration mismatch for Dual OVS"
        info("### Switch1: Dual OVS bridge configuration",
             "correctly set ###\n")

        s1_getvlanadmin_ops, s1_getvlanid_ops, \
        s1_getvlanname_ops = s1.ovscmd("/usr/bin/ovs-vsctl get vlan " \
            "VLAN100 admin id name").splitlines()

        assert s1_getvlanadmin_ops == 'up' \
               and s1_getvlanid_ops == '100' \
               and s1_getvlanname_ops == 'VLAN100', \
               "Switch1: OpenSwitch OVS VLAN configuration mismatch"
        info("### Switch1: OpenSwitch OVS VLAN configuration",
             "correctly set ###\n")

        s1_getportname_ops, s1_getporttrunk_ops, \
        s1_getportvlanmode_ops = s1.ovscmd("/usr/bin/ovs-vsctl get " \
            "port lag0 name trunks vlan_mode").splitlines()
        s1_getportname_openovs, s1_getporttrunk_openovs, \
        s1_getportvlanmode_openovs = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
            "get port lag0 name trunks vlan_mode").splitlines()

        assert s1_getportname_ops == s1_getportname_openovs \
               and s1_getporttrunk_ops == s1_getporttrunk_openovs \
               and s1_getportvlanmode_ops == s1_getportvlanmode_openovs, \
               "Switch1: Dual OVS bond configuration mismatch"
        info("### Switch1: Dual OVS bond configuration correctly set ###\n")

        s1_getintfadmin_ops, s1_getintfmac_ops, s1_getintflink_ops = \
        s1.ovscmd("/usr/bin/ovs-vsctl get interface 2 admin_state " \
            "hw_intf_info:mac_addr link_state ").splitlines()
        s1_getintfadmin_openovs, s1_getintfmac_openovs, s1_getintflink_openovs = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
            "get interface 2 admin_state mac_in_use link_state").splitlines()

        assert s1_getintfadmin_ops == s1_getintfadmin_openovs \
               and s1_getintfmac_ops == s1_getintfmac_openovs \
               and s1_getintflink_ops == s1_getintflink_openovs, \
               "Switch1: Dual OVS interface configuration mismatch"
        info("### Switch1: Dual OVS interface configuration correctly set ###\n")

        info("\n\n### Deleting VLAN100. Bond is expected to",
             "get deleted from Internal ASIC OVS ###")
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get " \
        "interface 2 admin_state").strip()
        assert "no row 2 in table Interface" in out, "Dual OVS port configuration \
                not deleted even after deleting VLAN"
        info("\n### Switch1: ASIC OVS bond configuration correctly deleted ###\n")

        info("\n\n### Re-adding VLAN100. Bond is expected to",
             "get reconfigured and added in Internal ASIC OVS ###")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
            "get port lag0 name tag vlan_mode").splitlines()

        assert s1_getportname_ops == s1_getportname_openovs \
               and s1_getporttrunk_ops == s1_getporttrunk_openovs \
               and s1_getportvlanmode_ops == s1_getportvlanmode_openovs, \
               "Switch1: ASIC OVS bond configuration mismatch after re-adding VLAN"
        info("\n### Switch1: ASIC OVS bond configuration correctly re-set ###\n\n\n\n")
        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")


        # Note: Once we support Static LAGs in ASIC OVS, more
        # test cases can be added.

class Test_switchd_container_lag_trunk:

    def setup_class(cls):
        Test_switchd_container_lag_trunk.test = lagTrunkTest()

    def test_switchd_container_config_check(self):
        self.test.config_check()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_lag_trunk.test.net.stop()

    def __del__(self):
        del self.test
