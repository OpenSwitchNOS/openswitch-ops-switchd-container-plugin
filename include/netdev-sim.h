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

#ifndef NETDEV_SIM_H
#define NETDEV_SIM_H 1

#include "netdev-provider.h"

#define STR_EQ(s1, s2)      ((s1 != NULL) && (s2 != NULL) && \
                             (strlen((s1)) == strlen((s2))) && \
                             (!strncmp((s1), (s2), strlen((s2)))))

#define MAX_CMD_LEN     2048
#define SWNS_EXEC       "/sbin/ip netns exec swns"

/* SIM provider API. */
void netdev_sim_register(void);
extern int netdev_sim_get_hw_id(struct netdev *netdev);
extern void netdev_sflow_reset(struct netdev *netdev);
extern void netdev_sflow_stats_enable(struct netdev *netdev, bool enabled);
extern void netdev_sim_l3stats_xtables_rules_create(struct netdev *netdev);
extern void netdev_sim_l3stats_xtables_rules_delete(struct netdev *netdev);
#endif /* netdev-sim.h */
