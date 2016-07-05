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

#ifndef __OPS_CLASSIFIER_SIM_H
#define __OPS_CLASSIFIER_SIM_H 1

#include "ops-cls-asic-plugin.h"
#include "ovs/unixctl.h"

/************************************************************************//**
 * @defgroup ops-switchd-classifier-api classifier plug-in interface
 *
 * See ops/doc/switchd_classifier_api_design.md for additional information
 *
 * @todo write ops/doc/switchd_classifier_api_design.md
 ***************************************************************************/

/************************************************************************//**
 * @ingroup ops-switchd-classifier-api
 *
 * @file
 * Prototypes for the Classifier List plug-in interface. For now,
 * documentation for these functions can be found in the ops-openvswitch
 * repo:
 * vswitchd/plugins/ops-classifier/include/ofproto-ops-classifier.h"
 ***************************************************************************/

/**************************************************************************//**
 * Initializer for debug functions in container plugin. Registers a
 * unixctl command
 *****************************************************************************/
void classifier_sim_init(void);

/**************************************************************************//**
 * Register OPS_CLS plugin for container platform
 *****************************************************************************/
 int register_ops_cls_plugin(void);

/**************************************************************************//**
 * See @ref ofproto_ops_cls_apply
 *****************************************************************************/
int ops_cls_pd_apply(struct ops_cls_list            *list,
                     struct ofproto                 *ofproto,
                     void                           *aux,
                     struct ops_cls_interface_info  *interface_info,
                     enum ops_cls_direction         direction,
                     struct ops_cls_pd_status       *pd_status);

/***************************************************************************//**
 * See @ref ofproto_ops_cls_remove
 *****************************************************************************/
int ops_cls_pd_remove(const struct uuid                *list_id,
                      const char                       *list_name,
                      enum ops_cls_type                list_type,
                      struct ofproto                   *ofproto,
                      void                             *aux,
                      struct ops_cls_interface_info    *interface_info,
                      enum ops_cls_direction           direction,
                      struct ops_cls_pd_status         *pd_status);

/***************************************************************************//* *
 * See @ref ofproto_ops_cls_lag_update
 *****************************************************************************/
int
ops_cls_pd_lag_update(struct ops_cls_list             *list,
                      struct ofproto                  *ofproto,
                      void                            *aux,
                      ofp_port_t                      ofp_port,
                      enum ops_cls_lag_update_action  action,
                      struct ops_cls_interface_info   *interface_info,
                      enum ops_cls_direction          direction,
                      struct ops_cls_pd_status        *pd_status);

/***************************************************************************//**
 * See @ref ofproto_ops_cls_replace
 *****************************************************************************/
int ops_cls_pd_replace(const struct uuid               *list_id_orig,
                       const char                      *list_name_orig,
                       struct ops_cls_list             *list_new,
                       struct ofproto                  *ofproto,
                       void                            *aux,
                       struct ops_cls_interface_info   *interface_info,
                       enum ops_cls_direction          direction,
                       struct ops_cls_pd_status        *pd_status);

/***************************************************************************//**
 * See @ref ofproto_ops_cls_list_update
 *****************************************************************************/
int ops_cls_pd_list_update(struct ops_cls_list              *list,
                           struct ops_cls_pd_list_status    *status);

/**************************************************************************//**
 * See @ref ofproto_ops_cls_statistics_get
 *****************************************************************************/
int ops_cls_pd_statistics_get(const struct uuid              *list_id,
                              const char                     *list_name,
                              enum ops_cls_type              list_type,
                              struct ofproto                 *ofproto,
                              void                           *aux,
                              struct ops_cls_interface_info  *interface_info,
                              enum ops_cls_direction         direction,
                              struct ops_cls_statistics      *statistics,
                              int                            num_entries,
                              struct ops_cls_pd_list_status  *status);

/**************************************************************************//**
 * See @ref ofproto_ops_cls_statistics_clear
 *****************************************************************************/
int ops_cls_pd_statistics_clear(const struct uuid               *list_id,
                                const char                      *list_name,
                                enum ops_cls_type               list_type,
                                struct ofproto                  *ofproto,
                                void                            *aux,
                                struct ops_cls_interface_info   *interface_info,
                                enum ops_cls_direction          direction,
                                struct ops_cls_pd_list_status   *status);

/**************************************************************************//**
 * See @ref ofproto_ops_cls_statistics_clear_all
 *****************************************************************************/
int ops_cls_pd_statistics_clear_all(struct ops_cls_pd_list_status *status);

#endif  /* __OPS_CLASSIFIER_SIM_H */
