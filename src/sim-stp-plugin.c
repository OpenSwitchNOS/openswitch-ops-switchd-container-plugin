/* Copyright (C) 2016 Hewlett-Packard Development Company, L.P.
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

/* @file switchd_stp.c
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>

#include "hash.h"
#include "hmap.h"
#include "shash.h"
#include "vswitch-idl.h"
#include "openswitch-idl.h"
#include "ofproto/ofproto.h"
#include "openvswitch/vlog.h"
#include "plugin-extensions.h"
#include "asic-plugin.h"
#include "sim-stp.h"
#include <netinet/in.h>
#include <linux/if_bridge.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <errno.h>
#include "unixctl.h"

VLOG_DEFINE_THIS_MODULE(sim_stp_plugin);

#define SYSFS_CLASS_NET "/sys/class/net/"
#define CIST_BR_NAME    "bridge-sim"
#define BR_LINK_TYPE    "bridge"

#define SYSFS_PATH_MAX        256
#define INSTANCE_STRING_LEN 10
#define MAX_CMD_LEN         256

struct hmap all_mstp_instances = HMAP_INITIALIZER(&all_mstp_instances);
const char *port_state_str[] = {"Disabled", "Listening", "Learning",
                                "Blocking", "Forwarding", "Invalid"};
char cmd[MAX_CMD_LEN];

#define RUNCMD(cmd) {\
          if (system(cmd) != 0) {\
              VLOG_ERR("Failed to run the command : %s", cmd);\
              return false;\
          }\
        }

/** @fn int init(int phase_id)
    @brief Initialization of the plugin, needs to be run.
    @param[in] phase_id Indicates the number of times a plugin has been initialized.
    @param[out] ret Check if the plugin was correctly registered.
    @return 0 if success, errno value otherwise.
*/

/*------------------------------------------------------------------------------
| Function:  register_stp_plugins
| Description: register stp sim plugin with switchd
| Parameters[in]: none
| Parameters[out]: none
| Return: none
-----------------------------------------------------------------------------*/
void register_stp_plugins(void)
{

    if (register_reconfigure_callback(stp_reconfigure,
                      BLK_BR_FEATURE_RECONFIG, NO_PRIORITY) != 0)
    {
        VLOG_ERR("Failed to register STP reconfigure in block %d",
                 BLK_BR_FEATURE_RECONFIG);
    }
    else
    {
        VLOG_INFO("STP Reconfgiure registerd in bridge reconfig block %d",
                     BLK_BR_FEATURE_RECONFIG);
    }

    return;
}

/*------------------------------------------------------------------------------
| Function:  br_set
| Description: set a property of the bridge
| Parameters[in]:
| Parameters[out]:
| Return:
-----------------------------------------------------------------------------*/
int br_set(const char *bridge, const char *name,
           unsigned long value, unsigned long oldcode)
{
    int ret;
    char path[SYSFS_PATH_MAX];
    int br_socket_fd;
    FILE *fd;

    if ((br_socket_fd = socket(AF_LOCAL, SOCK_STREAM, 0)) < 0)
    {
        VLOG_ERR("ERROR:: Failed to create socket");
        return 1;
    }

    snprintf(path, SYSFS_PATH_MAX, SYSFS_CLASS_NET "%s/%s", bridge, name);

    fd = fopen(path, "w");
    if (fd) {
        ret = fprintf(fd, "%ld\n", value);
        fclose(fd);
    } else {
        /* fallback to old ioctl */
        struct ifreq ifr;
        unsigned long args[4] = { oldcode, value, 0, 0 };

        strncpy(ifr.ifr_name, bridge, IFNAMSIZ);
        ifr.ifr_data = (char *) &args;
        ret = ioctl(br_socket_fd, SIOCDEVPRIVATE, &ifr);
    }

    return ret;
}

