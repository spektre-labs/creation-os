/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * σ-verified service marketplace — local catalog + HTTP peer RPC.
 */

#ifndef COS_MARKETPLACE_H
#define COS_MARKETPLACE_H

#include "error_attribution.h"
#include "proof_receipt.h"

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum cos_error_source cos_error_source_t;

struct cos_service {
    char    service_id[64];
    char    name[128];
    char    description[256];
    char    provider_id[64];
    float   sigma_mean;
    float   reliability;
    int     times_served;
    float   cost_eur;
    int64_t registered;
    char    endpoint[256]; /* optional http(s) base for this provider */
};

struct cos_service_request {
    char    service_id[64];
    char    prompt[2048];
    char    requester_id[64];
    float   max_sigma;
    float   max_cost_eur;
    int64_t timeout_ms;
};

struct cos_service_response {
    char                     response[4096];
    float                    sigma;
    cos_error_source_t       attribution;
    struct cos_proof_receipt receipt;
    float                    cost_eur;
    int64_t                  latency_ms;
};

struct cos_think_result;

int cos_marketplace_init(void);
void cos_marketplace_shutdown(void);

int cos_marketplace_register_service(const struct cos_service *service);

/** Local catalog + merged remote listings (COS_MARKETPLACE_PEERS comma URLs). */
int cos_marketplace_browse(struct cos_service *services, int max, int *n);

int cos_marketplace_request(const struct cos_service_request *req,
                            struct cos_service_response *resp);

/** Serve using local cos think + proof receipt for the matching service_id. */
int cos_marketplace_serve(const struct cos_service_request *req,
                          struct cos_service_response *resp);

int cos_marketplace_verify_receipt(const struct cos_service_response *resp);

/** Ω-loop: invoke marketplace before cloud if env-configured peers exist. */
int cos_marketplace_omega_rescue(const char *goal,
                                 struct cos_think_result *tr);

/** Stats JSON-ish line to stdout helpers for CLI */
int cos_marketplace_stats_fprint(FILE *fp);

#if defined(CREATION_OS_ENABLE_SELF_TESTS) || defined(COS_MARKETPLACE_TEST)
int cos_marketplace_self_test(void);
#endif

#ifdef __cplusplus
}
#endif
#endif /* COS_MARKETPLACE_H */
