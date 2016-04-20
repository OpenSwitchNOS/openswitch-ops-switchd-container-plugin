# Switchd Container Plugin Test Cases

## Contents
### [Port Configuration in Different VLAN Modes](#port-configuration-in-different-vlan-modes)
- [Port in access VLAN mode](#port-in-access-vlan-mode)
	- [Objective for port in access VLAN mode](#objective-for-port-in-access-vlan-mode)
	- [Requirements for port in access VLAN mode](#requirements-for-port-in-access-vlan-mode)
	- [Setup topology diagram port in access VLAN mode](#setup-topology-diagram-port-in-access-vlan-mode)
	- [Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-access-mode)
		- [Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode](#description-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-access-mode)
		- [Test result criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode](#test-result-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-access-mode)
			- [Test pass criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode](#test-pass-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-access-mode)
	- [Test Case 2 Adding VLAN and ports and pass traffic in Access mode](#test-case-2-adding-vlan-and-ports-and-pass-traffic-in-access-mode)
		- [Description for Test Case 2 Adding VLAN and ports and pass traffic in Access mode](#description-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-access-mode)
		- [Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic in Access mode](#test-result-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-access-mode)
			- [Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic in Access mode](#test-pass-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-access-mode)
	- [Test Case 3 With and without global VLAN  in Access mode](#test-case-3-with-and-without-global-vlan-in-access-mode)
		- [Description for Test Case 3 With and without global VLAN in Access mode](#description-for-test-case-3-with-and-without-global-vlan-in-access-mode)
		- [Test result criteria for Test Case 3 With and without global VLAN in Access mode](#test-result-criteria-for-test-case-3-with-and-without-global-vlan-in-access-mode)
			- [Test pass criteria for Test Case 3 With and without global VLAN in Access mode](#test-pass-criteria-for-test-case-3-with-and-without-global-vlan-in-access-mode)
	- [Test Case 4 Different and same tags in Access mode](#test-case-4-different-and-same-tags-in-access-mode)
		- [Description for Test Case 4 Different and same tags in Access mode](#description-for-test-case-4-different-and-same-tags-in-access-mode)
		- [Test result criteria for Test Case 4 Different and same tags in Access mode](#test-result-criteria-for-test-case-4-different-and-same-tags-in-access-mode)
			- [Test pass criteria for Test Case 4 Different and same tags in Access mode](#test-pass-criteria-for-test-case-4-different-and-same-tags)
- [Port in trunk VLAN mode](#port-in-trunk-vlan-mode)
	- [Objective for port in trunk VLAN mode](#objective-for-port-in-trunk-vlan-mode)
	- [Requirements for port in trunk VLAN mode](#requirements-for-port-in-trunk-vlan-mode)
	- [Setup topology diagram port in trunk VLAN mode](#setup-topology-diagram-port-in-trunk-vlan-mode)
	- [Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-trunk-mode)
		- [Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode](#description-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-trunk-mode)
		- [Test result criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode](#test-result-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-trunk-mode)
			- [Test pass criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode](#test-pass-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs-in-trunk-mode)
	- [Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode](#test-case-2-adding-vlan-and-ports-and-pass-traffic-in-trunk-mode)
		- [Description for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode](#description-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-trunk-mode)
		- [Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode](#test-result-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-trunk-mode)
			- [Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode](#test-pass-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic-in-trunk-mode)
	- [Test Case 3 With and without global VLAN in Trunk mode](#test-case-3-with-and-without-global-vlan-in-trunk-mode)
		- [Description for Test Case 3 With and without global VLAN in Trunk mode](#description-for-test-case-3-with-and-without-global-vlan-in-trunk-mode)
		- [Test result criteria for Test Case 3 With and without global VLAN in Trunk mode](#test-result-criteria-for-test-case-3-with-and-without-global-vlan-in-trunk-mode)
			- [Test pass criteria for Test Case 3 With and without global VLAN in Trunk mode](#test-pass-criteria-for-test-case-3-with-and-without-global-vlan-in-trunk-mode)
	- [Test Case 4 Different and same trunks](#test-case-4-different-and-same-trunks-in-trunk-mode)
		- [Description for Test Case 4 Different and same trunks in Trunk mode](#description-for-test-case-4-different-and-same-trunks-in-trunk-mode)
		- [Test result criteria for Test Case 4 Different and same trunks in Trunk mode](#test-result-criteria-for-test-case-4-different-and-same-trunks-in-trunk-mode)
			- [Test pass criteria for Test Case 4 Different and same trunks in Trunk mode](#test-pass-criteria-for-test-case-4-different-and-same-trunks-in-trunk-mode)

### [Sampled Flow](#sampled-flow)
- [Objective for sFlow](#objective-for-sflow)
- [Requirements for sFlow](#requirements-for-sflow)
- [Setup topology diagram for sFlow](#setup-topology-diagram-for-sflow)
- [Test Case for sFlow](#test-case-for-sflow)
	- [Description of Test Case for sFlow](#description-of-test-case-for-sflow)
	- [Test result criteria for sFlow](#test-result-criteria-for-sflow)
		- [Test pass criteria for sFlow](#test-pass-criteria-for-sflow)
		- [Test fail criteria for sFlow](#test-fail-criteria-for-sflow)

## Port Configuration in Different VLAN Modes

##  Port in access VLAN mode

### Objective for port in access VLAN mode
The test case checks whether the OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both of the OVS's. The ports in Access mode get configured only when a corresponding VLAN is added. If the VLAN gets deleted, the respective ports with matching tags get deleted from the internal OVS. The following test cases verify ping traffic in different configurations.

### Requirements for port in access VLAN mode
- Virtual Mininet test setup
- **CT File**: ops-switchd-container-plugin/tests/test\_switchd\_container\_ct\_vlan\_access.py


### Setup topology diagram port in access VLAN mode
Single switch topology
```ditaa
                1 +-------------+ 2
       +---------->             <----------+
       |          |    Switch   |          |
       |          |             |          |
       | eth0     |             |          | eth0
+------v-----+    +-------------+   +------v-----+
|            |                      |            |
|  Host1     |                      |   Host2    |
|            |                      |            |
|            |                      |            |
+------------+                      +------------+

```
### Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode

#### Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode
1. Add bridge `br0` on the switch.
2. Add `VLAN100` to the global VLAN table on the switch.
3. Add port 1 with tag=100 on the switch in access mode.
4. Verify whether the same configuration gets set on the "ASIC" simulating Internal OVS.
5. Run the following commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
```

#### Test result criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode

##### Test pass criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Access mode
- Bridge `br0` should get created in OpenSwitch OVS and Internal "ASIC" OVS.
- Create `VLAN100` in OpenSwitch OVS and admin=up.
- After adding port 1 to the VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
- The interface 1 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2 Adding VLAN and ports and pass traffic in Access mode

#### Description for Test Case 2 Adding VLAN and ports and pass traffic in Access mode
1. Add bridge `br0` on the switch.
2. Add `VLAN100` to the global VLAN table on the switch.
3. Add port 1 and 2 with tag=100 on the switch in access mode.
4. Verify whether host1 can ping host2.
5. Run the following commands:

```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```

#### Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic in Access mode

##### Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic in Access mode
Ping from host1 to host2 should be successful.

### Test Case 3 With and without global VLAN in Access mode

#### Description for Test Case 3 With and without global VLAN in Access mode
1. Add bridge `br0` on the switch.
2. Do NOT add `VLAN100` to the global VLAN table on the switch.
3. Add port 1 and 2 with tag=100 on the switch in access mode.
4. Verify whether host1 can ping host2.
5. Add `VLAN100` to the global VLAN table on the switch.
6. Verify whether host1 can ping host2.
7. Run the following commands:

```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=200
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
/usr/bin/ovs-vsctl del-port br0 2
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
```

#### Test result criteria for Test Case 3 With and without global VLAN in Access mode

##### Test pass criteria for Test Case 3 With and without global VLAN in Access mode
Ping from host1 to host2 should be successful **only** when VLAN is enabled.

### Test Case 4 Different and same tags in Access mode

#### Description for Test Case 4 Different and same tags in Access mode
1. Add Bridge `br0` on the switch.
2. Add `VLAN100` to the global VLAN table on the switch.
3. Add port 1 with tag=100 and port 2 with tag=200 on the switch in access mode.
4. Verify whether host1 can ping host2.
5. Re-add port 2 with tag=100.
6. Verify whether host1 can ping host2.
7. Run the following commands:

```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=200
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
/usr/bin/ovs-vsctl del-port br0 2
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
```

#### Test result criteria for Test Case 4 Different and same tags in Access mode

##### Test pass criteria for Test Case 4 Different and same tags in Access mode
Ping from host1 to host2 should be successful **only** when both the ports have **same tag**=100.

##  Port in trunk VLAN mode

### Objective for port in trunk VLAN mode
The test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS's. The ports in Trunk mode get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective ports with matching trunks get deleted from the internal OVS. The following test cases verify ping traffic in different configurations.

### Requirements for port in trunk VLAN mode
- Virtual Mininet test setup
- **CT File**: ops-switchd-container-plugin/tests/test\_switchd\_container\_ct\_vlan\_trunk.py


### Setup topology diagram port in trunk VLAN mode
Dual switch topology
```ditaa
                 +------------+      +------------+
               1 |            | 2  2 |            | 1
      +---------->   Switch1  <------->  Switch2  <---------+
 eth0 |          |            |      |            |         | eth0
+-----+------+   |            |      |            |  +------+----+
|            |   +------------+      +------------+  |           |
|   Host1    |                                       |  Host2    |
|            |                                       |           |
|            |                                       |           |
+------------+                                       +-----------+


```
### Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode

#### Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode
1. Add Bridge `br0` on switch1.
2. Add `VLAN100` to the global VLAN table on Switch1.
3. Add port 1 with tag=100 on switch1 in access mode.
4. Add port 2 with trunks=200 on switch1 in trunk mode.
5. Verify whether the same configurations get set on the "ASIC" simulating internal OVS.
6. Run the following commands:

```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```

#### Test result criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode

##### Test pass criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS in Trunk mode
- Bridge `br0` should get created in OpenSwitch OVS and Internal "ASIC" OVS.
- `VLAN100` should get created in OpenSwitch OVS and admin=up.
- After adding port 2 to the VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
- The interface 2 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode

#### Description for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode
1. Add bridge `br0` on both the switches.
2. Add `VLAN100` to global VLAN table on both switches.
3. Add port 1 with tag=100 in access mode on both switches.
4. Add port 2 with trunks=100 in trunk mode on both switches.
5. Verify whether host1 can ping host2.
6. Run the following commands:

```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```

#### Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode

##### Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic in Trunk mode
Ping from host1 to host2 should be successful

### Test Case 3 With and without global VLAN in Trunk mode

#### Description for Test Case 3 With and without global VLAN in Trunk mode
1. Add bridge `br0` on both switches.
2. Do NOT add `VLAN100` to global VLAN table on both switches.
3. Add port 1 with tag=100 in access mode on both switches.
4. Add port 2 with trunks=100 in trunk mode on both switches.
5. Verify whether host1 can ping host2.
6. Add `VLAN100` to the global VLAN table on both switches.
7. Verify whether host1 can ping host2.
8. Run the following commands:

```
On Switch1 and Switch2,
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
Test ping from host1 to host2
```

#### Test result criteria for Test Case 3 With and without global VLAN in Trunk mode

##### Test pass criteria for Test Case 3 With and without global VLAN in Trunk mode
Ping from host1 to host2 should be successful **only** when the VLAN is enabled.

### Test Case 4 Different and same trunks in Trunk mode

#### Description for Test Case 4 Different and same trunks in Trunk mode
1. Add bridge `br0` on both switches.
2. Add `VLAN100` to the global VLAN table on both switches.
3. Add port 1 with tag=100 in access mode on both switches.
4. Add port 2 with trunks=100 in trunk mode on switch1 and trunks=200 on switch2.
5. Verify whether host1 can ping host2.
6. Re-add port 2 on switch2 with trunks=100.
7. Verify whether host1 can ping host2.
8. Run the following commands:

```
On Switch1 and Switch2,
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
On Switch1,
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
On Switch2,
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=200
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
On Switch2,
/usr/bin/ovs-vsctl del-port br0 2
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
Test ping from host1 to host2
```

#### Test result criteria for Test Case 4 Different and same trunks in Trunk mode

##### Test pass criteria for Test Case 4 Different and same trunks in Trunk mode
Ping from host1 to host2 should be successful **only** when both of the ports have **same trunks**=100.

## Sampled Flow
### Objective for sFlow
The test case checks whether sFlow gets successfully configured in the ASIC OVS through OpenSwitch CLI.
It also verifies if the configuration gets correctly written to the "hsflowd" daemon which is responsible in sending samples and statistics to the collector.

### Requirements for sFlow
- Virtual Mininet test setup
- **CT File**: ops-switchd-container-plugin/tests/test\_switchd\_container\_ct\_sflow.py


### Setup topology diagram for sFlow
Single switch topology
```ditaa
+-------------+
|             |
|    Switch   |
|             |
|             |
+-------------+

```
### Test Case for sFlow
#### Description of Test Case for sFlow
1. Configure an interface with IP address and bring it up
2. Enable sflow globally
3. Configure some sampling rate, agent interface as the interface configured above
4. Configure the collector IP address and the polling interval as shown below
```
switch(config)# interface 1
switch(config-if)# ip address 10.10.10.1/24
switch(config-if)# no shutdown
switch(config-if)# exit
switch(config)# sflow enable
switch(config)# sflow sampling 100
switch(config)# sflow agent-interface 1
switch(config)# sflow collector 10.10.10.2
switch(config)# sflow polling 10
```
Verify the correct setting of the sFlow configuration in the ASIC OVS using the following command
```
/opt/openvswitch/bin/ovs-vsctl list sFlow
```
Also, verify if the configuration is correctly written in hsflowd daemon configuration file
```
cat /etc/hsflowd.conf
```
#### Test result criteria for sFlow
##### Test pass criteria for sFlow
The sFlow configuration set through CLI should be seen in ASIC OVS as communication between switch and other devices happens through ASIC OVS.
The same configuration should also be seen in "hsflowd.conf" file as it is responsible to send samples and other statistics to the right collector with configured sampling and polling rates.
Correct setting of configuration at both the places ensures smooth functionality of sFlow.

##### Test fail criteria for sFlow
Failure in setting of sFlow configuration in ASIC OVS or hsflowd.conf file results in unpredictable functionality of sFlow and irregular processing by collector.
