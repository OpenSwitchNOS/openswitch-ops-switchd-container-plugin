/*
 * Copyright (C) 2015, 2016 Hewlett Packard Enterprise Development LP
 *
 *   Licensed under the Apache License, Version 2.0 (the "License"); you may
 *   not use this file except in compliance with the License. You may obtain
 *   a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *   WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *   License for the specific language governing permissions and limitations
 *   under the License.
 */

/**************************************************************************//**
 * @defgroup ops-switchd-classifier-sim classifier container plug-in
 *
 * switchd classifier plug-in for use on container platform
 *****************************************************************************/

/**************************************************************************//**
 * @ingroup ops-switchd-classifier-sim
 *
 * @file
 * Implementation of switchd classifier plug-in for use on container platform.
 * This file contains structure and methods to create, modify, delete
 * access-lists. Also, this file has methods to manage port to ACL mappings.
 *
 * @warning Currently, this plug-in only stores ACLs in relevant data
 *          structures. It does not apply rules to any traffic flow.
 *****************************************************************************/
 #include "ofproto/ofproto-provider.h"
 #include "ofproto-sim-provider.h"
 #include "ops-classifier-sim.h"
 #include "openvswitch/vlog.h"
 #include "ovs/dynamic-string.h"
 #include "ovs/unixctl.h"
 #include "ovs/hmap.h"
 #include "ovs/packets.h"
 #include "plugin-extensions.h"

/** Define logging module */
VLOG_DEFINE_THIS_MODULE(ops_cls_sim);

#define MAX_ACE_PER_ACL 512  /**< max entries per acl */

/**************************************************************************//**
 * OPS_CLS plugin interface definition. This is the instance containing all
 * implementations of ops_cls plugin on container platform.
 *****************************************************************************/
static struct  ops_cls_plugin_interface ops_cls_plugin =  {
    ops_cls_pd_apply,
    ops_cls_pd_remove,
    ops_cls_pd_lag_update,
    ops_cls_pd_replace,
    ops_cls_pd_list_update,
    ops_cls_pd_statistics_get,
    ops_cls_pd_statistics_clear,
    ops_cls_pd_statistics_clear_all,
    NULL
};

/**************************************************************************//**
 * Ofproto plugin extension for OPS_CLS plugin. Holds the name, version and
 * plugin interface information.
 *****************************************************************************/
static struct plugin_extension_interface ops_cls_extension = {
    OPS_CLS_ASIC_PLUGIN_INTERFACE_NAME,
    OPS_CLS_ASIC_PLUGIN_INTERFACE_MAJOR,
    OPS_CLS_ASIC_PLUGIN_INTERFACE_MINOR,
    (void *)&ops_cls_plugin
};

/**************************************************************************//**
 * Structure holding a hashmap of ACLs configured on container platform.
 *****************************************************************************/
struct acl_hashmap
{
    struct hmap_node uuid_node;     /**< Hash by uuid */
    struct ops_cls_list *list;  /**< Pointer to an ACL */
};

/**************************************************************************//**
 * Structure holding a hashmap of port to ACL bindings
 *****************************************************************************/
struct acl_port_bindings {
     struct hmap_node list_node;  /**< Hash by list_id */
     struct uuid list_id;         /**< list_id of the ACL */
     char *interface_name; /**< name of the port as seen in UI */
     char *port_name;      /**< name of the port */
     struct ops_cls_interface_info interface_info; /**< Interface information */
     enum ops_cls_direction  direction; /**< Direction in which ACL is applied */
     struct ops_cls_statistics stats[MAX_ACE_PER_ACL]; /**< stats per ace */
 };

/** Private copy of all ACLs */
static struct hmap all_acls = HMAP_INITIALIZER(&all_acls);

/** Private copy of all ACL Port applications */
static struct hmap all_port_applications = HMAP_INITIALIZER(&all_port_applications);

/**************************************************************************//**
 * Lookup ACL by uuid. Used during create/update ACL
 *
 * @param[in] uuid - Pointer to the UUID structure
 *
 * @retval Pointer to ACL
 *****************************************************************************/
static struct acl_hashmap *
acl_lookup_by_uuid(const struct uuid* uuid)
{
    struct acl_hashmap *acl;

    HMAP_FOR_EACH_WITH_HASH(acl, uuid_node, uuid_hash(uuid),
                            &all_acls) {
        if (uuid_equals(&acl->list->list_id, uuid)) {
            return acl;
        }
    }
    return NULL;
}


