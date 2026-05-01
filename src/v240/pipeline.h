/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v240 σ-Pipeline — dynamic cognitive pipeline engine
 *   with branching and fusion.
 *
 *   No fixed reasoning order.  Every request picks a
 *   pipeline shape by task type, watches σ per stage,
 *   and is allowed to **branch** mid-pipeline when σ
 *   rises above τ_branch, or **fuse** two pipelines
 *   when the task demands both.
 *
 *   Pipeline shapes (v0):
 *     FACTUAL  : recall  → verify → emit
 *     CREATIVE : generate → debate → refine → emit
 *     CODE     : plan    → generate → sandbox → test → emit
 *     MORAL    : analyze → multi_framework → geodesic → emit
 *
 *   Each stage carries its own σ ∈ [0, 1] and a short
 *   canonical name; the v0 fixture ships σ values
 *   tuned so that no clean shape crosses τ_branch
 *   (0.50).  A fifth request is seeded to *start*
 *   factual, see σ climb above τ_branch at the verify
 *   stage, and branch into EXPLORATORY.
 *
 *   EXPLORATORY (branch target): retrieve_wide →
 *   synthesise → re_verify → emit.  The branch event
 *   is recorded as (from_shape, from_stage_id,
 *   to_shape, sigma_at_branch).
 *
 *   Fusion:
 *     A sixth request fuses CODE + MORAL into FUSED:
 *       moral_analyse → code_plan → sandbox →
 *       moral_verify → emit.
 *     σ_fusion = max_stage_sigma (the bottleneck of
 *     the fused pipeline).
 *
 *   σ_pipeline (terminal confidence per request):
 *       σ_pipeline = max over stages of σ_stage
 *     Low σ_pipeline ⇒ the whole chain is certain;
 *     high ⇒ a weak stage dominates.
 *
 *   Contracts (v0):
 *     1. n_requests == 6 (four clean shapes + one
 *        branch + one fusion).
 *     2. Every stage has σ ∈ [0, 1].
 *     3. Clean shapes emit the declared stage sequence
 *        verbatim; no branch event, no fusion event.
 *     4. σ_pipeline per request == max stage σ.
 *     5. Clean shapes keep σ_pipeline ≤ τ_branch
 *        (0.50).
 *     6. Exactly one branch request; the branch fires
 *        on a stage with σ > τ_branch AND the branched
 *        pipeline emits at least one post-branch stage
 *        before emit.
 *     7. Exactly one fusion request; its final shape
 *        contains at least one stage from CODE and at
 *        least one stage from MORAL.
 *     8. FNV-1a chain over every request + branch /
 *        fusion record replays byte-identically.
 *
 *   v240.1 (named, not implemented): live `/pipeline`
 *     web UI, server-sent stage events, plug-in
 *     registry of per-task shapes, branch learning
 *     that updates the σ→shape policy from outcomes.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V240_PIPELINE_H
#define COS_V240_PIPELINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V240_N_REQUESTS    6
#define COS_V240_MAX_STAGES   10

typedef enum {
    COS_V240_SHAPE_FACTUAL     = 1,
    COS_V240_SHAPE_CREATIVE    = 2,
    COS_V240_SHAPE_CODE        = 3,
    COS_V240_SHAPE_MORAL       = 4,
    COS_V240_SHAPE_EXPLORATORY = 5,
    COS_V240_SHAPE_FUSED       = 6
} cos_v240_shape_t;

typedef struct {
    int    stage_id;
    char   name[24];
    float  sigma;
    int    tick;
} cos_v240_stage_t;

typedef struct {
    int              request_id;
    char             label[24];
    cos_v240_shape_t declared_shape;
    cos_v240_shape_t final_shape;   /* after branch/fusion */

    cos_v240_stage_t stages[COS_V240_MAX_STAGES];
    int              n_stages;

    bool             branched;
    int              branch_from_stage;    /* stage id at which branch fired */
    cos_v240_shape_t branch_from_shape;
    cos_v240_shape_t branch_to_shape;
    float            sigma_at_branch;

    bool             fused;
    cos_v240_shape_t fusion_a;
    cos_v240_shape_t fusion_b;

    float            sigma_pipeline;  /* max σ across stages */
} cos_v240_request_t;

typedef struct {
    cos_v240_request_t requests[COS_V240_N_REQUESTS];
    float              tau_branch;     /* 0.50 */
    int                n_branched;
    int                n_fused;

    bool               chain_valid;
    uint64_t           terminal_hash;
    uint64_t           seed;
} cos_v240_state_t;

void   cos_v240_init(cos_v240_state_t *s, uint64_t seed);
void   cos_v240_run (cos_v240_state_t *s);

size_t cos_v240_to_json(const cos_v240_state_t *s,
                         char *buf, size_t cap);

int    cos_v240_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V240_PIPELINE_H */
