# -*- coding: utf-8 -*-
# (C) Copyright 2016 Hewlett Packard Enterprise Development LP
# All Rights Reserved.
#
#    Licensed under the Apache License, Version 2.0 (the "License"); you may
#    not use this file except in compliance with the License. You may obtain
#    a copy of the License at
#
#         http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
#    WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
#    License for the specific language governing permissions and limitations
#    under the License.
#
##########################################################################

"""
OpenSwitch Test for switchd related configurations.
"""

from time import sleep
from pytest import mark

TOPOLOGY = """
# +-------+
# |  ops1 |
# +-------+

# Nodes
[type=openswitch name="OpenSwitch 1"] ops1
"""

# Timeout in seconds to wait for ops-switchd to come up
ops_switchd_active_timeout = 60

@mark.platform_incompatible(['ostl'])
def test_switchd_container_ct_switchd_restartability(topology, step):
    ops1 = topology.get("ops1")
    assert ops1 is not None

    ops1._shells["bash"]._timeout = 120
    step("Step 1- Adding VLAN100 and ports 1, 2, 3 to default bridge")
    ops1("add-vlan bridge_normal 100 admin=up", shell="vsctl")
    ops1("add-port bridge_normal 1", shell="vsctl")
    ops1("set interface 1 user_config:admin=up", shell="vsctl")
    ops1("add-port bridge_normal 2", shell="vsctl")
    ops1("set interface 2 user_config:admin=up", shell="vsctl")
    ops1("add-port bridge_normal 3", shell="vsctl")
    ops1("set interface 3 user_config:admin=up", shell="vsctl")

    step("Step 2- Stopping switchd service")
    ops1("systemctl stop switchd", shell="bash")

    step("Step 3- Deleting port 3 from the OpenSwitch database")
    ops1("ovs-vsctl --no-wait del-port bridge_normal 3", shell="bash")

    step("Step 4- Verifying that port 3 still exists in the 'ASIC' OVS "
         "database")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 3 vlan_mode",
                     shell="bash")
    assert "trunk" in port_name

    step("Step 5- 'Starting' switchd service")
    ops1("systemctl start switchd", shell="bash")
    for i in range(0, ops_switchd_active_timeout):
        is_switchd_active = ops1("systemctl is-active switchd.service",
                                 shell="bash")
        if not is_switchd_active == 'active':
            sleep(1)
        else:
            break
    assert is_switchd_active == 'active', \
        'Timed out while waiting for ops-switchd to come up'

    step("Step 6- Verifying that port 3 got deleted from the 'ASIC' OVS "
         "database")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 3 vlan_mode",
                     shell="bash")
    assert "trunk" not in port_name

    step("Step 7- Stopping switchd service")
    ops1("systemctl stop switchd", shell="bash")

    step("Step 8- Deleting port 2 from the OpenSwitch database")
    ops1("--no-wait del-port bridge_normal 2", shell="vsctl")

    step("Step 9- Verifying that port 2 still exists in the 'ASIC' OVS "
         "database")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 2 vlan_mode",
                     shell="bash")
    assert "trunk" in port_name

    step("Step 10- 'Re-starting' switchd service")
    ops1("systemctl restart switchd", shell="bash")

    step("Step 11- Verifying that port 2 got deleted from the 'ASIC' OVS "
         "database")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 2 vlan_mode",
                     shell="bash")
    assert "trunk" not in port_name

    step("Step 1- Taking note of 'pid' of ops-switchd, ovs-vswitchd-sim and "
         "'ASIC' ovsdb-server processes before killing OpenSwitch "
         "switchd service")
    pid_switchd_bf = ops1("pidof ops-switchd", shell="bash")
    pid_vswitchd_sim_bf = ops1("pidof ovs-vswitchd-sim", shell="bash")
    pid_ovsdb_sim_bf = ops1("pidof /opt/openvswitch/sbin/ovsdb-server",
                            shell="bash")

    step("Step 2- Stopping 'ASIC' ovsdb-server service")
    ops1("systemctl stop ovsdb-server-sim", shell="bash")

    step("Step 3- Deleting 'ASIC' OVS database file")
    ops1("sudo rm -rf /var/run/openvswitch-sim/ovsdb.db", shell="bash")

    step("Step 4- Killing switchd service which is expected to restart by "
         "itself")
    ops1("sudo kill -9 $(ps -A | grep ops-switchd | awk '{print $1}')",
         shell="bash")
    sleep(2)

    step("Step 5- Taking note of 'pid' of ops-switchd, ovs-vswitchd-sim and "
         "ovsdb-server-sim services after OpenSwitch switchd "
         "service restarted")
    pid_switchd_af = ops1("pidof ops-switchd", shell="bash")
    pid_vswitchd_sim_af = ops1("pidof ovs-vswitchd-sim", shell="bash")
    pid_ovsdb_sim_af = ops1("pidof /opt/openvswitch/sbin/ovsdb-server",
                            shell="bash")

    step("Step 6- Verifying if new processes were created when switchd "
         "restarts by itself")
    assert (
        str(pid_switchd_bf) != str(pid_switchd_af) and
        str(pid_vswitchd_sim_bf) != str(pid_vswitchd_sim_af) and
        str(pid_ovsdb_sim_bf) != str(pid_ovsdb_sim_af)
    )

    step("Step 7- Verifying if the 'ASIC' OVS data is in sync with the "
         "OpenSwitch OVS")
    port_name = ops1("/opt/openvswitch/bin/ovs-vsctl get port 1 vlan_mode",
                     shell="bash")
    assert "trunk" in port_name