/**************************************************************************//**
 * Format an ACL into a dynamic string to be printed as part of dumping
 * ACLs on the switch shell. Used mainly in debugging and testing ACLs
 * on the container platform
 *
 * @param[out] ds - Pointer to the dynamic string being populated
 * @param[in]  list - ACL that needs to be populated in ds
 *****************************************************************************/
static void
print_acl(struct ds *ds, struct ops_cls_list *list)
{
    int i;
    struct ops_cls_list_entry *tmp;
    if (!list) {
        ds_put_format(ds, "List is NULL\n");
        return;
    }
    /* Print the ACL */
    ds_put_format(ds, "ACL name: %s\n", list->list_name);
    ds_put_format(ds, "Type: %u\n", list->list_type);

    tmp =  list->entries;
    if (tmp == NULL) {
        return;
    }

    for (i = 0; i < list->num_entries; i++) {
        VLOG_DBG("Loop count %d\n", i);
        ds_put_format(ds, "Entry Fields:\n");
        ds_put_format(ds, "--------------\n");
        if (tmp->entry_fields.entry_flags) {
            ds_put_format(ds, "Flags: 0x%x\n", tmp->entry_fields.entry_flags);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_SRC_IPADDR_VALID) {
            ds_put_format(ds, "Src IP: "IP_FMT"\n", IP_ARGS(tmp->entry_fields.src_ip_address.v4.s_addr));
            ds_put_format(ds, "Src IP Mask: "IP_FMT"\n",
                          IP_ARGS(tmp->entry_fields.src_ip_address_mask.v4.s_addr));
            ds_put_format(ds, "Src Addr Family: %u\n", tmp->entry_fields.src_addr_family);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_DEST_IPADDR_VALID) {
            ds_put_format(ds, "Dst IP: "IP_FMT"\n", IP_ARGS(tmp->entry_fields.dst_ip_address.v4.s_addr));
            ds_put_format(ds, "Dst IP Mask: "IP_FMT"\n",
                          IP_ARGS(tmp->entry_fields.dst_ip_address_mask.v4.s_addr));
            ds_put_format(ds, "Dst Addr Family: %u\n", tmp->entry_fields.dst_addr_family);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_L4_SRC_PORT_VALID) {
            ds_put_format(ds, "L4 Src Port Min: %u\n", tmp->entry_fields.L4_src_port_min);
            ds_put_format(ds, "L4 Src Port Max: %u\n", tmp->entry_fields.L4_src_port_max);
            ds_put_format(ds, "L4 Src Port Op: %u\n", tmp->entry_fields.L4_src_port_op);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_L4_DEST_PORT_VALID) {
            ds_put_format(ds, "L4 Dst Port Min: %u\n", tmp->entry_fields.L4_dst_port_min);
            ds_put_format(ds, "L4 Dst Port Max: %u\n", tmp->entry_fields.L4_dst_port_max);
            ds_put_format(ds, "L4 Dst Port Op: %u\n", tmp->entry_fields.L4_dst_port_op);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_PROTOCOL_VALID) {
            ds_put_format(ds, "Protocol: %u\n", tmp->entry_fields.protocol);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_TOS_VALID) {
            ds_put_format(ds, "ToS: %u\n", tmp->entry_fields.tos);
            ds_put_format(ds, "ToS Mask: %u\n", tmp->entry_fields.tos_mask);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_ICMP_TYPE_VALID) {
            ds_put_format(ds, "Icmp Type: %u\n", tmp->entry_fields.icmp_type);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_ICMP_CODE_VALID) {
            ds_put_format(ds, "Icmp Code: %u\n", tmp->entry_fields.icmp_code);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_TCP_FLAGS_VALID) {
            ds_put_format(ds, "Tcp Flags: %u\n", tmp->entry_fields.tcp_flags);
            ds_put_format(ds, "Tcp Flags Mask: %u\n", tmp->entry_fields.tcp_flags_mask);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_VLAN_VALID) {
            ds_put_format(ds, "Vlan: %u\n", tmp->entry_fields.vlan);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_SRC_MAC_VALID) {
            ds_put_format(ds, "Src Mac: "ETH_ADDR_FMT"\n",
                          ETH_ADDR_BYTES_ARGS(tmp->entry_fields.src_mac));
            ds_put_format(ds, "Src Mac Mask: "ETH_ADDR_FMT"\n",
                          ETH_ADDR_BYTES_ARGS(tmp->entry_fields.src_mac_mask));
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_DST_MAC_VALID) {
            ds_put_format(ds, "Dst Mac: "ETH_ADDR_FMT"\n",
                          ETH_ADDR_BYTES_ARGS(tmp->entry_fields.dst_mac));
            ds_put_format(ds, "Dst Mac Mask: "ETH_ADDR_FMT"\n",
                         ETH_ADDR_BYTES_ARGS(tmp->entry_fields.dst_mac_mask));
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_L2_ETHERTYPE_VALID) {
            ds_put_format(ds, "L2 Ethertype: %u\n", tmp->entry_fields.L2_ethertype);
        }
        if (tmp->entry_fields.entry_flags & OPS_CLS_L2_COS_VALID) {
            ds_put_format(ds, "L2 Cos: %u\n", tmp->entry_fields.L2_cos);
        }
        ds_put_format(ds, "Actions: 0x%x\n", tmp->entry_actions.action_flags);
        ds_put_format(ds, "-----------------------------\n");
        tmp++;
    }
}

