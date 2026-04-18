/*
 * v154 σ-Showcase — end-to-end demo pipeline orchestrator.
 *
 * v154.0 is deterministic, weights-free, framework-free, offline.
 * It composes the *shape* of three headline cross-kernel pipelines
 * (research-assistant, self-improving-coder, collaborative-research)
 * with per-stage σ, a geomean σ_product, and a σ_abstain gate.
 *
 * v154.1 is where the stage σ values come from live kernel calls:
 *   research stages -> v118 vision, v152 corpus, v135 symbolic,
 *                      v111.2 reason, v150 swarm, v140 causal,
 *                      v153 identity, v115 memory;
 *   coder    stages -> v151 code-agent, v113 sandbox, v147 reflect,
 *                      v119 speculative, v124 continual, v145 skill;
 *   collab   stages -> v128 mesh, v129 federated, v150 swarm,
 *                      v130 codec.
 *
 * No network. No weights. No filesystem writes except the optional
 * JSON dump via `cos_v154_report_to_json`. Pure C11 + libm.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V154_SHOWCASE_H
#define COS_V154_SHOWCASE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V154_MAX_STAGES       12
#define COS_V154_MAX_NAME         48
#define COS_V154_MAX_KERNEL       16
#define COS_V154_MAX_MSG          96
#define COS_V154_TAU_ABSTAIN_DEF  0.60f

typedef enum {
    COS_V154_SCENARIO_RESEARCH = 0,
    COS_V154_SCENARIO_CODER    = 1,
    COS_V154_SCENARIO_COLLAB   = 2,
} cos_v154_scenario_t;

typedef struct {
    int   index;
    char  name[COS_V154_MAX_NAME];
    char  kernel[COS_V154_MAX_KERNEL];
    float sigma_in;
    float sigma_out;
    float delta_sigma;
    bool  abstained;
    char  note[COS_V154_MAX_MSG];
} cos_v154_stage_t;

typedef struct {
    cos_v154_scenario_t scenario;
    uint64_t            seed;
    int                 n_stages;
    cos_v154_stage_t    stages[COS_V154_MAX_STAGES];
    float               sigma_product;
    float               tau_abstain;
    bool                abstained;
    int                 abstain_stage_index;
    float               mean_sigma;
    float               max_sigma;
    float               min_sigma;
    char                title[COS_V154_MAX_NAME];
    char                user_query[COS_V154_MAX_MSG];
    char                final_message[COS_V154_MAX_MSG];
} cos_v154_report_t;

/* Run one scenario end-to-end. Deterministic in (scenario, seed,
 * tau_abstain). Returns true on success, false on invalid input. */
bool cos_v154_run(cos_v154_scenario_t scenario,
                  uint64_t            seed,
                  float               tau_abstain,
                  cos_v154_report_t  *out);

/* Serialize a report to a preallocated buffer. Returns the number of
 * bytes written (excluding the terminating NUL), or 0 on failure. */
size_t cos_v154_report_to_json(const cos_v154_report_t *r,
                               char                    *buf,
                               size_t                   cap);

/* Human-readable scenario label ("research", "coder", "collab"). */
const char *cos_v154_scenario_name(cos_v154_scenario_t s);

/* Exhaustive self-test. Returns 0 on pass, a nonzero contract id
 * (S1..S6) on failure. */
int cos_v154_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V154_SHOWCASE_H */
