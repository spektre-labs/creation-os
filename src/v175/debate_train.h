/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v175 σ-Debate-Train — self-play learning from v150 debates.
 *
 *   v150 already lets N specialists argue; v175 *harvests* the
 *   argument as training data:
 *
 *       3 specialists answer the same prompt.
 *       B attempts to refute A (adversarial verify).
 *       If B wins         → (A=rejected, B=chosen)  DPO pair.
 *       If B fails        → A=chosen                positive.
 *       Both consensus    → skip (uninformative).
 *
 *   Plus a round-robin self-play tournament with Elo ratings
 *   per adapter, and a SPIN-style current-vs-previous loop
 *   whose σ_improvement → 0 signals convergence.
 *
 * v175.0 (this file) ships deterministic, weights-free demos:
 *   - 12 debate rounds over 4 prompts × 3 specialists
 *     (medical / creative / code): every σ per specialist is
 *     hand-picked so two classes (pair + positive + skip) all
 *     appear, and the big-model corrector wins where expected.
 *   - 3-adapter round-robin: 6 matches, Elo update constant
 *     K=32, expected-score from standard Elo formula.
 *   - A SPIN loop over 5 generations with a monotone σ_delta
 *     shrink, proving convergence.
 *
 * v175.1 (named, not shipped):
 *   - real v150 debate protocol, real v125 DPO training,
 *     real LoRA-adapter swap between rounds, real v114 swarm
 *     specialists.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V175_DEBATE_TRAIN_H
#define COS_V175_DEBATE_TRAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V175_N_PROMPTS    4
#define COS_V175_N_SPECIALISTS 3
#define COS_V175_N_ADAPTERS   3
#define COS_V175_MAX_MATCHES  32
#define COS_V175_SPIN_MAX     8
#define COS_V175_MAX_STR     96

typedef enum {
    COS_V175_RES_CHOSEN_POS = 0,   /* A survived critique — chosen (positive) */
    COS_V175_RES_PAIR       = 1,   /* B refuted A — DPO pair                  */
    COS_V175_RES_SKIP       = 2,   /* σ's low and agreeing — uninformative    */
} cos_v175_debate_result_t;

typedef struct {
    int                      prompt_id;
    int                      spec_a;
    int                      spec_b;
    float                    sigma_a;
    float                    sigma_b;
    float                    sigma_delta;
    cos_v175_debate_result_t result;
    char                     note[COS_V175_MAX_STR];
} cos_v175_debate_t;

typedef struct {
    int   adapter_id;
    int   opponent_id;
    float sigma_adapter;
    float sigma_opponent;
    float expected;               /* Elo expected score */
    float actual;                 /* 1 / 0.5 / 0 */
    float rating_before;
    float rating_after;
} cos_v175_match_t;

typedef struct {
    int   gen;
    float sigma_current;
    float sigma_previous;
    float sigma_delta;            /* |current − previous| */
    bool  converged;
} cos_v175_spin_t;

typedef struct {
    float tau_consensus;          /* σ below AND |σ_a − σ_b| below → SKIP */
    float spin_convergence;       /* stop SPIN when delta < this */
    float elo_k;                  /* default 32.0 */
    float elo_init;               /* default 1500.0 */

    int   n_debates;
    cos_v175_debate_t debates[COS_V175_N_PROMPTS * COS_V175_N_SPECIALISTS];

    int   n_matches;
    cos_v175_match_t  matches[COS_V175_MAX_MATCHES];
    float ratings[COS_V175_N_ADAPTERS];

    int   n_spin;
    cos_v175_spin_t   spin[COS_V175_SPIN_MAX];
    bool  spin_converged;
    int   spin_converged_at;

    uint64_t seed;
} cos_v175_state_t;

void cos_v175_init(cos_v175_state_t *s, uint64_t seed);

void cos_v175_run_debates    (cos_v175_state_t *s);
void cos_v175_run_tournament (cos_v175_state_t *s);
void cos_v175_run_spin       (cos_v175_state_t *s);

int  cos_v175_adapter_champion(const cos_v175_state_t *s);

const char *cos_v175_result_name(cos_v175_debate_result_t r);

size_t cos_v175_to_json(const cos_v175_state_t *s, char *buf, size_t cap);
size_t cos_v175_dpo_ndjson(const cos_v175_state_t *s,
                           char *buf, size_t cap);

int cos_v175_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V175_DEBATE_TRAIN_H */