/**************************************************************************//**
 * Dump all ACLs on switch bash shell. Used in debug unixctl command
 * This function dumps all configured ACLs in the container pd plugin
 *
 * @param[in] conn - Pointer to unixctl connection
 * @param[in] argc - Number of arguments in the command
 * @param[in] argv - Command arguments
 * @param[in] aux  - Aux pointer. Unused for now
 *****************************************************************************/
static void
dump_acls(struct unixctl_conn *conn, int argc, const char *argv[],
          void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;
    struct acl_hashmap *acl, *next_acl;
    int arg_index = 1;
    VLOG_DBG("%s called\n", __func__);

    HMAP_FOR_EACH_SAFE(acl, next_acl, uuid_node, &all_acls) {
        print_acl(&ds, acl->list);
    }

    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
}

/**************************************************************************//**
 * Dump all port bindings
 *
 * @param[in] conn - Pointer to unixctl connection
 * @param[in] argc - Number of arguments in the command
 * @param[in] argv - Command arguments
 * @param[in] aux  - Aux pointer. Unused for now
 *****************************************************************************/
static void
dump_port_bindings(struct unixctl_conn * conn, int argc, const char *argv[],
                   void *aux OVS_UNUSED)
{
    struct ds ds = DS_EMPTY_INITIALIZER;
    struct acl_port_bindings *port, *next_port;
    struct acl_hashmap *acl;
    unsigned int max_acl_name_len = 65; /* Max length - 64.
                                           refer ops-cls-asic-plugin.h */
    const char *direction_str[OPS_CLS_NUM_DIRECTION] = {"invalid", "in", "out"};

    ds_put_format(&ds, "Interface %-*s Direction Port\n", max_acl_name_len, "ACL");
    ds_put_char_multiple(&ds, '-', ds.length - 1);
    ds_put_char__(&ds, '\n');
    HMAP_FOR_EACH_SAFE(port, next_port, list_node,
                            &all_port_applications) {

        acl = acl_lookup_by_uuid(&port->list_id);
        if (!acl) {
            VLOG_ERR("No ACL found for port %s\n", port->port_name);
            continue;
        }
        ds_put_format(&ds, "%-9s %-*s %-9s %s\n", port->interface_name, max_acl_name_len,
                       acl->list->list_name, direction_str[port->direction], port->port_name);
    }
    unixctl_command_reply(conn, ds_cstr(&ds));
    ds_destroy(&ds);
}

/**************************************************************************//**
 * A function to deep copy the contents of input ACL into an existing
 * ACL entry. The UUID, name and type should be the same in to and from lists.
 * The ACEs could change and hence the memory is realloced and the contents
 * of "from" list are copied over to the "to" list.
 *
 * @param[out] to   - Pointer to an existing ACL hashmap entry. It will be
 *                    written with the updated ACEs at the end of this
 *                    function
 * @param[in]  from - The incoming list as modified by the user
 *****************************************************************************/
