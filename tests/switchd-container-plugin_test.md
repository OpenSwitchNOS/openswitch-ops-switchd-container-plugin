
Switchd Container Plugin Test Cases
==============================
#Contents
- [Port in access VLAN mode](#port-in-access-vlan-mode)
	- [Objective](#objective1)
	- [Requirements](#requirements1)
	- [Setup Topology Diagram](#setup-topology-diagram1)
	- [Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Description](#description1)
	- [Test Result Criteria](#test-result-criteria1)
		- [Test Pass Criteria](#test-pass-criteria1)
	- [Test Case 2-Adding VLAN and Ports and Pass Traffic](#test-case-2-adding-vlan-and-ports-and-pass-traffic)
	- [Description](#description2)
	- [Test Result Criteria](#test-result-criteria2)
		- [Test Pass Criteria](#test-pass-criteria2)
	- [Test Case 3-With and Without Global VLAN](#test-case-3-with-and-without-global-vlan)
	- [Description](#description3)
	- [Test Result Criteria](#test-result-criteria3)
		- [Test Pass Criteria](#test-pass-criteria3)
	- [Test Case 4-Different and Same Tags](#test-case-4-different-and-same-tags)
	- [Description](#description4)
	- [Test Result Criteria](#test-result-criteria4)
		- [Test Pass Criteria](#test-pass-criteria4)
- [Port in trunk VLAN mode](#port-in-trunk-vlan-mode)
	- [Objective](#objective2)
	- [Requirements](#requirements2)
	- [Setup Topology Diagram](#setup-topology-diagram2)
	- [Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Description](#description5)
	- [Test Result Criteria](#test-result-criteria5)
		- [Test Pass Criteria](#test-pass-criteria5)
	- [Test Case 2-Adding VLAN and Ports and Pass Traffic](#test-case-2-adding-vlan-and-ports-and-pass-traffic)
	- [Description](#description6)
	- [Test Result Criteria](#test-result-criteria6)
		- [Test Pass Criteria](#test-pass-criteria6)
	- [Test Case 3-With and Without Global VLAN](#test-case-3-with-and-without-global-vlan)
	- [Description](#description7)
	- [Test Result Criteria](#test-result-criteria7)
		- [Test Pass Criteria](#test-pass-criteria7)
	- [Test Case 4-Different and Same Trunks](#test-case-4-different-and-same-trunks)
	- [Description](#description8)
	- [Test Result Criteria](#test-result-criteria8)
		- [Test Pass Criteria](#test-pass-criteria8)
- [Bond in access VLAN mode](#bond-in-access-vlan-mode)
	- [Objective](#objective3)
	- [NOTE](#note1)
	- [Requirements](#requirements3)
	- [Setup Topology Diagram](#setup-topology-diagram3)
	- [Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Description](#description9)
	- [Test Result Criteria](#test-result-criteria9)
		- [Test Pass Criteria](#test-pass-criteria9)
- [Bond in trunk VLAN mode](#bond-in-trunk-vlan-mode)
	- [Objective](#objective4)
	- [NOTE](#note2)
	- [Requirements](#requirements4)
	- [Setup Topology Diagram](#setup-topology-diagram4)
	- [Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS](#test-case-1-verify-correct-driving-of-internal-asic-ovs-by-openswitch-ovs)
	- [Description](#description10)
	- [Test Result Criteria](#test-result-criteria10)
		- [Test Pass Criteria](#test-pass-criteria10)

##  Port in access VLAN mode
<div id='objective1'/>
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS'. Ports in Access Mode gets configured only when corresponding VLAN is enabled in the VLAN table. If the VLAN gets deleted, respective ports with matching tags get deleted from the Internal ""ASIC"" OVS. Following test cases will verify ping traffic between hosts via switch in different configurations.
<div id='requirements1'/>
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_vlan\_access.py
<div id='setup-topology-diagram1'/>
### Setup Topology Diagram
Single Switch Topology
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
### Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS
<div id='description1'/>
###Description
Add Bridge 'br0' on the Switch
Add VLAN100 to global VLAN table on the Switch
Add port 1 with tag=100 on Switch in access mode
Verify whether same configuration gets set on the "ASIC" simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
```
<div id='test-result-criteria1'/>
### Test Result Criteria
<div id='test-pass-criteria1'/>
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal "ASIC" OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding port 1 to VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
The interface 1 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2-Adding VLAN and Ports and Pass Traffic
<div id='description2'/>
###Description
Add Bridge 'br0' on the Switch
Add VLAN100 to global VLAN table on the Switch
Add port 1 and 2 with tag=100 on Switch in access mode
Verify whether host1 can ping host2

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```
<div id='test-result-criteria2'/>
### Test Result Criteria
<div id='test-pass-criteria2'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful

### Test Case 3-With and Without Global VLAN
<div id='description3'/>
###Description
Add Bridge 'br0' on the Switch
Do NOT add VLAN100 to global VLAN table on the Switch
Add port 1 and 2 with tag=100 on Switch in access mode
Verify whether host1 can ping host2
Now add VLAN100 to global VLAN table on the Switch
Verify whether host1 can ping host2

Run commands:
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
<div id='test-result-criteria3'/>
### Test Result Criteria
<div id='test-pass-criteria3'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when VLAN is enabled

### Test Case 4-Different and Same Tags
<div id='description4'/>
###Description
Add Bridge 'br0' on the Switch
Add VLAN100 to global VLAN table on the Switch
Add port 1 with tag=100 and port 2 with tag=200 on Switch in access mode
Verify whether host1 can ping host2
Now re-add port 2 with tag=100
Verify whether host1 can ping host2

Run commands:
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
<div id='test-result-criteria4'/>
### Test Result Criteria
<div id='test-pass-criteria4'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when both the ports have **same tag**=100

##  Port in trunk VLAN mode
<div id='objective2'/>
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS'. Ports in Trunk Mode get configured only when corresponding VLAN is enabled in the VLAN table. If the VLAN gets deleted, respective ports with matching trunks get  deleted from the internal OVS. Following test cases will verify ping traffic between hosts via both the switches in different configurations.
<div id='requirements2'/>
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_vlan\_trunk.py
<div id='setup-topology-diagram2'/>
### Setup Topology Diagram
Dual Switch Topology
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
### Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS
<div id='description5'/>
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add port 2 with trunks=200 on Switch1 in trunk mode
Verify whether same configuration gets set on the "ASIC" simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```
<div id='test-result-criteria5'/>
### Test Result Criteria
<div id='test-pass-criteria5'/>
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal "ASIC" OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding port 2 to VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
The interface 2 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal "ASIC" OVS.

### Test Case 2-Adding VLAN & Ports and Pass Traffic
<div id='description6'/>
###Description
Add Bridge 'br0' on both the switches
Add VLAN100 to global VLAN table on both the switches
Add port 1 with tag=100 in access mode on both the switches
Add port 2 with trunks=100 in trunk mode on both the switches
Verify whether host1 can ping host2

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```
<div id='test-result-criteria6'/>
### Test Result Criteria
<div id='test-pass-criteria6'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful

### Test Case 3-With and Without Global VLAN
<div id='description7'/>
###Description
Add Bridge 'br0' on both the switches
Do NOT add VLAN100 to global VLAN table on both the switches
Add port 1 with tag=100 in access mode on both the switches
Add port 2 with trunks=100 in trunk mode on both the switches
Verify whether host1 can ping host2
Now add VLAN100 to global VLAN table on both the switches
Verify whether host1 can ping host2

Run commands:
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
<div id='test-result-criteria7'/>
### Test Result Criteria
<div id='test-pass-criteria7'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when VLAN is enabled

### Test Case 4-Different and Same Trunks
<div id='description8'/>
###Description
Add Bridge 'br0' on both the switches
Add VLAN100 to global VLAN table on both the switches
Add port 1 with tag=100 in access mode on both the switches
Add port 2 with trunks=100 in trunk mode on Switch1 and trunks=200 on Switch2
Verify whether host1 can ping host2
Now re-add port 2 on Switch2 with trunks=100
Verify whether host1 can ping host2

Run commands:
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
<div id='test-result-criteria8'/>
### Test Result Criteria
<div id='test-pass-criteria8'/>
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when both the ports have **same trunks**=100

##  Bond in access VLAN mode
<div id='objective3'/>
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS'. Bonds in Access Mode get configured only when corresponding VLAN is enabled in the VLAN table. If the VLAN gets deleted, respective bonds with matching tags get deleted from the internal OVS.
<div id='note1'/>
####**NOTE**
- Currently the Internal "ASIC" OVS does not support static LAG. Hence, testing ping traffic for LAG configurations is not possible. As of now, we support only configuration checks for Bonds.
<div id='requirements3'/>
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_lag\_access.py
<div id='setup-topology-diagram3'/>
### Setup Topology Diagram
Dual Switch Topology
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
### Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS
<div id='description9'/>
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add  lag0 (port 2,3) with tag=100 on Switch1 in access mode
Verify whether same configuration gets set on the "ASIC" simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
/usr/bin/ovs-vsctl set interface 3 user_config:admin=up
```
<div id='test-result-criteria9'/>
### Test Result Criteria
<div id='test-pass-criteria9'/>
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal "ASIC" OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding lag0 to VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
The interface 2 and 3 should be up and the MAC addresses' and admin states should be same in OpenSwitch OVS and Internal "ASIC" OVS.

##  Bond in trunk VLAN mode
<div id='objective4'/>
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal "ASIC" OVS and set the configurations correctly in both the OVS'. Bonds in Trunk Mode gets configured only when corresponding VLAN is enabled in the VLAN table. If the VLAN gets deleted, respective bonds with matching trunks get deleted from the internal OVS.
<div id='note2'/>
####**NOTE**
- Currently the Internal "ASIC" OVS does not support static LAG. Hence, testing ping traffic for LAG configurations is not possible. As of now, we support only configuration checks for Bonds.
<div id='requirements4'/>
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test\_switchd\_container\_ct\_lag\_trunk.py
<div id='setup-topology-diagram4'/>
### Setup Topology Diagram
Dual Switch Topology
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
### Test Case 1-Verify Correct Driving of Internal "ASIC" OVS by OpenSwitch OVS
<div id='description10'/>
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add  lag0 (port 2,3) with trunks=100 on Switch1 in trunk mode
Verify whether same configuration gets set on the "ASIC" simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-bond br0 lag0 2 3 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
/usr/bin/ovs-vsctl set interface 3 user_config:admin=up
```
<div id='test-result-criteria10'/>
### Test Result Criteria
<div id='test-pass-criteria10'/>
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal "ASIC" OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding lag0 to VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal "ASIC" OVS.
The interface 2 and 3 should be up and the MAC addresses' and admin states should be same in OpenSwitch OVS and Internal "ASIC" OVS.
