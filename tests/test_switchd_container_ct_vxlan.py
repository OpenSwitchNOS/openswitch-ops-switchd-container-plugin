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

OVS_VSCTL="/opt/openvswitch/bin/ovs-vsctl"
OPS_VSCTL="/usr/bin/ovs-vsctl"
OVS_OFCTL="/opt/openvswitch/bin/ovs-ofctl"
OVS_ADDFLOW="/opt/openvswitch/bin/ovs-ofctl add-flow"
OVS_APPCTL="/opt/openvswitch/bin/ovs-appctl"
HOST_PER_SW = 2
NUM_OF_SWS  = 2
SWNS_EXE = "ip netns exec swns "

HOSTMAC=["notused","00:00:00:00:aa:01","00:00:00:00:aa:02", "00:00:00:00:aa:01","00:00:00:00:aa:02",]
SW_ROUTERIP=["9.0.1.1", "9.0.2.2"]


#  2 hosts per switch
#  2 switches


class mytopo(Topo):
    def build(self, hsts=2, sws=2, **_opts):
        self.hsts = hsts*sws
        self.sws  = sws
        # Create Hosts
        tot_host= sws * hsts
        for h in irange(1,tot_host):
           host = self.addHost("h%s" % h, mac="%s" % HOSTMAC[h])

        # Create Switch
        for s in irange(1, sws):
           sw = self.addSwitch("s%s" % s)
        '''
        for s in irange(1, sws):
           sw = self.addSwitch("s%s" % s)
           for h in irange(1, hsts):
              host = self.addHost("h%s-s%s" % (h,s)
        '''
        # Create Links between host and switch

        self.addLink('h1', 's1')
        self.addLink('h2', 's2')
        self.addLink('h3', 's1')
        self.addLink('h4', 's2')
        self.addLink('s1', 's2')

class vxlanTest( OpsVsiTest ):

    def setupNet(self):
        host_opts = self.getHostOpts()
        switch_opts = self.getSwitchOpts()
        vxlan_topo = mytopo(hsts=HOST_PER_SW, sws=NUM_OF_SWS, hopts=host_opts, sopts=switch_opts)
        self.net = Mininet(vxlan_topo, switch=VsiOpenSwitch,
                           host=Host, link=OpsVsiLink,
                           controller=None, build=True)

    def vtep_setup(self,ip1,ip2, br="bridge_normal",vni = "flow"):
        info("\n ### vtep_setup remote_ip for s1 %s for s2 %s ###\n" % (ip1,ip2))
        s1 = self.net.switches[ 0 ]
        s2 = self.net.switches[ 1 ]
        s1.ovscmd(OPS_VSCTL + " add-port %s vtep -- set interface vtep type=vxlan options:remote_ip=%s options:key=%s" % (br,ip1,vni))
        sleep(1)
        s2.ovscmd(OPS_VSCTL + " add-port %s vtep  -- set interface vtep type=vxlan options:remote_ip=%s options:key=%s" % (br,ip2,vni))
        '''
        ops = s1.ovscmd(OPS_VSCTL + " show")
        ovs = s2.ovscmd(OVS_VSCTL + " show")
        info("\n \n ops-ctl \n %s \n\n ovs-ctl \n%s \n\n" % (ops, ovs))
        '''

    def conf_access_port(self, intf, vlan):
        info("\n ### Access_ports setup ### \n")
        nsw = 0
        for sw in self.net.switches:
            sw.cmdCLI("configure terminal")
            sw.cmdCLI("vlan %s" % vlan)
            sw.cmdCLI("no shutdown")
            sw.cmdCLI("end")
            sleep(1)
            sw.cmdCLI("configure terminal")
            sw.cmdCLI("interface %s" % intf)
            sw.cmdCLI("no routing")
            sw.cmdCLI("vlan access %s" % vlan)
            sw.cmdCLI("no shutdown")
            sw.cmdCLI("end")
            nsw += 1


    def conf_net_port(self, intf):
       info("\n ### route_conf for interface %s" % intf)
       nsw = 0
       for sw in self.net.switches:
           sw.cmdCLI("configure terminal")
           sw.cmdCLI("interface %s" % intf)
           sw.cmdCLI("no shutdown")
           sw.cmdCLI("ip address %s/24" % SW_ROUTERIP[nsw])
           sw.cmdCLI("no shutdown")
           sw.cmdCLI("exit")
           nsw += 1
           sleep(1)
       sleep(5)
       s1 = self.net.switches[ 0 ]
       s2 = self.net.switches[ 1 ]
       info("\nRouting switch s1\n")
       s1.ovscmd(SWNS_EXE + "route add -net 9.0.2.0/24 gw 9.0.1.1 dev %s" % intf)
       sleep(1)
       out = s1.ovscmd(SWNS_EXE + "route -n")
       info("result %s" % out)
       info("\nRouting switch s2\n")
       s2.ovscmd(SWNS_EXE + "route add -net 9.0.1.0/24 gw 9.0.2.2 dev %s" % intf)
       sleep(1)
       out = s2.ovscmd(SWNS_EXE + "route -n")
       info("result %s" % out)


    def tcpdump_setup(self):
        info("\n ### tcpdump setup ### \n")
        for sw in self.net.switches:
            sw.ovscmd("mv /usr/sbin/tcpdump /usr/bin/")
            out = sw.ovscmd("which tcpdump")
            if "usr/bin/tcpdump" not in out:
               info("Failed to set up tcpdump")

    def test_ping(self):
        h1 = self.net.hosts[0]
        h2 = self.net.hosts[1]
        info("\nh1 ip is %s\nh2 ip is %s\n" % (h1.IP(), h2.IP()))
        out = h1.cmd("ping -c 3 %s" % h2.IP())
        status = parsePing(out)
        if status:
            info("\n **** h1 - h2 Ping success! *****\n")
        else:
            info("\n h1- h2 Ping failed!\n")

    def vxlan(self):
        info("\n\n\n ########## testing vxlan standalone mode ##########\n\n\n")
        self.vtep_setup(SW_ROUTERIP[1], SW_ROUTERIP[0], "bridge_normal", "100")
        self.tcpdump_setup()
        self.conf_access_port("1", "100")
        self.conf_net_port("3")
        self.test_ping()

@pytest.mark.timeout(0)
class Test_switchd_container_vlan_access:

    def setup_class(cls):
        Test_switchd_container_vlan_access.test = vxlanTest()

    # TC_1
    def test_switchd_container_check_config(self):
        self.test.vxlan()
        CLI(self.test.net)

    def teardown_class(cls):
        # Stop the Docker containers, and
        # mininet topology
        Test_switchd_container_vlan_access.test.net.stop()

    def __del__(self):
        del self.test