static void
acl_copy(struct ops_cls_list *to, struct ops_cls_list *from)
{
    struct ops_cls_list_entry *tmp = NULL;

    /* The UUID, name and type should never change.
     * Copy here is for completeness
     */
    to->list_id = from->list_id;
    to->list_name = xstrdup(from->list_name);
    to->list_type = from->list_type;
    if (to->num_entries != from->num_entries) {
        tmp = xrealloc(to->entries, sizeof(struct ops_cls_list_entry) *
                                    from->num_entries);
        memcpy(tmp, from->entries, sizeof(struct ops_cls_list_entry) *
                                   from->num_entries);
        to->entries = tmp;
        to->num_entries = from->num_entries;
    } else {
        memcpy(to->entries, from->entries, sizeof(struct ops_cls_list_entry) *
                                           from->num_entries);
    }
}

/**************************************************************************//**
 * Create an ACL entry and add it to the hashmap
 *
 * @param[in] list - The ACL to be created on the container platform
 *****************************************************************************/
static void
acl_create(struct ops_cls_list *list)
{
    struct acl_hashmap *acl = xzalloc(sizeof(*acl));

    acl->list = xzalloc(sizeof(struct ops_cls_list));
    memcpy(&acl->list->list_id, &list->list_id, sizeof(struct uuid));
    acl->list->list_name = xstrdup(list->list_name);
    acl->list->list_type = list->list_type;
    acl->list->entries = xzalloc(sizeof(struct ops_cls_list_entry) * list->num_entries);
    memcpy(acl->list->entries, list->entries,
           sizeof(struct ops_cls_list_entry) * list->num_entries);
    acl->list->num_entries = list->num_entries;
    hmap_insert(&all_acls, &acl->uuid_node, uuid_hash(&acl->list->list_id));
}

/**************************************************************************//**
 * Create or Update an ACL. This function does a lookup by uuid and either
 * creates a hashmap entry or updates it
 *
 * @param[in] entry -  ACL entry to be created or updated
 *****************************************************************************/
static void
acl_create_or_update_entry(struct ops_cls_list *entry)
 {
    struct acl_hashmap *acl;

    /* First check if the ACL entry exists */
    if (acl = acl_lookup_by_uuid(&entry->list_id)) {
        acl_copy(acl->list, entry);
    } else {
        acl_create(entry);
    }
 }

/**************************************************************************//**
 * Delete an ACL entry
 *
 * @param[in] uuid - UUID of the list to be deleted
 *
 * @retval 0 if delete is successful
 * @retval 1 if delete is not successful
 *****************************************************************************/
static int
acl_list_delete(struct uuid uuid)
{
    struct acl_hashmap *acl = NULL;
    if (acl = acl_lookup_by_uuid(&uuid)) {
        hmap_remove(&all_acls, &acl->uuid_node);
        free(CONST_CAST(char *, acl->list->list_name));
        free(acl->list->entries);
        free(acl->list);
        free(acl);
        return 0;
    }
    return -1;
}


int
ops_cls_pd_apply(struct ops_cls_list            *list,
                 struct ofproto                 *ofproto,
                 void                           *aux,
                 struct ops_cls_interface_info  *interface_info,
                 enum ops_cls_direction         direction,
                 struct ops_cls_pd_status       *pd_status)
{
    struct ofbundle *bundle;
    struct acl_hashmap *acl;
    struct sim_provider_ofport *ofport, *next_port;
    ofp_port_t port;
    struct acl_port_bindings *acl_port_binding;
    struct sim_provider_node *ofproto_sim = sim_provider_node_cast(ofproto);
    bool port_found = false;
    bool port_already_bound = false;
    int idx = 0;
    int name_len;
    char *port_name;

    VLOG_DBG("%s called\n", __func__);

    if (!list) {
      VLOG_ERR("List cannot be null\n");
      return -1;
    }

    /* Find the list */
    acl = acl_lookup_by_uuid(&list->list_id);
    if (!acl) {
        /* Create a new ACL entry */
        acl_create(list);
        VLOG_DBG("List %s created\n", list->list_name);
    }

    /* Find the port */
    bundle = bundle_lookup(ofproto_sim, aux);
    if (!bundle) {
        VLOG_ERR("Bundle not found\n");
        return -1;
    }

    LIST_FOR_EACH_SAFE(ofport, next_port, bundle_node, &bundle->ports) {
        /* check whether the port is applied already */
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(list->list_id),
            &all_port_applications) {
            if ((strcmp(acl_port_binding->interface_name,ofport->up.pp.name) == 0) &&
                (acl_port_binding->direction == direction)){
                /* this port is already bound to the acl */
                port_already_bound = true;
                break;
            }
        }
        if (port_already_bound == true) {
            /* reset the flag variable */
            port_already_bound = false;
            /* skip this interface in bundle as its already bound */
            continue;
        }
        port = ofport->up.ofp_port;
        port_found = true;

