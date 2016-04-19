# (c) Copyright 2015 Hewlett Packard Enterprise Development LP
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

project (ovs_sim_plugin)
cmake_minimum_required (VERSION 2.8)
set (SRC_DIR src)
set (INCL_DIR include)

# Define compile flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Werror")

###
### Define sources
###

set (SOURCES ${SRC_DIR}/sim-plugins.c ${SRC_DIR}/netdev-sim.c ${SRC_DIR}/ofproto-sim-provider.c)

###
### Define and locate needed libraries and includes
###
include(FindPkgConfig)
pkg_check_modules(OVSCOMMON REQUIRED libovscommon)


###include_directories(${CMAKE_SOURCE_DIR} ${OVSCOMMON_INCLUDE_DIRS})
include_directories(${CMAKE_SOURCE_DIR} ${OVSCOMMON_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/${INCL_DIR})

###
### Create our library
###

add_library (ovs_sim_plugin SHARED ${SOURCES})

target_link_libraries (ovs_sim_plugin ${OVSCOMMON_LIBRARIES} -lsupportability)

###
### Installation
###
install(TARGETS ovs_sim_plugin
        LIBRARY DESTINATION lib/openvswitch/plugins
    )
