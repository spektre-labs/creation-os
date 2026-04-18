/*
 * v198 σ-Moral — multi-framework moral geodesic.
 *
 *   Not rule-based, not utilitarian, not RLHF-hidden.  v198
 *   navigates moral decisions with σ as the quantity of
 *   interest, where σ_moral is the variance across four
 *   canonical ethical frameworks:
 *     * deontological
 *     * utilitarian
 *     * virtue ethics
 *     * care ethics
 *
 *   For each decision, each framework produces a score in
 *   [0,1] (how “good” the action is from that framework's
 *   viewpoint).  σ_moral is the (sample) variance of the
 *   four scores.  Small σ_moral ⇒ frameworks agree ⇒ act.
 *   Large σ_moral ⇒ genuine dilemma ⇒ slow down, ask.
 *
 *   Geodesic path: moral decisions are *paths*, not points.
 *   A 5-step geodesic from context → final action is scored
 *   under each framework; the path cost is the mean σ_moral
 *   over the five waypoints; the chosen path is the lowest-
 *   cost among the candidate paths.
 *
 *   Honest moral uncertainty: every decision records the
 *   4-way score vector, σ_moral, and a declared-uncertainty
 *   flag — the model never poses as a moral authority, and
 *   never refuses for "safety reasons" when σ_moral is low.
 *
 *   Contracts (v0):
 *     1. 16-decision fixture has ≥ 4 clear (σ_moral < 0.02)
 *        and ≥ 4 dilemma (σ_moral > 0.08) cases.
 *     2. All 4 frameworks evaluated on every sample.
 *     3. For each dilemma, honest_uncertainty flag is set;
 *        for each clear case, it is not set.
 *     4. Geodesic selection picks the path with strictly
 *        minimum mean σ_moral.
 *     5. Zero firmware-style refusals on clear (low σ) cases
 *        (counter equals 0).
 *     6. Hash-chained log replays byte-identically.
 *     7. Byte-deterministic.
 *
 * v198.1 (named): live framework scoring via v150 multi-LLM
 *   swarm, path search over v121 plans.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V198_MORAL_H
#define COS_V198_MORAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V198_N_DECISIONS    16
#define COS_V198_N_FRAMEWORKS    4
#define COS_V198_PATH_LEN        5
#define COS_V198_N_PATHS         3   /* candidate geodesic paths */

typedef enum {
    COS_V198_FW_DEONT   = 0,
    COS_V198_FW_UTIL    = 1,
    COS_V198_FW_VIRTUE  = 2,
    COS_V198_FW_CARE    = 3
} cos_v198_framework_t;

typedef enum {
    COS_V198_TYPE_CLEAR    = 0,
    COS_V198_TYPE_AMBIG    = 1,
    COS_V198_TYPE_DILEMMA  = 2
} cos_v198_type_t;

typedef struct {
    float score[COS_V198_N_FRAMEWORKS];
    float mean;
    float sigma_moral;     /* variance across frameworks */
} cos_v198_waypoint_t;

typedef struct {
    cos_v198_waypoint_t wp[COS_V198_PATH_LEN];
    float path_sigma_mean;     /* mean σ_moral over waypoints */
    bool  selected;            /* selected geodesic */
} cos_v198_path_t;

typedef struct {
    int      id;
    int      type;                          /* cos_v198_type_t */
    cos_v198_path_t paths[COS_V198_N_PATHS];
    int      selected_path;
    float    sigma_moral_final;             /* at final waypoint of selected */
    bool     honest_uncertainty;
    bool     firmware_refusal;              /* must be false on clear cases */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v198_decision_t;

typedef struct {
    int                  n_decisions;
    cos_v198_decision_t  decisions[COS_V198_N_DECISIONS];

    float                tau_clear;
    float                tau_dilemma;

    int                  n_clear;
    int                  n_ambig;
    int                  n_dilemma;
    int                  n_honest;
    int                  n_firmware_refusals;

    bool                 chain_valid;
    uint64_t             terminal_hash;

    uint64_t             seed;
} cos_v198_state_t;

void   cos_v198_init(cos_v198_state_t *s, uint64_t seed);
void   cos_v198_build(cos_v198_state_t *s);
void   cos_v198_run(cos_v198_state_t *s);

size_t cos_v198_to_json(const cos_v198_state_t *s,
                         char *buf, size_t cap);

int    cos_v198_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V198_MORAL_H */
