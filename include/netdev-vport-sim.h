/*
 * Copyright (c) 2010, 2011, 2012, 2013 Nicira, Inc.
 * Copyright (C) 2015 Hewlett-Packard Development Company, L.P.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef NETDEV_VPORT_SIM_H
#define NETDEV_VPORT_SIM_H 1

#include <stdbool.h>
#include <stddef.h>

struct dpif_netlink_vport;
struct dpif_flow_stats;
struct netdev;
struct netdev_class;
struct netdev_stats;

/* SIM provider API. */
void netdev_vport_sim_register(void);
void netdev_vport_patch_register(void);
//extern int netdev_sim_get_hw_id(struct netdev *netdev);

#endif /* netdev-sim.h */
