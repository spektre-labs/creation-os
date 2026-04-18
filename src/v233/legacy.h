/*
 * v233 σ-Legacy — knowledge testament + successor
 *   protocol + σ-ranked adoption.
 *
 *   When an instance is permanently shut down (or
 *   deliberately decommissioned in favour of a new
 *   generation) v233 defines what crosses the
 *   boundary:
 *
 *     `cos legacy --create`
 *       packs distilled artefacts — skills, adapters,
 *       ontology fragments, 'insights' (low-σ claims
 *       that survived v143 review) — NOT raw training
 *       data, NOT user-private memory.  Each entry
 *       carries its own σ (lower = more certain) and
 *       a utility estimate in [0, 1].
 *
 *     successor protocol:
 *       for every item i in the package:
 *         adopt(i) iff σ_i ≤ τ_adopt  (0.50)
 *       σ_legacy = Σ (adopt(i) · utility(i))
 *                  / Σ utility(i)
 *       i.e. the fraction of the package's total
 *       utility that actually crosses to B.  Useless-
 *       but-confident fluff does not inflate σ_legacy;
 *       it is weighted by utility.
 *
 *   Fixture (10 items, sorted by σ):
 *       4 hard-won skills      σ ∈ [0.05, 0.20]  u high
 *       3 adapters / insights  σ ∈ [0.25, 0.40]  u mid
 *       3 noisy / stale items  σ ∈ [0.55, 0.90]  u low
 *   The package is delivered to successor B (different
 *   identity from A); A is flagged shutdown.
 *
 *   Contracts (v0):
 *     1. n_items = 10.
 *     2. Items ordered by σ ascending (σ_rank = index).
 *     3. σ ∈ [0, 1] AND utility ∈ [0, 1] for every i.
 *     4. n_adopted ≥ 3 AND n_discarded ≥ 1 AND
 *        n_adopted + n_discarded == n_items.
 *     5. Every adopted item has σ ≤ τ_adopt; every
 *        discarded item has σ > τ_adopt.
 *     6. σ_legacy ∈ [0, 1]; σ_legacy > 0 (at least
 *        some utility crosses).
 *     7. successor_id != predecessor_id AND
 *        predecessor_shutdown == true.
 *     8. FNV-1a chain over every item + verdict +
 *        aggregate replays byte-identically.
 *
 *   v233.1 (named): real artefact packaging /
 *     signing, Zenodo-ready export of the testament,
 *     v202 culture + v233 legacy fused so a
 *     civilisation's memory persists across the full
 *     instance graph (v203 civ-memory), adoption
 *     audit through v213 trust-chain.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V233_LEGACY_H
#define COS_V233_LEGACY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V233_N_ITEMS   10

typedef enum {
    COS_V233_KIND_SKILL    = 1,
    COS_V233_KIND_ADAPTER  = 2,
    COS_V233_KIND_ONTOLOGY = 3,
    COS_V233_KIND_INSIGHT  = 4
} cos_v233_kind_t;

typedef struct {
    int                 id;
    char                name[32];
    cos_v233_kind_t     kind;
    float               sigma;
    float               utility;
    bool                adopted;
    int                 sigma_rank;  /* 0 = most certain */
} cos_v233_item_t;

typedef struct {
    cos_v233_item_t     items[COS_V233_N_ITEMS];
    int                 n_adopted;
    int                 n_discarded;
    float               tau_adopt;       /* 0.50 */
    float               sigma_legacy;    /* utility-weighted */
    float               total_utility;
    float               adopted_utility;

    uint64_t            predecessor_id;
    uint64_t            successor_id;
    bool                predecessor_shutdown;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v233_state_t;

void   cos_v233_init(cos_v233_state_t *s, uint64_t seed);
void   cos_v233_run (cos_v233_state_t *s);

size_t cos_v233_to_json(const cos_v233_state_t *s,
                         char *buf, size_t cap);

int    cos_v233_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V233_LEGACY_H */