/*------------------------------------------------------------------------------
| Function:  get_port_state_from_string
| Description: get the port state
| Parameters[in]: portstate_str: string  contains port name
| Parameters[out]: port_state:- object conatins port state enum mstp_instance_port_state
| Return: True if valid port state else false.
-----------------------------------------------------------------------------*/
bool
get_port_state_from_string(const char *portstate_str, int *port_state)
{
    bool retval = false;

    if (!portstate_str || !port_state) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return retval;
    }

    VLOG_DBG("%s: entry port state %s", __FUNCTION__, portstate_str);
    if (!strcmp(portstate_str,
                OVSREC_MSTP_COMMON_INSTANCE_PORT_PORT_STATE_BLOCKING)) {
        *port_state = MSTP_INST_PORT_STATE_BLOCKED;
        retval = true;
    } else if (!strcmp(portstate_str,
                       OVSREC_MSTP_INSTANCE_PORT_PORT_STATE_DISABLED)) {
        *port_state = MSTP_INST_PORT_STATE_DISABLED;
        retval = true;
    } else if (!strcmp(portstate_str,
                       OVSREC_MSTP_INSTANCE_PORT_PORT_STATE_LEARNING)) {
        *port_state = MSTP_INST_PORT_STATE_LEARNING;
        retval = true;
    } else if (!strcmp(portstate_str,
                       OVSREC_MSTP_INSTANCE_PORT_PORT_STATE_FORWARDING)) {
        *port_state = MSTP_INST_PORT_STATE_FORWARDING;
        retval = true;
    } else {
        *port_state = MSTP_INST_PORT_STATE_INVALID;
        retval = false;
    }

    VLOG_DBG("%s: exit port state val %d retval %d",
             __FUNCTION__, *port_state, retval);
    return retval;
}

/*------------------------------------------------------------------------------
| Function:  mstp_inform_stp_global_port_state
| Description:  validates to inform stp port state globally
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]: mstp_instance object
| Parameters[in]: mstp_instance_port object
| Parameters[out]: None
| Return:  True if it's single instance,
|                  multi instance: port blocked in all mstp instances
-----------------------------------------------------------------------------*/
bool
mstp_inform_stp_global_port_state(const struct stp_blk_params *br,
                                        struct mstp_instance *msti,
                                        struct mstp_instance_port *mstp_port)
{
    int msti_count = 0;
    int port_state;
    bool block_all_msti = false;
    const char *data = NULL;
    const struct ovsrec_port *port_cfg = NULL;