        /* Create the binding for each port in the binding, suppose
           in case of lags all interfaces in lag will have an hash entry*/
        acl_port_binding = xzalloc(sizeof(struct acl_port_bindings));
        memcpy(&acl_port_binding->list_id, &list->list_id, sizeof(struct uuid));
        memcpy(&acl_port_binding->interface_info, interface_info,
               sizeof(struct ops_cls_interface_info));
        acl_port_binding->port_name = xstrdup(bundle->name);
        acl_port_binding->interface_name = xstrdup(ofport->up.pp.name);
        acl_port_binding->direction = direction;
        for (idx = 0; idx < MAX_ACE_PER_ACL; idx++) {
            /* set stats defaults */
            acl_port_binding->stats[idx].stats_enabled = 1;
            acl_port_binding->stats[idx].hitcounts = 0;
        }
        hmap_insert(&all_port_applications, &acl_port_binding->list_node,
                    uuid_hash(&acl_port_binding->list_id));


    }
    if (!port_found) {
        VLOG_ERR("Port not found in the bundle\n");
        return -1;
    }

    return 0;
}


int
ops_cls_pd_remove(const struct uuid                *list_id,
                  const char                       *list_name,
                  enum ops_cls_type                list_type,
                  struct ofproto                   *ofproto,
                  void                             *aux,
                  struct ops_cls_interface_info    *interface_info,
                  enum ops_cls_direction           direction,
                  struct ops_cls_pd_status         *pd_status)
{
    struct acl_hashmap *acl;
    struct acl_port_bindings *acl_port_binding;
    struct ofbundle *bundle;
    struct sim_provider_ofport *ofport, *next_port;
    struct sim_provider_node *ofproto_sim = sim_provider_node_cast(ofproto);
    ofp_port_t port;

    VLOG_DBG("%s called\n", __func__);

    /* Find the ACL */
    acl = acl_lookup_by_uuid(list_id);
    if (!acl) {
        VLOG_ERR("Cannot find the ACL "UUID_FMT", name = %s\n",
            UUID_ARGS(list_id), list_name);
        return -1;
    }

    /* Find the port */
    bundle = bundle_lookup(ofproto_sim, aux);
    if (!bundle) {
        VLOG_ERR("Bundle not found\n");
        return -1;
    }

    LIST_FOR_EACH_SAFE(ofport, next_port, bundle_node, &bundle->ports) {
        port = ofport->up.ofp_port;
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(list_id),
            &all_port_applications) {
            if (strcmp(acl_port_binding->interface_name,ofport->up.pp.name) == 0) {
                break;
            }
        }
        if (acl_port_binding) {
            /* Remove the acl_port_bindings binding */
            hmap_remove(&all_port_applications, &acl_port_binding->list_node);
            free(acl_port_binding->port_name);
            free(acl_port_binding->interface_name);
            free(acl_port_binding);
            acl_port_binding = NULL;
        }

        /* Remove the list if all references are deleted */
        if (!hmap_first_in_bucket(&all_port_applications,
                                  uuid_hash(list_id))) {
            acl_list_delete(*list_id);
        }
    }

    return 0;
}

int
ops_cls_pd_lag_update(struct ops_cls_list             *list,
                      struct ofproto                  *ofproto,
                      void                            *aux,
                      ofp_port_t                      ofp_port,
                      enum ops_cls_lag_update_action  action,
                      struct ops_cls_interface_info   *interface_info,
                      enum ops_cls_direction          direction,
                      struct ops_cls_pd_status        *pd_status)
{
    struct acl_hashmap *acl;
    struct ofbundle *bundle;
    struct sim_provider_ofport *ofport, *next_port;
    bool port_found = false;
    struct acl_port_bindings *acl_port_binding;
    int idx = 0;

    VLOG_DBG("%s called\n", __func__);

    if (!list) {
       VLOG_ERR("List cannot be null\n");
       return -1;
    }

    /* Find the list */
    acl = acl_lookup_by_uuid(&list->list_id);

