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

class vlanModeTest( OpsVsiTest ):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        vlanmode_topo = SingleSwitchTopo(k=0, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(vlanmode_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def default_behavior(self):
        '''
        This test case verifies the default behavior of port add/del
        functionality when the VLANs are enabled in the VLAN table.
        If no VLAN is enabled, ports do not get added in the internal
        'ASIC' OVS. Also, the ports get reconfigured  in trunk mode as soon as
        VLANs are enabled and start trunking the enabled VLANs in the table
        '''
        s1 = self.net.switches[ 0 ]

        info("\n######## Verify Changing VLAN Configuration" \
                " on the fly and Port Add/Del Functionality in " \
             "Internal ASIC OVS driven by OpenSwitch OVS ########\n")

        info("\n######## Test Case 1: Verifying default behavior when VLAN " \
                "is disabled or enabled ########\n")

        info("### Adding port 1 without any VLAN configuration and no VLAN " \
                "enabled in the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 1")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")

        port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "trunks")
        assert "[1]" in port_trunk, "Port 1 did not trunk VLAN100 " \
            "by default"

        info("### Port 1 did not get added in 'ASIC' OVS as expected ###\n\n")

        info("### Enabling VLAN100 in the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan bridge_normal 100 admin=up")
        time.sleep(1)
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "vlan_mode")
        assert "trunk" in port_name, "Port 1 did not get added in " \
            "'ASIC' OVS even after global VLAN100 was enabled"
        info("### Port 1 got added in trunk mode in 'ASIC' OVS as expected " \
                "###\n")
        port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "trunks")
        assert "[1, 100]" in port_trunk, "Port 1 did not trunk VLAN100 " \
            "by default"
        info("### Port 1 trunks enabled VLAN100 by default " \
                "when no trunks are explicitly configured ###\n")

        info("### Now deleting port 1 ###\n\n")
        s1.ovscmd("/usr/bin/ovs-vsctl del-port bridge_normal 1")

    def changing_modes(self):
        '''
        This test case verifies the 'set' feature of ovs-vsctl utility where
        vlan_mode. tag and trunks column in the Port table can be changed
        dynamically. Passing valid configuration would result in the port
        getting reconfigured with the corresponding vlan_mode and respective
        tag/trunks or both. Respective VLAN need to be enabled in the global
        VLAN table.
        '''
        s1 = self.net.switches[ 0 ]

        info("\n######## Test Case 2: Changing different VLAN modes on the " \
                "fly ########\n")
        info("### Specifying just the tag field while adding port 1 ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 1 tag=100")
        time.sleep(2)
        port_mode, port_tag = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get " \
                "port 1 vlan_mode tag").splitlines()
        assert "access" in port_mode and "100" in port_tag, "Port 1 did not " \
                "added in access mode with tag=100"
        info("### Port 1 got added in 'access' mode with tag=100 as expected" \
                " ###\n\n")

        info("### Changing vlan mode to 'trunk' using ovs-vsctl set feature " \
                "###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl set port 1 vlan_mode=trunk")
        time.sleep(2)
        port_mode, port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
                "get port 1 vlan_mode trunks").splitlines()
        assert "trunk" in port_mode and "100" in port_trunk, "Port 1 did " \
                "not added in trunk mode with trunks=100"
        info("### Port 1 got added in 'trunk' mode with trunks=100 as " \
                "expected just by changing the vlan_mode ###\n\n")

        info("### Changing vlan mode to 'native-tagged' using ovs-vsctl set " \
                "feature ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl set port 1 vlan_mode=native-tagged "\
                "tag=100 trunks=100")
        time.sleep(2)
        port_mode, port_trunk, port_tag = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode " \
                "trunks tag").splitlines()
        assert "native-tagged" in port_mode and "100" in port_trunk and "100" \
                in port_tag, "Port 1 did not get added in native-tagged mode" \
                " with trunks=100 tag=100"
        info("### Port 1 got successfully added in 'native-tagged' mode with" \
                " trunks=100 tag=100 as expected ###\n\n")

        info("### Changing vlan mode to 'native-untagged' using ovs-vsctl " \
                "set feature ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl set port 1 vlan_mode=native-untagged "\
                "tag=100 trunks=100")
        time.sleep(2)
        port_mode, port_trunk, port_tag = \
        s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode " \
                "trunks tag").splitlines()
        assert "native-untagged" in port_mode and "100" in port_trunk and \
                "100" in port_tag, "Port 1 did not get added in " \
                "native-tagged mode with trunks=100 tag=100"
        info("### Port 1 got successfully added in 'native-untagged' mode " \
                "with trunks=100 tag=100 as expected ###\n\n")

    def invalid_config(self):
        '''
        This test case verifies the behavior of ports when invalid VLAN
        configuration is passed. Passing invalid VLAN configuration results in
        port getting deleted from the internal 'ASIC' OVS.
        '''
        s1 = self.net.switches[ 0 ]

        info("\n######## Test Case 3: Passing invalid VLAN configuration " \
                "########\n")
        info("### Setting trunks=200 and vlan_mode=trunk for port 1 which " \
                "is not enabled in the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl set port 1 vlan_mode=trunk "\
                "trunks=200")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "vlan_mode")
        assert not "trunk" in port_name, "Port 1 got added in " \
            "'ASIC' OVS even without any global VLAN200 enabled"
        info("### Port 1 did not get added in 'ASIC' OVS with invalid config" \
                " as expected ###\n\n")

        info("### Setting tag=200 and vlan_mode=access for port 1 which " \
                "is not enabled in the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl set port 1 vlan_mode=access tag=200")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "vlan_mode")
        assert not "access" in port_name, "Port 1 got added in " \
            "'ASIC' OVS even without any global VLAN200 enabled"
        info("### Port 1 did not get added in 'ASIC' OVS with invalid config" \
                " as expected ###\n\n")

    def add_del_vlan(self):
        '''
        This test case verifies port reconfiguration when corresponding VLANs
        are enabled or disabled. Port 1 is expeced to trunk VLAN200 when it is
        enabled and Port 2 is epected to trunk all the VLANs which are enabled
        in the table and get reconfigured accordingly.
        '''
        s1 = self.net.switches[ 0 ]

        info("\n######## Test Case 4: Disabling VLANs and expected port " \
                "reconfiguration ########\n")
        info("### Deleting VLAN100 from the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan bridge_normal 100")
        time.sleep(1)
        info("### Adding port 2 without an VLAN configuration specified ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 2")
        time.sleep(1)
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        time.sleep(1)
        port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 2 "
                              "trunks")
        assert "[1]" in port_trunk, "Port 2 did not trunk VLAN100 " \
            "by default"

        info("### Port 2 did not get added in 'ASIC' OVS as expected ###\n\n")
        info("### Adding VLAN100 and VLAN200 in the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan bridge_normal 100 admin=up")
        time.sleep(1)
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan bridge_normal 200 admin=up")
        time.sleep(1)
        info("### Ports 1 and 2 are expected to get re-configured ###\n")
        port_mode_1, port_tag = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
                "get port 1 vlan_mode tag").splitlines()
        port_mode_2, port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
                "get port 2 vlan_mode trunks").splitlines()
        assert "access" in port_mode_1 and "200" in port_tag and "trunk" in \
                port_mode_2 and "100, 200" in port_trunk, "Port 1 got added " \
                "in 'ASIC' OVS even without any global VLAN200 enabled"
        info("### Port 1 gets re-added in access mode with tag=200 and " \
                " port 2 in trunk mode with trunks=100,200 ###\n\n")

        info("### Deleting VLAN200 from the global VLAN table ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl del-vlan bridge_normal 200")
        port_name_1 = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "vlan_mode")
        port_mode_2, port_trunk = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl " \
                "get port 2 vlan_mode trunks").splitlines()
        assert not "access" in port_name_1 and "100" in port_trunk and \
                "trunk" in port_mode_2 and not "200" in port_trunk, \
                "Port 1 got deleted and Port 2 got reconfigured with " \
                "trunks=100 as only global VLAN100 is now enabled"
        info("### Port 1 got deleted from the 'ASIC' OVS and Port 2 got " \
                "reconfigured with trunks=100 as expected ###\n\n\n")

class Test_switchd_container_changing_vlan_config:

    def setup_class(cls):
        Test_switchd_container_changing_vlan_config.test = vlanModeTest()

    # TC_1
    def test_switchd_container_default_behavior(self):
        self.test.default_behavior()

    # TC_2
    def test_switchd_container_changing_modes(self):
        self.test.changing_modes()

    # TC_3
    def test_switchd_container_invalid_config(self):
        self.test.invalid_config()

    # TC_4
    def test_switchd_container_add_del_vlan(self):
        self.test.add_del_vlan()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_changing_vlan_config.test.net.stop()

    def __del__(self):
        del self.test
