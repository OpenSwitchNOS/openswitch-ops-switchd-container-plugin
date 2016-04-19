# High level design of ops-switchd-container-plugin (OpenSwitch simulation)


## Contents

- [Description](#description)
- [Responsibilities](#responsibilities)
- [Design choices](#design-choices)
- [Relationship between OpenSwitch and Open vSwitch](#relationship-between-openswitch-and-open-vswitch)
- [Internal structure](#internal-structure)
	- [netdev simulation provider](#netdev-simulation-provider)
		- [netdev simulation key data structures](#netdev-simulation-key-data-structures)
		- [netdev simulation provider class functions](#netdev-simulation-provider-class-functions)
	- [ofproto simulation provider](#ofproto-simulation-provider)
		- [ofproto simulation provider plugin](#ofproto-simulation-provider-plugin)
		- [ofproto simulation provider data structure](#ofproto-simulation-provider-data-structure)
		- [ofproto simulation provider key class functions](#ofproto-simulation-provider-key-class-functions)
- [References](#references)

## Description

The OpenSwitch simulation enables component and feature test in a pure software simulation environment without a need for any physical network setup.

Target users include developers, testers and continuous integration systems. The simulation is especially useful, when dealing with protocol daemons, configuration interfaces, system components and other key portions of the code. The simulation is of little benefit for testing components, such as an actual data plane or platform devices (fans, LEDs and sensors). Hence, it does not simulate any of these aspects of the product.

OpenSwitch controls and programs the forwarding plane device ("ASIC") by using an ofproto provider class to manage L2 and L3 features, as well as a netdev class to manage physical interfaces. The class functions are invoked by the bridge software, and they abstract the forwarding device implementation.

In the case of simulation, the forwarding device is an Open vSwitch (OVS), which acts as a forwarding "ASIC". The simulation provider programs the target OVS by executing `ovs-vsctl` CLI commands.

The simulation environment consists of a Docker namespace framework running Mininet. Yocto build systems create a Docker image target that extends a Mininet-like environment. Mininet allows the instantiation of switches and hosts. It also supports the connection setup between any host/switch port to any host/switch port. The Docker/Mininet environment is very scalable, and it allows the simultaneous testing of complex topologies in a virtual environment.

## Responsibilities

The simulation provider implements control path class functions to manage the simulated "ASIC". It also programs IP tables to provide L3 interface support by the Linux kernel. It currently, supports creation, deletion and modification
of a bridge, a port, LAG and LACP. It also supports VLAN modes, inter-VLAN routing and a per port VLAN bitmap. The VLAN bitmap is used to make sure only active VLANs are programmed into the target OVS.

## Design choices

The design selected an Open vSwitch as a forwarding plane since it is a full feature software switch implementation that is easy to manage via `ovs-vsctl` and `ovs-ofctl` commands. OpenSwitch had to implement provider interface functions to manage a real "ASIC"-based switch. It made sense to leverage this model for developing the simulation.

The Docker/Mininet framework was selected because the virtual machine based simulation proved too difficult to deploy and manage. Docker provides a lightweight scalable virtualization, which is critical to regression testing and continuous
integration. Mininet provides a simple and powerful framework to develop networking tests using Python scripts which execute either in simulation or on real hardware.

## Relationship between OpenSwitch and Open vSwitch

```ditaa
+---------------------------------------------------------------------------+
|                       OpenSwitch namespace                                |
|                                                                           |
|+-------------------------------------------------------------------------+|
||                                                                         ||
||                         OVSDB-Server                                    ||
||                                                                         ||
|+-------------------------------------------------------------------------+|
|     ^                       ^                      ^             ^        |
|     |                       |                      |             |        |
|     V                       V                      V             V        |
|+------------+  +-----------------------------+  +---------+  +-----------+|
||Mgmt Daemons|  | Derivation of ovs-vswitchd  |  | System  |  |   L2/L3   ||
||CLI, Rest,  |  |                             |  | Daemons |  |  Daemons  ||
||WebUI       |  +-----------------------------+  |         |  |           ||
||            |  | Simulation ofproto/netdev   |  |         |  |           ||
||            |  |      Providers              |  |         |  |           ||
||            |  |                             |  |         |  |           ||
||            |  |   ovs-vsctl   ovs-ofctl     |  |         |  |           ||
|+------------+  +-----------------------------+  +---------+  +-----------+|
|                       |            |                              ^       |
|                       |            |                              |       |
+---------------------------------------------------------------------------+
                        |            |                              |
                        |            |                              |
+---------------------------------------------------------------------------+
|                       V            V                              V       |
|                  +-------------------------+    +--------+  +-----------+ |
|                  |      ovs-vswitchd       |    | OVSDB- |  | Linux     | |
| OpenvSwitch      |                         |<-->| Server |  | Kernel    | |
|                  |                         |    |        |  | Namespace | |
|                  +-------------------------+    +--------+  +-----------+ |
|                                                              | | | | |    |
+---------------------------------------------------------------------------+

                                                               | | | | |
                                                               | | | | |
                                                               Interfaces
                                                                 1 - N
```

## Internal structure

### netdev simulation provider

Netdev is an interface (i.e. physical port) class that consists of data structures and interface functions. Netdev simulation class manages a set of Linux interfaces that emulate switch data path interfaces. The bridge (`bridge.c`) instantiates the class by mapping a generic set of netdev functions to netdev simulation functions. `vswitchd`, will then, manage switch interfaces by invoking these class functions. Netdev configures Linux kernel interfaces by constructing CLI commands and invoking system(cmd) to execute these commands. It also maintains local state for supporting class functions.


#### netdev simulation key data structures

```
struct netdev_sim {
    struct netdev up;
    struct ovs_list list_node OVS_GUARDED_BY(sim_list_mutex);
    struct ovs_mutex mutex OVS_ACQ_AFTER(sim_list_mutex);
    uint8_t hwaddr[ETH_ADDR_LEN] OVS_GUARDED;
    char hw_addr_str[18];
    struct netdev_stats stats OVS_GUARDED;
    enum netdev_flags flags OVS_GUARDED;
    char linux_intf_name[16];
    int link_state;
    uint32_t hw_info_link_speed;
    uint32_t link_speed;
    uint32_t mtu;
    bool autoneg;
    bool pause_tx;
    bool pause_rx;
};
```

#### netdev simulation provider class functions

* `netdev_sim_alloc`              - Allocates a netdev object.
* `netdev_sim_construct`          - Assigns a dynamic MAC address and adds to netdev linked list.
* `netdev_sim_destruct`           - Removed netdev object from linked list.
* `netdev_sim_dealloc`            - Frees up netdev object.
* `netdev_sim_set_hw_intf_info`   - Interface bringup with a given MAC address. Other interface parameters are ignored.
* `netdev_sim_set_hw_intf_config` - Enable/Disable interface.
* `netdev_sim_set_etheraddr`      - Sets up a MAC address.
* `netdev_sim_get_etheraddr`      - Reports a MAC address.
* `netdev_sim_get_carrier`        - Reports link state.
* `netdev_sim_get_stats`          - Reports interface stats.
* `netdev_sim_dump_queue_stats`   - Reports simulated QOS queue stats.
* `netdev_sim_get_features`       - Reports interface features.
* `netdev_sim_update_flags`       - Updates interface flags.



### ofproto simulation provider
-------------------------------

`Ofproto` is a port (i.e. logical port) class which consists of data structures and port interface functions. The simulation provider supports L2 and L3 interfaces. The simulation provider works in conjunction with protocol daemons to provide control path support for VLAN, LAG, LACP and Inter-VLAN routing. Additional protocols including VXLAN, QOS, ACLs, security well as open flow will be added in the future. All process communications between protocol daemons, UI and the simulation class is done via OVSDB. Configuration requests are triggered by switchd invoking class functions.

`bridge.c` instantiates the class by mapping a generic set of ofproto provider functions to ofproto simulation functions. `vswitchd`, will then, manage switch ports by invoking these class functions.

The simulation provider programs the "ASIC" target OVS by executing `ovs-vsctl` and `ovs-ofctl` CLI commands. Its also tracking state for its managed objects to allow additions, modifications and deletions. A class function creates a
CLI command by filling up a string buffer with the CLI command and, if needed, incrementally, adding all of its parameters. Once the CLI buffer is ready, the code invoke the `system(cli_buf)` command to execute it.

The bridge configures a logical port by calling `bundle_set()` and `bundle_remove()`. The simulation decides when and how to configure the target OVS based on member ports and VLAN state. The simulations brings up an access port, only when the interface and the access VLAN are up. The simulation disables that port when either the interface or the access VLAN is disabled. The simulation configures a trunk port with a subset of its enabled VLANS (assuming at least one).  Similarly, a bond (LAG) is configured with any subset of its enabled ports (interfaces). `bundle_configure()` gets invoked whenever a port and/or a VLAN state/mode is changed which triggers a reconfiguration CLI request. When the last member port, in a bond, is removed, the bond itself is deleted.


#### ofproto simulation provider plugin
---------------------------------------

The `ofproto` class functions are loaded dynamically via a plugin. It allows flexibility in terms of which API to package as well as avoids licensing issues caused by shipping proprietary APIs. The class functions load before `ofproto` invokes any of the class functions. The plugin key function is `ofproto_register()` that maps `ofproto_sim_provider_class`.

#### ofproto simulation provider data structure
-----------------------------------------------

* `struct sim_provider_node`   - Generic ofproto port data.
* `struct sim_provider_ofport` - Simulation port data.
* `struct ofbundle`            - Simulation Bond/vlan data structure.

#### ofproto simulation provider key class functions
----------------------------------------------------


* `enumerate_types`      - Report datapath types: system and vrf.
* `enumerate_names`      - Report bridge names that match supported types.
* `port_open_type`       - Report port type: system or internal.
* `alloc`                - Allocate memory for ofproto.
* `construct`            - Multiple functions:
    - Create a bridge in target OVS ("ASIC") and set.
    - Datapath to netdev to allow kernel forwarding.
    - Create a port with same name.
    - Bring up the bridge in data path name space (swns).
    - Initialize `ofproto` data structures.
    - Allocate an empty VLAN bitmap.
    - Add `ofproto` to a global hash map.
* `destruct`             - Remove bridge from target OVS and free up internal structures. Remove ofproto from global hash map.
* `dealloc`              - Free up ofproto memory.
* `port_alloc`           - Allocate port memory.
* `port_dealloc`         - Deallocate port memory.
* `bundle_set`           - Create vlan/bond in target OVS and update state.
* `bundle_configure`     - Apply port and mode changes to vlan/bond.
* `bundle_remove`        - Remove a port from a bond and reconfigure target OVS.
* `set_vlan`             - Enable/disable a VLAN after a VLAN change. Scan all ports in the bridge; Reconfigure if needed.
* `get_datapath_version` - Get datapath version.

## References
-------------
* [OpenSwitch](http://www.openswitch.net/)
* [Open vSwitch](http://www.openvswitch.org/)
* [Docker](http://www.docker.com/)
* [Mininet](http://www.mininet.org/)