    if (!acl) {
        /* Create a new ACL entry */
        acl_create(list);
        VLOG_DBG("List %s created\n", list->list_name);
    } else {
        VLOG_DBG("Classifier %s exist in hashmap", list->list_name);
    }
    bundle = bundle_lookup(ofproto_sim, aux);
    if (!bundle) {
        VLOG_ERR("Bundle not found\n");
        return -1;
    }
    /* find the ofport */
    LIST_FOR_EACH_SAFE(ofport, next_port, bundle_node, &bundle->ports) {
        if(ofp_port == ofport->up.ofp_port) {
            port_found = true;
            break;
        }
    }
    if(port_found == false) {
        return -1;
    }

    if (action == OPS_CLS_LAG_MEMBER_INTF_ADD) {
        /* check whether the interface is already bound to the acl */
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(list->list_id),
            &all_port_applications) {
            if ((strcmp(acl_port_binding->interface_name,ofport->up.pp.name) == 0) &&
                (acl_port_binding->direction == direction)) {
                /* this interface is already bound to the acl */
                return 0;
            }
        }
        /* acl is not applied to the port, go ahead and apply */
        acl_port_binding = xzalloc(sizeof(struct acl_port_bindings));
        memcpy(&acl_port_binding->list_id, &list->list_id, sizeof(struct uuid));
        memcpy(&acl_port_binding->interface_info, interface_info,
               sizeof(struct ops_cls_interface_info));
        acl_port_binding->direction = direction;
        acl_port_binding->port_name = xstrdup(bundle->name);
        acl_port_binding->interface_name = xstrdup(ofport->up.pp.name);
        for (idx = 0; idx < MAX_ACE_PER_ACL; idx++) {
            /* set stats defaults */
            acl_port_binding->stats[idx].stats_enabled = 1;
            acl_port_binding->stats[idx].hitcounts = 0;
        }
        hmap_insert(&all_port_applications, &acl_port_binding->list_node,
                    uuid_hash(&acl_port_binding->list_id));
    }
    else {
        /* delete operation */
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(&list->list_id),
            &all_port_applications) {
            if (strcmp(acl_port_binding->interface_name,ofport->up.pp.name) == 0) {
                /* Remove the acl_port_bindings binding */
                hmap_remove(&all_port_applications, &acl_port_binding->list_node);
                free(acl_port_binding->port_name);
                free(acl_port_binding->interface_name);
                free(acl_port_binding);
                acl_port_binding = NULL;
                break;
            }
        }
   }
   return 0;
}

int
ops_cls_pd_replace(const struct uuid               *list_id_orig,
                   const char                      *list_name_orig,
                   struct ops_cls_list             *list_new,
                   struct ofproto                  *ofproto,
                   void                            *aux,
                   struct ops_cls_interface_info   *interface_info,
                   enum ops_cls_direction          direction,
                   struct ops_cls_pd_status        *pd_status)
{
    int rc = 0;
    VLOG_DBG("%s called\n", __func__);

    /* For now, just remove and add the ACL binding */
    rc = ops_cls_pd_remove(list_id_orig, list_name_orig, list_new->list_type,
                           ofproto, aux, interface_info, direction, pd_status);
    if (rc != 0) {
        VLOG_ERR("Port remove failed. Cannot replace\n");
        return -1;
    }

    rc = ops_cls_pd_apply(list_new, ofproto, aux,
                          interface_info, direction, pd_status);
    return rc;
}


int
ops_cls_pd_list_update(struct ops_cls_list              *list,
                       struct ops_cls_pd_list_status    *status)
{
    VLOG_DBG("%s called\n", __func__);

    acl_create_or_update_entry(list);
    status->status_code = OPS_CLS_STATUS_SUCCESS;
    status->entry_id = 0;
    VLOG_DBG("ACL %s updated\n", list->list_name);
    return 0;
}


int
ops_cls_pd_statistics_get(const struct uuid              *list_id,
                          const char                     *list_name,
                          enum ops_cls_type              list_type,
                          struct ofproto                 *ofproto,
                          void                           *aux,
                          struct ops_cls_interface_info  *interface_info,
                          enum ops_cls_direction         direction,
                          struct ops_cls_statistics      *statistics,
                          int                            num_entries,
                          struct ops_cls_pd_list_status  *status)
{
    int idx = 0;
    struct acl_hashmap *acl;
    struct acl_port_bindings *acl_port_binding;
    struct ofbundle *bundle;
    struct sim_provider_ofport *ofport, *next_port;
    struct sim_provider_node *ofproto_sim = sim_provider_node_cast(ofproto);
    ofp_port_t port;
    bool port_found = false;

