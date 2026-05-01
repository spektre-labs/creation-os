/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v243 σ-Complete — cognitive-completeness test for
 *   Creation OS.
 *
 *   Question v243 answers: *is the Creation OS stack
 *   cognitively complete?*  The answer is not a vibe;
 *   it is a typed checklist over exactly fifteen
 *   canonical categories, each with:
 *     - a covering kernel set (referenced by id)
 *     - an evidence tier (`M` measured / `P` partial)
 *     - a per-category σ_category ∈ [0, 1]
 *
 *   Categories (v0, exactly 15, ordered):
 *      1. PERCEPTION      — v118 vision + v127 voice
 *      2. MEMORY          — v115 memory + v172 narrative
 *      3. REASONING       — v111 reason + v190 latent
 *      4. PLANNING        — v121 planning + v194 horizon
 *      5. ACTION          — v112 function + v113 sandbox
 *      6. LEARNING        — v124 continual + v125 DPO
 *      7. REFLECTION      — v147 reflect + v223 metacognition
 *      8. IDENTITY        — v153 identity + v234 presence
 *      9. MORALITY        — v198 moral + v191 constitutional
 *     10. SOCIALITY       — v150 swarm + v201 diplomacy
 *     11. CREATIVITY      — v219 create
 *     12. SCIENCE         — v204 hypothesis + v206 theorem
 *     13. SAFETY          — v209 containment + v213 trust chain
 *     14. CONTINUITY      — v229 seed + v231 immortal
 *     15. SOVEREIGNTY     — v238 sovereignty
 *
 *   Evidence tier semantics (aligned with the repo's
 *   M-tier / P-tier convention):
 *     M (measured) — there is a per-kernel self-test +
 *                    merge-gate contract that makes the
 *                    category falsifiable here.
 *     P (partial)  — the category is declared but the
 *                    v0 tree only ships a fixture, not
 *                    a measured contract at benchmark
 *                    scale (host-level is still needed).
 *
 *   σ_completeness (system-wide):
 *       σ_completeness = 1 − (covered / total)
 *     `covered` = categories with ≥ 1 kernel AND
 *                  tier ∈ { M, P }
 *     `total`   = 15
 *
 *   The 1=1 test (declared == realized):
 *     For every category, the declared coverage in
 *     FIX[] must exactly equal the realized coverage
 *     after cos_v243_run().  Any mismatch fails the
 *     gate with `one_equals_one == false`.
 *
 *   Contracts (v0):
 *     1. n_categories == 15 and names appear in the
 *        canonical order above.
 *     2. Every category has ≥ 1 kernel id.
 *     3. Every category has tier ∈ { M, P }.
 *     4. Every category has σ_category ∈ [0, 1].
 *     5. `covered` == 15 (all categories covered at
 *        least at P-tier).
 *     6. σ_completeness ∈ [0, 1] AND σ_completeness == 0.0.
 *     7. `one_equals_one == true` (declared coverage
 *        byte-for-byte equal to realized coverage).
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v243.1 (named, not implemented): promote every
 *     P-tier category to M-tier by wiring host-level
 *     benchmarks (ARC, MMLU, HumanEval, etc.) through
 *     the harness and updating `WHAT_IS_REAL.md`
 *     accordingly.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V243_COMPLETE_H
#define COS_V243_COMPLETE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V243_N_CATEGORIES  15
#define COS_V243_MAX_KERNELS    4

typedef enum {
    COS_V243_TIER_M = 1,   /* measured at benchmark scale */
    COS_V243_TIER_P = 2    /* partial / structural only   */
} cos_v243_tier_t;

typedef struct {
    int              idx;
    char             name[24];
    int              kernels[COS_V243_MAX_KERNELS];
    int              n_kernels;
    cos_v243_tier_t  tier;
    float            sigma_category;
    bool             covered;
    bool             declared_eq_realized;
} cos_v243_category_t;

typedef struct {
    cos_v243_category_t cats[COS_V243_N_CATEGORIES];
    int                 n_covered;
    int                 n_m_tier;
    int                 n_p_tier;
    float               sigma_completeness;
    bool                one_equals_one;
    bool                cognitive_complete;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v243_state_t;

void   cos_v243_init(cos_v243_state_t *s, uint64_t seed);
void   cos_v243_run (cos_v243_state_t *s);

size_t cos_v243_to_json(const cos_v243_state_t *s,
                         char *buf, size_t cap);

int    cos_v243_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V243_COMPLETE_H */
