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

import time
from halonvsi.docker import *
from halonvsi.halon import *
from halonutils.halonutil import *

class vlanAccessTest( HalonTest ):

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

    def check_config(self):
        '''
            Check configuration changes in HALON-OvsDB and Sim-OvsDB for proper values
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 1 - Check configuration between Halon-OvsDB "\
             "and Sim-OvsDB ###################\n")
        info('\nTesting to check if configuration is as expected in the Halon-OvsDB and Sim-OvsDB\n')

        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        halon_br_name = s1.cmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        out = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get br br0 datapath_type name")
        ovs_datapath_type, ovs_br_name = out.splitlines()
        assert ovs_datapath_type == 'netdev' and halon_br_name == ovs_br_name, \
               "Unexpected configuration in Bridge"
        info("\nBridge configuration matches in Halon-OvsDB and SIM-OvsDB.\n")

        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        out = s1.cmd("/usr/bin/ovs-vsctl get vlan VLAN100 admin oper_state oper_state_reason")
        halon_admin_state, halon_oper_state, halon_oper_state_reason = out.splitlines()
        assert halon_admin_state == 'up' and halon_oper_state == 'down' \
               and halon_oper_state_reason == 'no_member_port', "Halon-OvsDB VLAN configuration "\
               "before adding port is wrong!"
        info("\nHalon-OvsDB VLAN configuration before adding port is as expected\n")

        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        time.sleep(1)

        out = s1.cmd("/usr/bin/ovs-vsctl get vlan VLAN100 oper_state oper_state_reason")
        halon_oper_state, halon_oper_state_reason = out.splitlines()
        assert halon_oper_state == 'up' and halon_oper_state_reason == 'ok', \
               "Halon-OvsDB VLAN configuration after adding port is wrong!"
        info("\nHalon-OvsDB VLAN configuration after adding port is as expected\n")

        out = s1.cmd("/usr/bin/ovs-vsctl get port 1 name tag vlan_mode")
        halon_port_name, halon_tag, halon_vlan_mode = out.splitlines()
        out = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get port 1 name tag vlan_mode")
        ovs_port_name, ovs_tag, ovs_vlan_mode = out.splitlines()
        assert halon_port_name == ovs_port_name and halon_tag == ovs_tag \
               and halon_vlan_mode == ovs_vlan_mode, "Mismatch in access port configuration "\
               "between Halon-OvsDB and OVS-OvsDB"
        info("\nHalon-OvsDB access port configuration matches the Sim-OvsDB access port configuration\n")

        out = s1.cmd("/usr/bin/ovs-vsctl get interface 1 admin_state link_state user_config:admin hw_intf_info:mac_addr")
        halon_admin_state, halon_link_state, halon_user_config, halon_mac_addr = out.splitlines()
        out = s1.cmd("/opt/openvswitch/bin/ovs-vsctl get interface 1 admin_state mac_in_use")
        ovs_admin_state, ovs_mac_addr = out.splitlines()
        assert halon_admin_state == ovs_admin_state and halon_link_state == 'up' \
               and halon_user_config == 'up' and halon_mac_addr == ovs_mac_addr, \
               "Mismatch in interface configuration between Halon-OvsDB and Sim-OvsDB"
        info("\nHalon-OvsDB interface configuration matches the Sim-OvsDB interface configuration\n")

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")

    def vlan_normal(self):
        '''
            1.1 Add VLAN 100 to global VLAN table
            1.2 Add port 1 with tag 100
            1.3 Add port 2 with tag 100
            1.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 2 - Normal VLAN access mode operation ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100 -- set interface 2 user_config:admin=up")

        info("\n\nThis test will test the normal VLAN access mode operation by making required configurations\n\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though VLAN was configured correctly"

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port br0 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")

    def vlan_missing(self):
        '''
            2.1 Dont add VLAN 100 to global VLAN table
            2.2 Add port 1 with tag 100
            2.3 Add port 2 with tag 100
            2.4 Test Ping - should not work
            2.5 Add VLAN 100 to global VLAN table
            2.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 3 - Without Global VLAN ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100 -- set interface 2 user_config:admin=up")

        info("\n\nTesting if ping fails when the VLAN is not present in the global VLAN table\n\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            assert 0, "Ping Success even though global VLAN was missing"
        else:
            info("\nPing Failed\n")

        info("\n################### Adding Global VLAN ###################\n")
        info("\n### Adding VLAN100. Ports 1,2 get reconfigured with tag=100 ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        time.sleep(1)
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though global VLAN was configured properly"

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port br0 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")

    def invalid_tags(self):
        '''
            3.1 Add VLAN 100 to global VLAN table
            3.2 Add port 1 with tag 100
            3.3 Add port 2 with tag 200
            3.4 Test Ping - should not work
            3.5 Delete port 2 and add port 2 with tag 100
            3.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n################### Test Case 4 - VLAN access ports With Different Tags ###################\n")
        s1.cmd("/usr/bin/ovs-vsctl add-br br0")
        s1.cmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100 -- set interface 1 user_config:admin=up")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=200 -- set interface 2 user_config:admin=up")

        info("\n\nTesting if ping fails when the VLAN access ports have different tags ex: 100 & 200\n\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            assert 0, "Ping Success even though different tags on VLAN access ports"
        else:
            info("\nPing Failed\n")

        info("\n################### Changing Tags Back to Same Config ###################\n")
        info("\n### Adding Ports 1,2 with tag=100. Ports get reconfigured ###\n\n")
        s1.cmd("/usr/bin/ovs-vsctl del-port 2")
        s1.cmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100 -- set interface 2 user_config:admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        if status:
            info("\nPing Success\n")
        else:
            assert 0, "Ping Failed even though VLAN access ports had the same tag"

        #Cleanup before next test
        s1.cmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.cmd("/usr/bin/ovs-vsctl del-port 1")
        s1.cmd("/usr/bin/ovs-vsctl del-port 2")
        s1.cmd("/usr/bin/ovs-vsctl del-br br0")

class Test_ovs_sim_vlan_access:

    def setup(self):
        pass

    def teardown(self):
        pass

    def setup_class(cls):
        Test_ovs_sim_vlan_access.test = vlanAccessTest()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_ovs_sim_vlan_access.test.net.stop()

    def setup_method(self, method):
        pass

    def teardown_method(self, method):
        pass

    def __del__(self):
        del self.test

    # TC_1
    def test_ovs_sim_check_config(self):
        self.test.check_config()

    # TC_2
    def test_ovs_sim_vlan_normal(self):
        self.test.vlan_normal()

    # TC_3
    def test_ovs_sim_vlan_missing(self):
        self.test.vlan_missing()

    # TC_4
    def test_ovs_sim_invalid_tags(self):
        self.test.invalid_tags()