    if (!br || !msti || !mstp_port) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return false;
    }

    msti_count = br->cfg->n_mstp_instances;
    port_state = mstp_port->stp_state;

    /* check for single instance, CIST only */
    if (msti_count == 0) {
        if ((MSTP_INST_PORT_STATE_BLOCKED == port_state) ||
            (MSTP_INST_PORT_STATE_FORWARDING == port_state)) {
            return true;
        }
        else {
            return false;
        }
    }

    /* get the port row config */
    if (msti->instance_id == MSTP_CIST) {
        port_cfg = mstp_port->cfg.cist_port_cfg->port;
    }
    else {
        port_cfg = mstp_port->cfg.msti_port_cfg->port;
    }

    /* get block_all_mstp key value from port row hw_config column */
    data = smap_get(&port_cfg->hw_config, "block_all_mstp");
    if (data && (MSTP_STR_EQ(data, "true"))) {
        block_all_msti = true;
    }
    else {
        if(data && (MSTP_STR_EQ(data, "false"))) {
           block_all_msti = false;
        }
    }

    if(block_all_msti && (MSTP_INST_PORT_STATE_BLOCKED == port_state)) {
        return true;
    }

    if ((!block_all_msti) && (MSTP_INST_PORT_STATE_FORWARDING == port_state)) {
        return true;
    }

    return false;
}

 /*-----------------------------------------------------------------------------
| Function:  mstp_cist_add_del_port
| Description: add/delete a port to/from cist bridge
| Parameters[in]:
| Parameters[out]:
| Return: true - if operation is successful
|         false - otherwise
-----------------------------------------------------------------------------*/
bool mstp_cist_add_del_port(char *port, bool add)
{
    int ret = -1;
    char *master = (add) ? "master" : "nomaster";

    /* command syntax "ip link set <port> [no]master <brname>  */
    sprintf(cmd, "ip link set %s %s %s", port, master, CIST_BR_NAME);
    RUNCMD(cmd);

    if (add)
        VLOG_INFO("Added port %s to bridge %s", port, CIST_BR_NAME);
    else
        VLOG_INFO("Delete port %s from bridge %s", port, CIST_BR_NAME);

    return true;
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_set_port
| Description: change state of a port
| Parameters[in]:
| Parameters[out]:
| Return: true - if operation is successful
|         false - otherwise
-----------------------------------------------------------------------------*/
bool mstp_cist_set_port(char *port, int state)
{
    int ret = -1;

    sprintf(cmd, "bridge link set dev %s state %d", port, state);
    RUNCMD(cmd);

    VLOG_INFO("%s: state changed to %d", port, state);

    return true;
}


/*------------------------------------------------------------------------------
| Function:  mstp_cist_and_instance_set_port_state
| Description:  set port state in cist/msti
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]: mstp_instance object
| Parameters[in]: mstp_instance_port object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
mstp_cist_and_instance_set_port_state(const struct stp_blk_params *br,
                                            struct mstp_instance *msti,
                                          struct mstp_instance_port *mstp_port)
{
    struct asic_plugin_interface *p_asic_interface = NULL;
    bool inform_stp_state = false;

    if (!msti || !br || !mstp_port) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return;
    }

    inform_stp_state = mstp_inform_stp_global_port_state(br, msti, mstp_port);
    VLOG_DBG("%s: stg %d port name %s state %d inform_state %s", __FUNCTION__,
             msti->hw_stg_id, mstp_port->name, mstp_port->stp_state,
             ((inform_stp_state)?"true":"false"));

    mstp_cist_set_port(mstp_port->name, mstp_port->stp_state);
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_and_instance_port_delete
| Description: delete port from cist/msti
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]: mstp_instance object
| Parameters[in]: mstp_instance_port object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
mstp_cist_and_instance_port_delete(const struct stp_blk_params *br,
                                         struct mstp_instance *msti,
                                         struct mstp_instance_port *port)
{


    if (!msti || !br || !port) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return;
    }
    VLOG_DBG("%s: entry inst %d port name %s", __FUNCTION__, msti->instance_id,
             port->name);

    if (port) {
        hmap_remove(&msti->ports, &port->hmap_node);
        mstp_cist_add_del_port(port->name, false);
        free(port->name);
        free(port);
        msti->nb_ports--;
    }

}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_and_instance_port_lookup
| Description: find port in cist/msti
| Parameters[in]:mstp_instance object
| Parameters[in]: port name
| Parameters[out]: None
| Return:
-----------------------------------------------------------------------------*/
static struct mstp_instance_port *
mstp_cist_and_instance_port_lookup(const struct mstp_instance *msti,
                                          const char *name)
{
    struct mstp_instance_port *port;

    if (!msti || !name) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return NULL;
    }
    VLOG_DBG("%s: inst %d port name %s", __FUNCTION__,
             msti->instance_id, name);

    HMAP_FOR_EACH_WITH_HASH (port, hmap_node, hash_string(name, 0),
                             &msti->ports) {
        if (!strcmp(port->name, name)) {
            return port;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_and_instance_lookup
| Description: find instance in mstp_instances data
| Parameters[in]: inst_id:- instance
| Parameters[out]: None
| Return: mstp_instance object
-----------------------------------------------------------------------------*/
static struct mstp_instance *
mstp_cist_and_instance_lookup(int inst_id)
{
    struct mstp_instance *msti;
    char inst_id_string[INSTANCE_STRING_LEN] = "";

    if (false == MSTP_CIST_INST_VALID(inst_id)) {
        VLOG_DBG("%s: invalid instance id %d", __FUNCTION__, inst_id);
        return NULL;
    }
    if (MSTP_CIST == inst_id) {
        snprintf(inst_id_string, sizeof(inst_id_string), "cist");
    }
    else {
        snprintf(inst_id_string, sizeof(inst_id_string), "mist%d", inst_id);
    }
    HMAP_FOR_EACH_WITH_HASH (msti, node, hash_string(inst_id_string, 0),
                             &all_mstp_instances) {
        if (inst_id == msti->instance_id) {
            return msti;
        }
    }
    return NULL;
}

/*-----------------------------------------------------------------------------
| Function:   mstp_cist_port_add
| Description: add port to cist
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]: mstp_instance object
| Parameters[in]: ovsrec_mstp_common_instance_port object
| Parameters[out]: None
| Return:
-----------------------------------------------------------------------------*/
void
mstp_cist_port_add(const struct stp_blk_params *br, struct mstp_instance *msti,
                 const struct ovsrec_mstp_common_instance_port *cist_port_cfg )
{
    struct mstp_instance_port *new_port = NULL;
    bool retval = false;
    int port_state;

    if (!msti || !br || !cist_port_cfg) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return;
    }

    VLOG_DBG("%s: entry inst %d", __FUNCTION__, msti->instance_id);

        /* Allocate structure to save state information for this port. */
        new_port = xzalloc(sizeof(struct mstp_instance_port));
        if (!new_port) {
           VLOG_ERR("%s: Failed to allocate memory for port %s in instance %d",
                    __FUNCTION__, cist_port_cfg->port->name, msti->instance_id);
           return;
        }

        hmap_insert(&msti->ports, &new_port->hmap_node,
                    hash_string(cist_port_cfg->port->name, 0));

        new_port->name = xstrdup(cist_port_cfg->port->name);

        retval = get_port_state_from_string(cist_port_cfg->port_state,
                                            &port_state);
        if (false == retval) {
            VLOG_DBG("%s:invalid CIST port %s state %s", __FUNCTION__,
                     new_port->name, cist_port_cfg->port_state);
            new_port->stp_state = MSTP_INST_PORT_STATE_INVALID;;
            new_port->cfg.cist_port_cfg = cist_port_cfg;
            return;
        }

        new_port->stp_state = port_state;
        new_port->cfg.cist_port_cfg = cist_port_cfg;
        msti->nb_ports++;
        if (1 == msti->nb_ports) {
            /* Enable STP on the bridge. NOOP if already enabled */
            if (0 != br_set(CIST_BR_NAME, "stp_state", 1, BRCTL_SET_BRIDGE_STP_STATE))
            {
                VLOG_ERR("Failed to enable STP on the bridge");
                return;
            }
            VLOG_INFO("Enabled STP on the bridge :: %s", CIST_BR_NAME);
        }

        mstp_cist_add_del_port(new_port->name, true);
        mstp_cist_and_instance_set_port_state(br, msti, new_port);
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_configure_ports
| Description: add/del / updateports in cist
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]: mstp_instance object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
mstp_cist_configure_ports(const struct stp_blk_params *br,
                             struct mstp_instance *msti)
{
    size_t i;
    struct mstp_instance_port *inst_port, *next;
    struct shash sh_idl_ports;
    struct shash_node *sh_node;
    int new_port_state;
    bool retval;

    if (!msti || !br) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return;
    }

    VLOG_DBG("%s: entry inst %d", __FUNCTION__, msti->instance_id);

    /* Collect all Instance Ports present in the DB. */
    shash_init(&sh_idl_ports);
    for (i = 0; i < msti->cfg.cist_cfg->n_mstp_common_instance_ports; i++) {
        const struct ovsrec_port *pcfg = NULL;
        const char *name = NULL;

        pcfg = msti->cfg.cist_cfg->mstp_common_instance_ports[i]->port;
        if (!pcfg) {
            continue;
        } else {
            name = pcfg->name;
        }
        if (!shash_add_once(&sh_idl_ports, name,
                          msti->cfg.cist_cfg->mstp_common_instance_ports[i])) {
            VLOG_WARN("instance id %d: %s specified twice as CIST Port",
                      msti->instance_id, name);
        }
    }

    /* Delete old Instance Ports. */
    HMAP_FOR_EACH_SAFE (inst_port, next, hmap_node, &msti->ports) {
        const struct ovsrec_mstp_common_instance_port *port_cfg;

        port_cfg = shash_find_data(&sh_idl_ports, inst_port->name);
        if (!port_cfg) {
            VLOG_DBG("Found a deleted Port %s in CIST", inst_port->name);
            mstp_cist_and_instance_port_delete(br, msti, inst_port);
        } else {
            inst_port->cfg.cist_port_cfg = port_cfg;
        }
    }

    /* Add new Instance ports. */
    SHASH_FOR_EACH (sh_node, &sh_idl_ports) {
        inst_port = mstp_cist_and_instance_port_lookup(msti, sh_node->name);
        if (!inst_port) {
            VLOG_DBG("Found an added Port %s in CIST", sh_node->name);
            mstp_cist_port_add(br, msti, sh_node->data);
        }
    }

    inst_port = NULL;
    /* Check for changes in the port row entries. */
    HMAP_FOR_EACH (inst_port, hmap_node, &msti->ports) {
        const struct ovsrec_mstp_common_instance_port *inst_port_row =
                                                 inst_port->cfg.cist_port_cfg;

        /* Check for port state changes. */
        retval =  get_port_state_from_string(inst_port_row->port_state,
                                             &new_port_state);
        if (false == retval) {
            VLOG_DBG("%s:- invalid port state", __FUNCTION__);
            return;
        }

        if(new_port_state != inst_port->stp_state) {
            VLOG_DBG("%s:Set CIST port state to %s", __FUNCTION__,
                     inst_port_row->port_state);
            inst_port->stp_state = new_port_state;
            mstp_cist_and_instance_set_port_state(br,
                                                  msti, inst_port);
        }
        else {
            VLOG_DBG("%s: No change in CIST port %s state" , __FUNCTION__,
                     inst_port->name);
         }
    }

    /* Destroy the shash of the IDL ports */
    shash_destroy(&sh_idl_ports);

}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_add_del_bridge
| Description: create/delete kernel bridge for cist
| Parameters[in]: bool - true/false to indicate add/delete
| Parameters[out]: None
| Return: true - if operation is successful
|         false - otherwise
-----------------------------------------------------------------------------*/
bool mstp_cist_add_del_bridge(bool add)
{
    int ret = -1;
    char *op = (add) ? "add" : "del";

    /* command syntax "ip link {add | del} <brname> type bridge  */
    sprintf(cmd, "ip link %s %s type %s", op, CIST_BR_NAME, BR_LINK_TYPE);

    RUNCMD(cmd);

    /* Set the bridge state to UP and enable STP on it */
    if (add)
    {
        /* Set the bridge status up */
        sprintf(cmd, "ip link set dev %s up", CIST_BR_NAME);
        RUNCMD(cmd);
    }

    VLOG_INFO("Successfully create the bridge :: %s", CIST_BR_NAME);

    return true;
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_create
| Description: create cist instance
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[in]:  ovsrec_mstp_common_instance object
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
mstp_cist_create(const struct stp_blk_params *br,
                    const struct ovsrec_mstp_common_instance *msti_cist_cfg)
{
    struct mstp_instance *msti;
    char inst_id_string[INSTANCE_STRING_LEN] = "";

    if (!msti_cist_cfg || !br) {
        VLOG_DBG("%s: invalid param", __FUNCTION__);
        return;
    }
    VLOG_DBG("%s: entry", __FUNCTION__);

    msti = xzalloc(sizeof *msti);
    if (!msti) {
        VLOG_ERR("%s: Failed to allocate memory for CIST", __FUNCTION__);
        return;
    }

    msti->instance_id = MSTP_CIST;
    msti->cfg.cist_cfg= msti_cist_cfg;
    hmap_init(&msti->vlans);
    hmap_init(&msti->ports);
    snprintf(inst_id_string, sizeof(inst_id_string), "cist");
    hmap_insert(&all_mstp_instances, &msti->node, hash_string(inst_id_string, 0));

    msti->hw_stg_id = MSTP_DEFAULT_STG_GROUP;
    msti->nb_vlans = 0;
    msti->nb_ports = 0;
    mstp_cist_add_del_bridge(true);
    mstp_cist_configure_ports(br, msti);
}

/*-----------------------------------------------------------------------------
| Function:  mstp_cist_update
| Description: check vlans, ports add/deleted -updated in cist
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
mstp_cist_update(const struct stp_blk_params *br)
{
    struct mstp_instance *msti;
    const struct ovsrec_mstp_common_instance *msti_cist_cfg;

    if (!br) {
        VLOG_DBG("%s: invalid bridge param", __FUNCTION__);
        return;
    }

    if (!br->cfg) {
        VLOG_DBG("%s: invalid bridge config param", __FUNCTION__);
        return;
    }
    VLOG_DBG("%s: entry", __FUNCTION__);

    msti_cist_cfg = br->cfg->mstp_common_instance;
    if (!msti_cist_cfg) {
        VLOG_DBG("%s: invalid mstp common instance config  param",
                 __FUNCTION__);
        return;
    }

    msti = mstp_cist_and_instance_lookup(MSTP_CIST);
    if (!msti) {
        VLOG_DBG("%s:Creating CIST", __FUNCTION__);
        mstp_cist_create(br, msti_cist_cfg);
        return;
    }
    else {
        msti->cfg.cist_cfg = msti_cist_cfg;
        /* check if any l2 ports added or deleted  or updated*/
        mstp_cist_configure_ports(br, msti);
    }

}

