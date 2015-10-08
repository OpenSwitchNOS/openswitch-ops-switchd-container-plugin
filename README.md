OPS-SWITCHD-CONTAINER-PLUGIN
============================

What is ops-switchd-container-plugin ?
--------------------------------------
ops-switchd-container-plugin is part of the virtual software based test
infrastructure of OpenSwitch.
OpenSwitch simulation enables component and feature test in a pure software
simulation environment without a need for any physical network setup.
OpenSwitch controls and programs the forwarding plane device ("ASIC")
using an ofproto provider class to manage L2 features, as well as a
netdev class to manage physical interfaces. The class functions are invoked
by the bridge software, and abstract the forwarding device implementation.
The simulation provider uses an open source OVS to mimic a forwarding device.
The simulation provider programs the target OVS by executing ovs-vsctl CLI
commands. In the future, the simulation provider will also use ovs-ofctl,
open flow commands, to implement L2 features which cannot be implemented by
ovs-vsctl.


What is the structure of the repository?
----------------------------------------
* src - contains all c source files.
* include - contains all c header files.
* tests - contains sample python tests.
* build - contains cmake build files.

What is the license?
--------------------
Apache 2.0 license. For more details refer to [COPYING](http://git.openswitch.net/cgit/openswitch/ops-switchd-container-plugin/tree/COPYING)

What other documents are available?
-----------------------------------
For the high level design of ops-switchd-container-plugin, refer to [DESIGN.md](http://git.openswitch.net/cgit/openswitch/ops-switchd-container-plugin/tree/DESIGN.md)
For answers to common questions, read [FAQ.md](http://git.openswitch.net/cgit/openswitch/ops-switchd-container-plugin/tree/FAQ.md)
For what has changed since last release, refer to [NEWS](http://git.openswitch.net/cgit/openswitch/ops-switchd-container-plugin/tree/NEWS)
For the current list of contributors and maintainers, refer to [AUTHORS.md](http://git.openswitch.net/cgit/openswitch/ops-switchd-container-plugin/tree/AUTHORS.md)

For general information about OpenSwitch project refer to http://www.openswitch.net
