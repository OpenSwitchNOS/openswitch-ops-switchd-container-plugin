
Switchd Container Plugin Test Cases
==============================

[TOC]

##  Port in access VLAN mode
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal ASIC OVS and set the configurations correctly in both the OVS'. Ports in Access Mode should get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective ports with matching tags should get deleted from the internal OVS. Following test cases will verify ping traffic in different configurations.
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test_switchd_container_ct_vlan_access.py

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
### Test Case 1:  Verify Correct Driving of Internal ASIC OVS by OpenSwitch OVS
###Description
Add Bridge 'br0' on the Switch
Add VLAN100 to global VLAN table on the Switch
Add port 1 with tag=100 on Switch in access mode
Verify whether same configuration gets set on the ASIC simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
```
### Test Result Criteria
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal ASIC OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding port 1 to VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal ASIC OVS.
The interface 1 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal ASIC OVS.

### Test Case 2:  Adding VLAN & Ports and Pass Traffic
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful

### Test Case 3:  With and Without Global VLAN
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when VLAN is present

### Test Case 4:  Different and Same Tags
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when both the ports have **same tag**=100

##  Port in trunk VLAN mode
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal ASIC OVS and set the configurations correctly in both the OVS'. Ports in Trunk Mode should get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective ports with matching trunks should get deleted from the internal OVS. Following test cases will verify ping traffic in different configurations.
### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test_switchd_container_ct_vlan_trunk.py

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
### Test Case 1:  Verify Correct Driving of Internal ASIC OVS by OpenSwitch OVS
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add port 2 with trunks=200 on Switch1 in trunk mode
Verify whether same configuration gets set on the ASIC simulating Internal OVS

Run commands:
```
/usr/bin/ovs-vsctl add-br br0
/usr/bin/ovs-vsctl add-vlan br0 100 admin=up
/usr/bin/ovs-vsctl add-port br0 1 vlan_mode=access tag=100
/usr/bin/ovs-vsctl set interface 1 user_config:admin=up
/usr/bin/ovs-vsctl add-port br0 2 vlan_mode=trunk trunks=100
/usr/bin/ovs-vsctl set interface 2 user_config:admin=up
```
### Test Result Criteria
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal ASIC OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding port 2 to VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal ASIC OVS.
The interface 2 should be up and the MAC address and admin state should be same in OpenSwitch OVS and Internal ASIC OVS.

### Test Case 2:  Adding VLAN & Ports and Pass Traffic
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful

### Test Case 3:  With and Without Global VLAN
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when VLAN is present

### Test Case 4:  Different and Same Tags
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
### Test Result Criteria
#### Test Pass Criteria
Ping from host1 to host2 should be successful **only** when both the ports have **same trunks**=100

##  Bond in access VLAN mode
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal ASIC OVS and set the configurations correctly in both the OVS'. Bonds in Access Mode should get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective bonds with matching tags should get deleted from the internal OVS.

####**NOTE**: Currently the Internal ASIC OVS does not support static LAG. Hence, testing ping traffic for LAG configurations is not possible. As of now, we support only configuration checks for Bonds.

### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test_switchd_container_ct_lag_access.py

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
### Test Case 1:  Verify Correct Driving of Internal ASIC OVS by OpenSwitch OVS
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add  lag0 (port 2,3) with tag=100 on Switch1 in access mode
Verify whether same configuration gets set on the ASIC simulating Internal OVS

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
### Test Result Criteria
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal ASIC OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding lag0 to VLAN, the name, tag and VLAN information should be same in OpenSwitch OVS and Internal ASIC OVS.
The interface 2 and 3 should be up and the MAC addresses' and admin states should be same in OpenSwitch OVS and Internal ASIC OVS.

##  Bond in trunk VLAN mode
### Objective
Test case checks whether OpenSwitch OVS is able to drive the Internal ASIC OVS and set the configurations correctly in both the OVS'. Bonds in Trunk Mode should get configured only when corresponding VLAN is added. If the VLAN gets deleted, respective bonds with matching trunks should get deleted from the internal OVS.

####**NOTE**: Currently the Internal ASIC OVS does not support static LAG. Hence, testing ping traffic for LAG configurations is not possible. As of now, we support only configuration checks for Bonds.

### Requirements
- Virtual Mininet Test Setup
- **CT File**: ops-switchd-sim-plugin/tests/test_switchd_container_ct_lag_trunk.py

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
### Test Case 1:  Verify Correct Driving of Internal ASIC OVS by OpenSwitch OVS
###Description
Add Bridge 'br0' on the Switch1
Add VLAN100 to global VLAN table on Switch1
Add port 1 with tag=100 on Switch1 in access mode
Add  lag0 (port 2,3) with trunks=100 on Switch1 in trunk mode
Verify whether same configuration gets set on the ASIC simulating Internal OVS

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
### Test Result Criteria
#### Test Pass Criteria
Bridge 'br0' should get created in OpenSwitch OVS and Internal ASIC OVS
VLAN100 should get created in  OpenSwitch OVS and admin=up
After adding lag0 to VLAN, the name, trunk and VLAN information should be same in OpenSwitch OVS and Internal ASIC OVS.
The interface 2 and 3 should be up and the MAC addresses' and admin states should be same in OpenSwitch OVS and Internal ASIC OVS.