/*-----------------------------------------------------------------------------
| Function:  stp_reconfigure
| Description: checks for vlans,ports added/deleted/updated in msti/cist
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[out]: None
| Return: True:- if any stp row/column modified
-----------------------------------------------------------------------------*/
bool
stp_plugin_need_propagate_change(struct blk_params* br_blk_param)
{
    struct ovsdb_idl *idl;
    unsigned int idl_seqno;
    const struct ovsrec_mstp_instance *mstp_row = NULL;
    const struct ovsrec_vlan *vlan_row = NULL;
    const struct ovsrec_mstp_instance_port *mstp_port_row = NULL;
    const struct ovsrec_mstp_common_instance_port *cist_port = NULL;
    const struct ovsrec_mstp_common_instance *cist_row = NULL;
    bool cist_row_created = false, cist_row_updated = false,
         mist_row_created = false, mist_row_updated = false,
         mist_row_deleted = false, cist_port_row_updated = false,
         mist_port_row_updated = false, br_mstp_inst_updated = false,
         propagate_change = false, vlan_updated = false;

    if(!br_blk_param || !br_blk_param->idl) {
        VLOG_DBG("%s: invalid blk param object", __FUNCTION__);
        return false;
    }
    VLOG_DBG("%s: entry", __FUNCTION__);

    /* Get idl and idl_seqno to work with */
    idl = br_blk_param->idl;
    idl_seqno = br_blk_param->idl_seqno;

    cist_row = ovsrec_mstp_common_instance_first(idl);
    if (cist_row) {
        cist_row_created = OVSREC_IDL_ANY_TABLE_ROWS_INSERTED(cist_row,
                                                              idl_seqno);
        cist_row_updated = OVSREC_IDL_ANY_TABLE_ROWS_MODIFIED(cist_row,
                                                              idl_seqno);
    } else {
        cist_row_created = false;
        cist_row_updated = false;
    }

    cist_port = ovsrec_mstp_common_instance_port_first(idl);
    if (cist_port) {
        cist_port_row_updated = OVSREC_IDL_ANY_TABLE_ROWS_MODIFIED(cist_port,
                                                                   idl_seqno);
    } else {
        cist_port_row_updated = false;
    }

    if (cist_row_created || cist_row_updated || cist_port_row_updated) {
        VLOG_DBG("%s:cc %d cu %d cpu %d", __FUNCTION__,
                  cist_row_created, cist_row_updated, cist_port_row_updated);
        propagate_change = true;
    } else {
        propagate_change = false;
    }

    return propagate_change;
}

/*-----------------------------------------------------------------------------
| Function:  stp_reconfigure
| Description: checks for vlans,ports added/deleted in msti/cist
| Parameters[in]: blk params :-object contains idl, ofproro, bridge cfg
| Parameters[out]: None
| Return: None
-----------------------------------------------------------------------------*/
void
stp_reconfigure(struct blk_params* br_blk_param)
{
    struct stp_blk_params blk_param;

    if(!br_blk_param || !br_blk_param->idl) {
        VLOG_DBG("%s: invalid blk param object", __FUNCTION__);
        return;
    }
    VLOG_DBG("%s: entry", __FUNCTION__);

    if (!stp_plugin_need_propagate_change(br_blk_param)) {
        VLOG_DBG("%s: propagate_change false", __FUNCTION__);
        return;
    }

    blk_param.idl = br_blk_param->idl;
    blk_param.cfg = ovsrec_bridge_first(br_blk_param->idl);
    mstp_cist_update(&blk_param);
}
