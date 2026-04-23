/*
 * σ-gated web retrieval facade (ULTRA search plane).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_SEARCH_H
#define COS_CLI_SEARCH_H

#include "error_attribution.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_SEARCH_MAX_RESULTS
#define COS_SEARCH_MAX_RESULTS 16
#endif

struct cos_search_result {
    char title[256];
    char url[512];
    char snippet[1024];
    float sigma;
    enum cos_error_source attribution;
};

int cos_search_run(const char *query, int max_results,
                   struct cos_search_result *results, int *n_found);

void cos_search_print_results(const struct cos_search_result *results,
                              int n);

int cos_search_main(int argc, char **argv);

int cos_search_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_CLI_SEARCH_H */
