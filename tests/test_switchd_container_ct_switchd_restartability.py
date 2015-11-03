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


class switchdTest(OpsVsiTest):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        switchd_topo = SingleSwitchTopo(
            k=0,
            hopts=host_opts,
            sopts=switch_opts)
        self.net = Mininet(switchd_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def switchd_restart(self):
        '''
        This test function checks whether the Internal ASIC "OVS" keeps in
        sync with the OpenSwitch OVS database.
        Here, we delete one of the ports from the DB when the ops-switchd
        is not running and check if the ports get deleted from the "ASIC" OVS
        when ops-switchd daemon is "started" and "restarted".
        '''
        s1 = self.net.switches[0]

        info("\n########## Test Case 1 - Verify Correct Data Reflection "
             "in Internal 'ASIC' OVS when Openswitch OVS Switchd Service is "
             "'Restarted' ##########\n")
        info("### Adding VLAN100 and ports 1, 2, 3 to default bridge ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl add-vlan bridge_normal 100 admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 1")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 1 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 2")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 2 user_config:admin=up")
        s1.ovscmd("/usr/bin/ovs-vsctl add-port bridge_normal 3")
        s1.ovscmd("/usr/bin/ovs-vsctl set interface 3 user_config:admin=up")

        info("\n### Stopping switchd service ###\n")
        s1.cmd("systemctl stop switchd")

        info("### Deleting port 3 from the OpenSwitch database ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl --no-wait del-port bridge_normal 3")

        info("### Verifying that port 3 still exists in the 'ASIC' OVS "
             "database ###\n")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 3 "
                              "vlan_mode")
        assert "trunk" in port_name, "Port 3 got deleted from 'ASIC' OVS " \
            "even before switchd was started"

        info("\n### 'Starting' switchd service ###\n")
        s1.cmd("systemctl start switchd")

        info("### Verifying that port 3 got deleted from the 'ASIC' OVS "
             "database ###\n")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 3 "
                              "vlan_mode")
        assert not "trunk" in port_name, "Port 3 did not get  deleted from " \
            "'ASIC' OVS even after switchd was started"

        info("\n### Stopping switchd service ###\n")
        s1.cmd("systemctl stop switchd")

        info("### Deleting port 2 from the OpenSwitch database ###\n")
        s1.ovscmd("/usr/bin/ovs-vsctl --no-wait del-port bridge_normal 2")

        info("### Verifying that port 2 still exists in the 'ASIC' OVS "
             "database ###\n")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 2 "
                              "vlan_mode")
        assert "trunk" in port_name, "Port 2 got deleted from 'ASIC' OVS " \
            "even before switchd was restarted"

        info("\n### 'Re-starting' switchd service ###\n")
        s1.cmd("systemctl restart switchd")

        info("### Verifying that port 2 got deleted from the 'ASIC' OVS "
             "database ###\n")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 2 "
                              "vlan_mode")
        assert not "trunk" in port_name, "Port 2 did not get  deleted from " \
            "'ASIC' OVS even after switchd was restarted"

    def switchd_kill(self):
        '''
        This test function checks whether the Internal ASIC "OVS" keeps in
        sync with the OpenSwitch OVS database.
        Here, we delete one of the ports from the DB when the ops-switchd
        is not running and check if the ports get deleted from the "ASIC" OVS
        when ops-switchd daemon is "started" and "restarted".
        '''
        s1 = self.net.switches[0]

        info("\n\n\n########## Test Case 2 - Verify Correct Data Reflection "
             "in Internal 'ASIC' OVS when Openswitch OVS Switchd Service is "
             "'Killed and Restarted' by Itself ##########\n")
        info("### Taking note of 'pid' of ops-switchd, ovs-vswitchd-sim and "
             "'ASIC' ovsdb-server processes before killing OpenSwitch "
             "switchd service ###")
        pid_switchd_bf = s1.cmd("pidof ops-switchd")
        pid_vswitchd_sim_bf = s1.cmd("pidof ovs-vswitchd-sim")
        pid_ovsdb_sim_bf = s1.cmd("pidof /opt/openvswitch/sbin/ovsdb-server")
        info("\n### Stopping 'ASIC' ovsdb-server service ###\n")
        s1.cmd("systemctl stop ovsdb-server-sim")
        info("### Deleting 'ASIC' OVS database file ###\n")
        s1.cmd("sudo rm -rf /var/run/openvswitch-sim/ovsdb.db")
        info("\n### Killing switchd service which is expected to restart by "
             "itself ###\n")
        s1.cmd("sudo kill -9 `pidof ops-switchd`")
        time.sleep(2)
        info("### Taking note of 'pid' of ops-switchd, ovs-vswitchd-sim and "
             "ovsdb-server-sim services after OpenSwitch switchd "
             "service restarted ###\n")
        pid_switchd_af = s1.cmd("pidof ops-switchd")
        pid_vswitchd_sim_af = s1.cmd("pidof ovs-vswitchd-sim")
        pid_ovsdb_sim_af = s1.cmd("pidof /opt/openvswitch/sbin/ovsdb-server")
        info("\n### Verifying if new processes were created when switchd "
             "restarts by itself ###\n")
        assert not (pid_switchd_bf == pid_switchd_af and pid_vswitchd_sim_bf
                    == pid_vswitchd_sim_af and pid_ovsdb_sim_bf ==
                    pid_ovsdb_sim_af), "New proceses were not created\n"
        info("### Verifying if the 'ASIC' OVS data is in sync with the "
             "OpenSwitch OVS ###\n\n\n")
        port_name = s1.ovscmd("/opt/openvswitch/bin/ovs-vsctl get port 1 "
                              "vlan_mode")
        assert "trunk" in port_name, "Port 1 did not get re-added in 'ASIC' " \
            "OVS even after switchd was restarted"


class Test_switchd_container_switchd_restartability:

    def setup_class(cls):
        Test_switchd_container_switchd_restartability.test = switchdTest()

    # TC_1
    def test_switchd_container_switchd_restart(self):
        self.test.switchd_restart()

    # TC_2
    def test_switchd_container_switchd_kill(self):
        self.test.switchd_kill()

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_switchd_restartability.test.net.stop()

    def __del__(self):
        del self.test
