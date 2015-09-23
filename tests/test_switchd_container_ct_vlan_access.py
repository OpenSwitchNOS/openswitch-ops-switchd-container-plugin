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
from opsvsi.docker import *
from opsvsi.opsvsitest import *
from opsvsiutils.systemutil import *

class vlanAccessTest( OpsVsiTest ):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        vlan_topo = SingleSwitchTopo(k=2, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(vlan_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def check_config(self):
        '''Check configuration changes in OpenSwitch-OvsDB and ''' \
        '''Sim-OvsDB for proper values'''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n########## Test Case 1 - Check configuration between",
             "OpenSwitch-OvsDB and Sim-OvsDB ##########\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        ops_br_name = s1.ovscmd("/usr/bin/ovs-vsctl get br br0 name").strip()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get br br0" \
                        " datapath_type name")
        ovs_datapath_type, ovs_br_name = out.splitlines()
        assert ovs_datapath_type == 'netdev' \
               and ops_br_name == ovs_br_name, \
               "Unexpected configuration in Bridge"
        info("### Bridge configuration matches in OpenSwitch-OvsDB",
             "and SIM-OvsDB. ###\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        out = s1.ovscmd("/usr/bin/ovs-vsctl get vlan VLAN100 admin " \
                     "id name")
        ops_admin_state, ops_vlan_id, ops_vlan_name = out.splitlines()
        assert ops_admin_state == 'up' \
               and ops_vlan_id == '100' \
               and ops_vlan_name == 'VLAN100', \
               "OpenSwitch-OvsDB VLAN configuration " \
               "before adding port is wrong!"
        info("### OpenSwitch-OvsDB VLAN configuration before",
             "adding port is as expected ###\n")

        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        time.sleep(1)

        out = s1.ovscmd("/usr/bin/ovs-vsctl get vlan VLAN100 " \
                     "oper_state oper_state_reason")
        ops_oper_state, ops_oper_state_reason = out.splitlines()
        assert ops_oper_state == 'up' \
               and ops_oper_state_reason == 'ok', \
               "OpenSwitch-OvsDB VLAN configuration " \
               "after adding port is wrong!"
        info("### OpenSwitch-OvsDB VLAN configuration",
             "after adding port is as expected ###\n")

        out = s1.ovscmd("/usr/bin/ovs-vsctl get port 1 name tag vlan_mode")
        ops_port_name, ops_tag, ops_vlan_mode = out.splitlines()
        out = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 " \
                     "name tag vlan_mode")
        ovs_port_name, ovs_tag, ovs_vlan_mode = out.splitlines()
        assert ops_port_name == ovs_port_name \
               and ops_tag == ovs_tag \
               and ops_vlan_mode == ovs_vlan_mode, \
               "Mismatch in access port configuration "\
               "between OpenSwitch-OvsDB and OVS-OvsDB"
        info("### OpenSwitch-OvsDB access port configuration matches",
             "the Sim-OvsDB access port configuration ###\n")

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
               "Mismatch in interface configuration " \
               "between OpenSwitch-OvsDB and Sim-OvsDB"
        info("### OpenSwitch-OvsDB interface configuration matches",
             "the Sim-OvsDB interface configuration ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def vlan_normal(self):
        '''
            2.1 Add VLAN 100 to global VLAN table
            2.2 Add port 1 with tag 100
            2.3 Add port 2 with tag 100
            2.4 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n########## Test Case 2 - Normal VLAN access",
             "mode operation ##########\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        assert status, \
               "Ping Failed even though VLAN was configured correctly"
        info("### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port br0 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port br0 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")

    def invalid_tags(self):
        '''
            4.1 Add VLAN 100 to global VLAN table
            4.2 Add port 1 with tag 100
            4.3 Add port 2 with tag 200
            4.4 Test Ping - should not work
            4.5 Delete port 2 and add port 2 with tag 100
            4.6 Test Ping - should work
        '''
        s1 = self.net.switches[ 0 ]
        h1 = self.net.hosts[ 0 ]
        h2 = self.net.hosts[ 1 ]

        info("\n########## Test Case 4 - VLAN access ports",
             "with different tags ##########\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-br br0")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan br0 200 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=200")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")

        info("### Testing if ping fails when the VLAN access ports",
             "have different tags ex: 100 & 200 ###\n")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        assert not status, \
               "Ping Success even though different tags on VLAN access ports"
        info("### Ping Failed ###\n")

        info("### Changing Tags Back to Same Config ###\n")
        info("### Adding Ports 1,2 with tag=100. Ports get reconfigured ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        out = h1.cmd("ping -c1 %s" % h2.IP())
        info(out)

        status = parsePing(out)
        assert status, \
               "Ping Failed even though VLAN access ports had the same tag"
        info("### Ping Success ###\n")

        #Cleanup before next test
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan br0 100")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 1")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port 2")
        s1.ovscmd("/usr/bin/ovs-vsctl del-br br0")

class Test_switchd_container_vlan_access:

    def setup_class(cls):
        Test_switchd_container_vlan_access.test = vlanAccessTest()

    # TC_1
    def test_switchd_container_check_config(self):
        self.test.check_config()

    # TC_2
    def test_switchd_container_vlan_normal(self):
        self.test.vlan_normal()

    # TC_3
    def test_switchd_container_invalid_tags(self):
        self.test.invalid_tags()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_vlan_access.test.net.stop()

    def __del__(self):
        del self.test
