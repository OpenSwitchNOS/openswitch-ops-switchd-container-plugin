
# Switchd Container Plugin Test Cases


## Contents

- [Port in access VLAN mode](#port-in-access-vlan-mode)
	- [Objective](#objective)
	- [Requirements](#requirements)
	- [Setup topology diagram](#setup-topology-diagram)
	- [Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
		- [Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#description-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
		- [Test result criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-result-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
			- [Test pass criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-pass-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Test Case 2 Adding VLAN and ports and pass traffic](#test-case-2-adding-vlan-and-ports-and-pass-traffic)
		- [Description for Test Case 2 Adding VLAN and ports and pass traffic](#description-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
		- [Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic](#test-result-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
			- [Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic](#test-pass-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
	- [Test Case 3 With and without global VLAN](#test-case-3-with-and-without-global-vlan)
		- [Description for Test Case 3 With and without global VLAN](#description-for-test-case-3-with-and-without-global-vlan)
		- [Test result criteria for Test Case 3 With and without global VLAN](#test-result-criteria-for-test-case-3-with-and-without-global-vlan)
			- [Test pass criteria for Test Case 3 With and without global VLAN](#test-pass-criteria-for-test-case-3-with-and-without-global-vlan)
	- [Test Case 4 Different and same tags](#test-case-4-different-and-same-tags)
		- [Description for Test Case 4 Different and same tags](#description-for-test-case-4-different-and-same-tags)
		- [Test result criteria for Test Case 4 Different and same tags](#test-result-criteria-for-test-case-4-different-and-same-tags)
			- [Test pass criteria for Test Case 4 Different and same tags](#test-pass-criteria-for-test-case-4-different-and-same-tags)
- [Port in trunk VLAN mode](#port-in-trunk-vlan-mode)
	- [Objective for port in trunk VLAN mode](#objective-for-port-in-trunk-vlan-mode)
	- [Requirements for port in trunk VLAN mode](#requirements-for-port-in-trunk-vlan-mode)
	- [Setup topology diagram port in trunk VLAN mode](#setup-topology-diagram-port-in-trunk-vlan-mode)
	- [Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
		- [Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#description-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
		- [Test result criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-result-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
			- [Test pass criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS](#test-pass-criteria-for-test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Test Case 2 Adding VLAN and ports and pass traffic](#test-case-2-adding-vlan-and-ports-and-pass-traffic)
		- [Description for Test Case 2 Adding VLAN and ports and pass traffic](#description-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
		- [Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic](#test-result-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
			- [Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic](#test-pass-criteria-for-test-case-2-adding-vlan-and-ports-and-pass-traffic)
	- [Test Case 3 With and without global VLAN](#test-case-3-with-and-without-global-vlan)
		- [Description for Test Case 3 With and without global VLAN](#description-for-test-case-3-with-and-without-global-vlan)
		- [Test result criteria for Test Case 3 With and without global VLAN](#test-result-criteria-for-test-case-3-with-and-without-global-vlan)
			- [Test pass criteria for Test Case 3 With and without global VLAN](#test-pass-criteria-for-test-case-3-with-and-without-global-vlan)
	- [Test Case 4 Different and same trunks](#test-case-4-different-and-same-trunks)
		- [Description for Test Case 4 Different and same trunks](#description-for-test-case-4-different-and-same-trunks)
		- [Test result criteria for Test Case 4 Different and same trunks](#test-result-criteria-for-test-case-4-different-and-same-trunks)
			- [Test pass criteria for Test Case 4 Different and same trunks](#test-pass-criteria-for-test-case-4-different-and-same-trunks)

##  Port in access VLAN mode

### Objective
The test case checks whether the OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both of the OVS's. The ports in Access mode get configured only when a corresponding VLAN is added. If the VLAN gets deleted, the respective ports with matching tags get deleted from the internal OVS. The following test cases verify ping traffic in different configurations.

### Requirements
- Virtual Mininet test setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_vlan\_access.py


### Setup topology diagram
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
### Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS

#### Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS
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

#### Test result criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS

##### Test pass criteria for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS
- Bridge `br0` should get created in OpenSwitch OVS and Internal "ASIC" OVS.
- Create `VLAN100` in OpenSwitch OVS and admin=up.
- After adding port 1 to the VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
- The interface 1 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2 Adding VLAN and ports and pass traffic

#### Description for Test Case 2 Adding VLAN and ports and pass traffic
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

#### Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic

##### Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic
Ping from host1 to host2 should be successful.

### Test Case 3 With and without global VLAN

#### Description for Test Case 3 With and without global VLAN
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

#### Test result criteria for Test Case 3 With and without global VLAN

##### Test pass criteria for Test Case 3 With and without global VLAN
Ping from host1 to host2 should be successful **only** when VLAN is enabled.

### Test Case 4 Different and same tags

#### Description for Test Case 4 Different and same tags
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

#### Test result criteria for Test Case 4 Different and same tags

##### Test pass criteria for Test Case 4 Different and same tags
Ping from host1 to host2 should be successful **only** when both the ports have **same tag**=100.

##  Port in trunk VLAN mode

### Objective for port in trunk VLAN mode
The test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS's. The ports in Trunk mode get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective ports with matching trunks get deleted from the internal OVS. The following test cases verify ping traffic in different configurations.

### Requirements for port in trunk VLAN mode
- Virtual Mininet test setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_vlan\_trunk.py


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
### Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS

#### Description for Test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS
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

#### Test result criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS

##### Test pass criteria for test Case 1 Verify correct driving of internal ASIC OVS by OpenSwitch OVS
- Bridge `br0` should get created in OpenSwitch OVS and Internal "ASIC" OVS.
- `VLAN100` should get created in OpenSwitch OVS and admin=up.
- After adding port 2 to the VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
- The interface 2 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2 Adding VLAN and ports and pass traffic

#### Description for Test Case 2 Adding VLAN and ports and pass traffic
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

#### Test result criteria for Test Case 2 Adding VLAN and ports and pass traffic

##### Test pass criteria for Test Case 2 Adding VLAN and ports and pass traffic
Ping from host1 to host2 should be successful

### Test Case 3 With and without global VLAN

#### Description for Test Case 3 With and without global VLAN
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

#### Test result criteria for Test Case 3 With and without global VLAN

##### Test pass criteria for Test Case 3 With and without global VLAN
Ping from host1 to host2 should be successful **only** when the VLAN is enabled.

### Test Case 4 Different and same trunks

#### Description for Test Case 4 Different and same trunks
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

#### Test result criteria for Test Case 4 Different and same trunks

##### Test pass criteria for Test Case 4 Different and same trunks
Ping from host1 to host2 should be successful **only** when both of the ports have **same trunks**=100.