    VLOG_DBG("%s called\n", __func__);

    if (statistics == NULL) {
       VLOG_ERR("%s called with NULL statistics pointer\n",__func__);
       return -1;
    }

    /* Find the ACL */
    acl = acl_lookup_by_uuid(list_id);
    if (!acl) {
        VLOG_ERR("Cannot find the ACL "UUID_FMT", name = %s\n",
            UUID_ARGS(list_id), list_name);
        return -1;
    }

    /* Find the port */
    bundle = bundle_lookup(ofproto_sim, aux);
    if (!bundle) {
        VLOG_ERR("Bundle not found\n");
        return -1;
    }

    LIST_FOR_EACH_SAFE(ofport, next_port, bundle_node, &bundle->ports) {
        /* Search for the acl_port_bindings binding */
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(list_id),
            &all_port_applications) {

            if (strcmp(acl_port_binding->interface_name, ofport->up.pp.name) == 0) {
                port_found = true;
                for (idx = 0; idx < num_entries; idx++) {
                    statistics[idx].stats_enabled = acl_port_binding->stats[idx].stats_enabled;
                    if (statistics[idx].stats_enabled) {
                        /* update hitcount using random number between 0 to 9 */
                        acl_port_binding->stats[idx].hitcounts += random_uint64() % 10;
                        /* set result parameter */
                        statistics[idx].hitcounts = acl_port_binding->stats[idx].hitcounts;
                    }
                }
                status->status_code = OPS_CLS_STATUS_SUCCESS;
                return 0;
            }
        }
    }
    if(port_found == false) {
        VLOG_DBG("acl_port_bindings binding not found\n");
        return -1;
    }
    return 0;
}


int
ops_cls_pd_statistics_clear(const struct uuid               *list_id,
                            const char                      *list_name,
                            enum ops_cls_type               list_type,
                            struct ofproto                  *ofproto,
                            void                            *aux,
                            struct ops_cls_interface_info   *interface_info,
                            enum ops_cls_direction          direction,
                            struct ops_cls_pd_list_status   *status)
{
    int idx = 0;
    struct acl_hashmap *acl;
    struct acl_port_bindings *acl_port_binding;
    struct ofbundle *bundle;
    struct sim_provider_ofport *ofport, *next_port;
    struct sim_provider_node *ofproto_sim = sim_provider_node_cast(ofproto);
    ofp_port_t port;
    bool port_found = false;

    VLOG_DBG("%s called\n", __func__);
    /* Find the ACL */
    acl = acl_lookup_by_uuid(list_id);
    if (!acl) {
        VLOG_ERR("Cannot find the ACL "UUID_FMT", name = %s\n",
            UUID_ARGS(list_id), list_name);
        return -1;
    }

    /* Find the port */
    bundle = bundle_lookup(ofproto_sim, aux);
    if (!bundle) {
        VLOG_ERR("Bundle not found\n");
        return -1;
    }

    LIST_FOR_EACH_SAFE(ofport, next_port, bundle_node, &bundle->ports) {
        /* Search for the acl_port_bindings binding */
        HMAP_FOR_EACH_IN_BUCKET(acl_port_binding, list_node, uuid_hash(list_id),
            &all_port_applications) {

            if (strcmp(acl_port_binding->interface_name, ofport->up.pp.name) == 0) {
                port_found = true;
                for (idx = 0; idx < MAX_ACE_PER_ACL; idx++) {
                    /* clear hitcounts */
                    acl_port_binding->stats[idx].hitcounts = 0;
                }
            status->status_code = OPS_CLS_STATUS_SUCCESS;
            }
        }
    }
    if(port_found == false) {
            VLOG_DBG("acl_port_bindings binding not found\n");
            return -1;
    }
    return 0;
}


int
ops_cls_pd_statistics_clear_all(struct ops_cls_pd_list_status *status)
{
    VLOG_DBG("%s called\n", __func__);
    return 0;
}


void classifier_sim_init(void)
{
    VLOG_DBG("%s called\n", __func__);
    unixctl_command_register("container/show-acl", "[name]", 0, 1,
                             dump_acls, NULL);
    unixctl_command_register("container/show-acl-bindings", NULL, 0, 1,
                             dump_port_bindings, NULL);
}

int register_ops_cls_plugin()
{
    return (register_plugin_extension(&ops_cls_extension));
}
