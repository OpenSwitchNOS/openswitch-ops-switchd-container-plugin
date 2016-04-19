/*
 *  (c) Copyright 2016 Hewlett Packard Enterprise Development LP
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License. You may obtain
 *  a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 */

#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include "openvswitch/vlog.h"
#include "copp-asic-provider.h" //from ops-switchd/plugins
#include "plugin-extensions.h"  //from ops-switchd/plugins

#define MAX_DEV_NAME_LEN 100 /* Just to be on the safe side, make it large */
#define MAX_ROW_LEN 140
#define NUM_IGNORED_DEV_NAMES 3

VLOG_DEFINE_THIS_MODULE(sim_copp_container_plugin);


int sim_copp_stats_get(const unsigned int hw_asic_id,
                      const enum copp_protocol_class class,
                      struct copp_protocol_stats *const stats);

int sim_copp_hw_status_get(const unsigned int hw_asic_id,
                          const enum copp_protocol_class class,
                          struct copp_hw_status *const hw_status);

/* Local helper function prototypes */
void parse_copp_stats(char *srcData, struct copp_protocol_stats *const stats);
bool isIgnoreRow(char *rowString, char *ignoreNames[], int listSize);

struct copp_asic_plugin_interface copp_interface ={
     /* The new functions that need to be exported, can be declared here*/
     .copp_stats_get = &sim_copp_stats_get,
     .copp_hw_status_get = &sim_copp_hw_status_get,
 };

void sim_copp_init(void) {

     struct plugin_extension_interface copp_extension;

     copp_extension.plugin_name = COPP_ASIC_PLUGIN_INTERFACE_NAME;
     copp_extension.major = COPP_ASIC_PLUGIN_INTERFACE_MAJOR;
     copp_extension.minor = COPP_ASIC_PLUGIN_INTERFACE_MINOR;
     copp_extension.plugin_interface = (void *)&copp_interface;

     if (register_plugin_extension(&copp_extension)) {
         VLOG_ERR("COPP plugin init failed");
     } else {
         VLOG_INFO("COPP plugin init succeeded");
     }
     return;
}

/* API for providing CoPP statistics based on interface packet
 * counts in /proc/net/dev of namespace swns
 */
 int sim_copp_stats_get(const unsigned int hw_asic_id,
                       const enum copp_protocol_class class,
                       struct copp_protocol_stats *const stats) {

    int rc = 0;
    int myPid = 0;
    int rowNum = 0;
    size_t readSize = MAX_ROW_LEN;
    ssize_t nRead = 0;
    FILE *devFile;
    char *rowStr = malloc(MAX_ROW_LEN);

    VLOG_DBG("%s: Entry()", __FUNCTION__);
    VLOG_DBG("%s: hw_asic_id is %d, Protocol_Class: %d", __FUNCTION__, hw_asic_id, class);
    /* look at /proc/net/dev and sum the rx packets, bytes, and dropped packets.  Set
     * bytes dropped to UNIT64_MAX since it's unsupported.
       We should be executing under ops-switchd so we have access to swns
       namespace for /proc/net/dev
     */

    if (class != COPP_DEFAULT_UNKNOWN) {
        /* Container doesn't support separating stats by classes */
        return EOPNOTSUPP;
    }
    stats->bytes_passed = 0;
    stats->packets_passed = 0;
    stats->packets_dropped = 0;

    devFile = fopen("/proc/net/dev", "r");

    if (devFile == NULL) {
        VLOG_ERR("%s: Failed to open /proc/net/dev for copp stats, error '%s'",
                    __FUNCTION__, strerror(errno));
        free(rowStr);
        return errno;
    }

    do {
        rowNum++;
        nRead = getline(&rowStr, &readSize, devFile);
        if (nRead < 0) {
            if (rowNum <= 1)
            {
                VLOG_ERR("%s: No data in /proc/net/dev for copp stats", __FUNCTION__);
                free(rowStr);
                fclose(devFile);
                return ENODATA;
            }
            break;
        }
        if (rowNum < 3) {
            /* skip rows 1 and 2, column headings */
            continue;
        }
        parse_copp_stats(rowStr, stats);
    } while (!feof(devFile));

    free(rowStr);
    fclose(devFile);

    /* Unsupported - bytes_dropped */
    stats->bytes_dropped = UINT64_MAX;

    return rc;
}

 /* Parse a single row of stats from /proc/net/dev and add it to
  * the stats summary
  */
void parse_copp_stats(char *rowStr, struct copp_protocol_stats *const stats)
{
    char *rowIgnoreList[NUM_IGNORED_DEV_NAMES] = {"ovs-netdev:",
                                "bridge_normal:","lo:"};
    char devName[MAX_DEV_NAME_LEN];
    unsigned long rxBytes, rxPkts, rxErrs, rxDrop;

    /* Assumptions: the file content we're parsing is fixed-format
     *   Column 1 = Port Name (number)
     *   Column 2 = Rx Bytes
     *   Column 3 = Rx Packets
     *   Column 4 = Rx Errors (not used)
     *   Column 5 = Rx drops
     *
     *   Space is column separator (0x20)
     */

    sscanf(rowStr, "%s%lu%lu%lu%lu", &devName, &rxBytes, &rxPkts, &rxErrs, &rxDrop);
    if (isIgnoreRow(devName, rowIgnoreList, NUM_IGNORED_DEV_NAMES)) {
        VLOG_DBG("%s: ignoring row %s", __FUNCTION__, devName);
        return;
    }
    VLOG_DBG("%s: parsed row %s, rxBytes: %d rxPkts: %d rxErrs: %d rxDrops: %d",
            __FUNCTION__, devName, rxBytes, rxPkts, rxErrs, rxDrop);
    stats->bytes_passed += rxBytes;
    stats->packets_passed += rxPkts;
    stats->packets_dropped += rxDrop;
}

/* Check if the name of the row from /proc/net/dev is an
 * interface that we should ignore for adding to the stats
 */
bool isIgnoreRow(char *name, char *ignoreNames[], int listSize)
{
    for (int i = 0; i < listSize; i++) {
        if (!strncmp(name, ignoreNames[i], MAX_DEV_NAME_LEN)) {
            return true;
        }
    }
    return false;
}

/* API to provide copp hw status values */
int sim_copp_hw_status_get(const unsigned int hw_asic_id,
                           const enum copp_protocol_class class,
                           struct copp_hw_status *const hw_status) {

     int rc = 0;

    VLOG_DBG("%s: Entry()", __FUNCTION__);

    if (class != COPP_DEFAULT_UNKNOWN) {
        /* Container doesn't support separating stats by classes */
        return EOPNOTSUPP;
    }

    hw_status->rate = 1000000000;
    hw_status->burst = 1000000000;
    hw_status->local_priority = 0;

    return rc;
 }
