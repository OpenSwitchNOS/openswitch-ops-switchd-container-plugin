#!/usr/bin/env python
#
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


from os.path import basename
import time
import pytest
from opsvsi.docker import *
from opsvsi.opsvsitest import *
from opsvsiutils.systemutil import *


def checkPing(pingOutput):
        '''
            Parse ping output and check to see if one of the pings
            succeeded or failed
        '''
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


class lagTrunkTest(OpsVsiTest):

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
                the configuration of the Internal ASIC OVS by OpenSwitch OVS
        '''
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        info("\n########## Test Case 1 - Verify Correct Driving of",
             "Internal ASIC OVS by OpenSwitch OVS ##########\n")
        info("### Adding ports 1,lag0(2,3) to VLAN100.",
             "Port 1:access, Port lag0(2,3):trunk ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)

        s1_getbrname_ops = \
            s1.ovscmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        s1_getbrname_openovs, \
            s1_getbrdp_openovs \
            = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get br "
                        "br0 name datapath_type").splitlines()

        assert s1_getbrname_ops == s1_getbrname_openovs \
            and s1_getbrdp_openovs == 'netdev', \
            "Switch1: Bridge configuration mismatch for Dual OVS"
        info("### Switch1: Dual OVS bridge configuration",
             "correctly set ###\n")

        s1_getvlanadmin_ops, s1_getvlanid_ops, s1_getvlanname_ops = \
            s1.ovscmd("/usr/bin/ovs-vsctl get vlan VLAN100 "
                      "admin id name").splitlines()

        assert s1_getvlanadmin_ops == 'up' \
            and s1_getvlanid_ops == '100' \
            and s1_getvlanname_ops == 'VLAN100', \
            "Switch1: OpenSwitch OVS VLAN configuration mismatch"
        info("### Switch1: OpenSwitch OVS VLAN configuration",
             "correctly set ###\n")

        s1_getportname_ops, s1_getporttrunk_ops, \
            s1_getportvlanmode_ops = s1.ovscmd("/usr/bin/ovs-vsctl get \
            port lag0 name trunks vlan_mode").splitlines()
        s1_getportname_openovs, s1_getporttrunk_openovs, \
            s1_getportvlanmode_openovs = \
            s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl "
                      "get port lag0 name trunks vlan_mode").splitlines()

        assert s1_getportname_ops == s1_getportname_openovs \
            and s1_getporttrunk_ops == s1_getporttrunk_openovs \
            and s1_getportvlanmode_ops == s1_getportvlanmode_openovs, \
            "Switch1: Dual OVS bond configuration mismatch"
        info("### Switch1: Dual OVS bond configuration correctly set ###\n")

        s1_getintfadmin_ops, s1_getintfmac_ops, s1_getintflink_ops = \
            s1.ovscmd("/usr/bin/ovs-vsctl get interface 2 admin_state "
                      "hw_intf_info:mac_addr link_state ").splitlines()
        s1_getintfadmin_openovs, s1_getintfmac_openovs, \
            s1_getintflink_openovs = \
            s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl \
            get interface 2 admin_state mac_in_use link_state").splitlines()

        assert s1_getintfadmin_ops == s1_getintfadmin_openovs \
            and s1_getintfmac_ops == s1_getintfmac_openovs \
            and s1_getintflink_ops == s1_getintflink_openovs, \
            "Switch1: Dual OVS interface configuration mismatch"
        info("### Switch1: Dual OVS interface configuration "
             "correctly set ###\n")

        info("\n\n### Deleting VLAN100. Bond is expected to",
             "get deleted from Internal ASIC OVS ###")
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get "
                        "interface 2 admin_state").strip()
        assert "no row 2 in table Interface" in out, \
            "Dual OVS port configuration not deleted even after deleting VLAN"
        info("\n### Switch1: ASIC OVS bond configuration"
             " correctly deleted ###\n")

        info("\n\n### Re-adding VLAN100. Bond is expected to",
             "get reconfigured and added in Internal ASIC OVS ###")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl "
                  "get port lag0 name tag vlan_mode").splitlines()

        assert s1_getportname_ops == s1_getportname_openovs \
            and s1_getporttrunk_ops == s1_getporttrunk_openovs \
            and s1_getportvlanmode_ops == s1_getportvlanmode_openovs, \
            "Switch1: ASIC OVS bond configuration mismatch \
            after re-adding VLAN"
        info("\n### Switch1: ASIC OVS bond configuration correctly "
             "re-set ###\n\n\n\n")
        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def lag_normal(self):
        '''
            2.1 Add VLAN 100 to global VLAN table on Switch1 and Switch2
            2.2 Add port 1 with tag 100 on Switch1 and Switch2
                (access mode)
            2.3 Add port lag0(2,3) with trunk 100 on Switch1 and Switch2
                (trunk mode)
            2.4 Test Ping - should work
        '''
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        info("\n#### Test Case 2 - Ping between two LAGs  #####\n")
        info("### Adding ports 1,lag0(2,3) to VLAN100. Port 1:access,",
             "Port lag0(2,3):trunk ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(11)

        out = h1.cmd("ping -c10 %s" % h2.IP())

        status = checkPing(out)
        assert status, "Ping Failed even though LAG and ports were \
            configured correctly"
        info("\n### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def lag_missing(self):
        '''
            3.1 Don't Add VLAN 100 to global VLAN table on Switch1 and Switch2
            3.2 Add port 1 with tag 100 on Switch1 and Switch2
                (access mode)
            3.3 Add port lag0(2,3) with trunk 100 on Switch1 and Switch2
                (trunk mode)
            3.4 Test Ping - should not work
            3.5 Add VLAN 100 to global VLAN table on Switch1 and Switch2
            3.6 Test Ping - should work
        '''
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        info("\n## Test Case 3 - Ping between two LAGs "
             "with and without global VLAN  ##\n")
        info("### Not adding ports 1,lag0(2,3) to VLAN100. Port 1:access,",
             "Port lag0(2,3):trunk ###\n")
        info("### Ping traffic is expected to fail as the LAG ports",
             "do not get added in the ASIC OVS ###\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(11)
        out = h1.cmd("ping -c5 %s" % h2.IP())

        status = checkPing(out)
        assert not status, "Ping Success even though VLAN100 was missing"
        info("\n### Ping Failed ###\n")

        info("\n\n### Adding Global VLAN ###\n")
        info("### Adding VLAN100. Ports lag0(2,3) should get reconfigured",
             "with trunk=100 ###\n")
        info("### Ping traffic is expected to pass as the ports",
             "get added in the ASIC OVS ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(11)

        out = h1.cmd("ping -c10 %s" % h2.IP())

        status = checkPing(out)
        assert status, "Ping Failed even though VLAN was added. \
                       Ports are expected to get reconfigured"
        info("\n### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def lag_invalid_trunks(self):
        '''
            4.1 Add VLAN 100 to global VLAN table on Switch1 and Switch2
            4.2 Add port 1 with tag 100 on Switch1 and Switch2
                (access mode)
            4.3 Add port lag0(2,3) with trunk 200 on Switch1 and Switch2
                (trunk mode)
            4.4 Test Ping - should not work
            4.5 Delete port lag0(2,3) with trunk 200 on Switch1 and Switch2
                (trunk mode)
            4.6 Add port lag0(2,3) with trunk 100 on Switch1 and Switch2
                (trunk mode)
            4.7 Test Ping - should work
        '''
        s1 = self.net.switches[0]
        s2 = self.net.switches[1]
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]

        info("\n## Test Case 4 - Ping between two LAGs",
             " - Different and same trunks  ##\n")
        info("### Adding ports 1 to VLAN100,lag0(2,3) to VLAN200. ",
             "Port 1:access, Port lag0(2,3):trunk ###\n")
        info("### Ping traffic is expected to fail as the LAG ports",
             "do not get added in the ASIC OVS ###\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s2.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access "
                  "tag=100 -- set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=200 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=200 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(11)
        out = h1.cmd("ping -c5 %s" % h2.IP())

        status = checkPing(out)
        assert not status, "Ping Success even though lag has different trunk"
        info("\n### Ping Failed ###\n")

        info("\n\n### Changing trunk 200 to 100 in port lag0 ###\n")
        info("### Adding trunk 100. Ports lag0(2,3) should get reconfigured",
             "with trunk=100 ###\n")
        info("### Ping traffic is expected to pass as the ports",
             "get added with valid trunks ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        time.sleep(2)
        s1.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(2)
        s2.ovscmd("/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 "
                  "vlan_mode=trunk trunks=100 "
                  "-- set interface 2 user_config:admin=up "
                  "-- set interface 3 user_config:admin=up")
        time.sleep(11)

        out = h1.cmd("ping -c10 %s" % h2.IP())

        status = checkPing(out)
        assert status, "Ping Failed even though a valid trunk was added. \
                       Ports are expected to get reconfigured"
        info("\n### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s2.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-port lag0")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")
        s2.ovscmd("/usr/bin/ovs-vsctl del-br br0")


class Test_switchd_container_lag_trunk:

    def setup_class(cls):
        Test_switchd_container_lag_trunk.test = lagTrunkTest()

    def test_switchd_container_config_check(self):
        self.test.config_check()

    def test_switchd_container_lag_normal(self):
        self.test.lag_normal()

    def test_switchd_container_lag_missing(self):
        self.test.lag_missing()

    def test_switchd_container_lag_invalid_trunks(self):
        self.test.lag_invalid_trunks()

    def teardown_class(cls):
        # Stop the Docker containers, and mininet topology
        Test_switchd_container_lag_trunk.test.net.stop()

    def __del__(self):
        del self.test
