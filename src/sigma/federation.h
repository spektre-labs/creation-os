/*
 * σ-federation — trust-weighted sharing of semantic knowledge (no raw episodes).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_FEDERATION_H
#define COS_SIGMA_FEDERATION_H

#include "knowledge_graph.h"
#include "skill_distill.h"

#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    COS_FED_UPDATE_TAU          = 0,
    COS_FED_UPDATE_SKILL        = 1,
    COS_FED_UPDATE_KG_RELATION    = 2,
    COS_FED_UPDATE_FORGET       = 3,
};

struct cos_federation_update {
    uint64_t node_id;
    uint64_t update_hash;
    int      update_type;

    uint64_t domain_hash;
    float    tau_local;
    float    sigma_mean;
    int      sample_count;

    struct cos_skill skill;

    struct cos_kg_relation relation;

    uint64_t forget_hash;
    char     forget_reason[128];

    float    trust_score;
    int64_t  timestamp_ms;
};

struct cos_federation_config {
    int   max_peers;
    float min_trust_to_accept;
    float min_sigma_to_share;
    int   auto_share;
    int   accept_forget;
};

int cos_federation_default_config(struct cos_federation_config *cfg);

int cos_federation_init(const struct cos_federation_config *cfg);

int cos_federation_share_update(const struct cos_federation_update *update);

int cos_federation_receive_update(const struct cos_federation_update *update);

int cos_federation_merge_tau(uint64_t domain_hash, float remote_tau,
                             int remote_samples, float trust);

int cos_federation_broadcast_forget(uint64_t forget_hash,
                                    const char *reason);

int cos_federation_peers(uint64_t *node_ids, float *trust_scores,
                         int max_peers, int *n_found);

/** Wire format — binary blob for A2A payload attachment. */
int cos_federation_pack(const struct cos_federation_update *u,
                        unsigned char *buf, size_t buf_cap,
                        size_t *out_len);

int cos_federation_unpack(const unsigned char *buf, size_t len,
                          struct cos_federation_update *out);

void cos_federation_print_status(FILE *fp);

int cos_federation_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_FEDERATION_H */
