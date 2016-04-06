

#include <stdlib.h>
#include <errno.h>
#include "copp-asic-provider.h" //from ops-switchd/plugins
#include "plugin-extensions.h"  //from ops-switchd/plugins

/*
 * feature plugins
 */



 int copp_stats_get_test(const unsigned int hw_asic_id,
                       const enum copp_protocol_class class,
                       struct copp_protocol_stats *const stats);

 int copp_hw_status_get_test(const unsigned int hw_asic_id,
                           const enum copp_protocol_class class,
                           struct copp_hw_status *const hw_status);


struct copp_asic_plugin_interface copp_interface ={
     /* The new functions that need to be exported, can be declared here*/
     .copp_stats_get = &copp_stats_get_test,
     .copp_hw_status_get = &copp_hw_status_get_test,
 };


#define STATS_RCS_MAX 3
#define STATSUS_RCS_MAX 5

int test_stats_rcs[STATS_RCS_MAX] = {0, EOPNOTSUPP, EINVAL};
int test_statsus_rcs[STATSUS_RCS_MAX] = {0, EOPNOTSUPP, ENOSPC, EIO, EINVAL};

void sim_copp_init(void) {

     struct plugin_extension_interface copp_extension;

     copp_extension.plugin_name = COPP_ASIC_PLUGIN_INTERFACE_NAME;
     copp_extension.major = COPP_ASIC_PLUGIN_INTERFACE_MAJOR;
     copp_extension.minor = COPP_ASIC_PLUGIN_INTERFACE_MINOR;
     copp_extension.plugin_interface = (void *)&copp_interface;

     register_plugin_extension(&copp_extension);
     return;
}

 int copp_stats_get_test(const unsigned int hw_asic_id,
                       const enum copp_protocol_class class,
                       struct copp_protocol_stats *const stats) {

    static int rc = STATS_RCS_MAX;

    stats->packets_passed = rand();
    stats->bytes_passed = rand();
    stats->packets_dropped = rand();
    stats->bytes_dropped = rand();

    rc++; if (rc > STATS_RCS_MAX) rc = 0;
    return test_stats_rcs[rc];

}

 int copp_hw_status_get_test(const unsigned int hw_asic_id,
                           const enum copp_protocol_class class,
                           struct copp_hw_status *const hw_status) {

    static int rc = STATSUS_RCS_MAX;
    hw_status->rate = rand();
    hw_status->burst = rand();
    hw_status->local_priority = rand();

    rc++; if (rc > STATSUS_RCS_MAX) rc = 0;
    return test_stats_rcs[rc];

 }
