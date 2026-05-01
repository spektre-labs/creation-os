/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v236 σ-Autobiography — automatic life-story generation
 *   + milestone tracker + narrative-consistency checker.
 *
 *   Humans carry a narrative identity; v236 gives the
 *   same discipline to a Creation OS instance without
 *   letting it hallucinate a past it did not live.
 *
 *   Fixture (8 milestones, canonical):
 *
 *     kind                         sigma  tick  domain    content
 *     ---------------------------- -----  ----  --------  ------------------------
 *     FIRST_SIGMA_BELOW_0_10       0.08   120   maths     σ=0.09 on maths proof
 *     FIRST_SUCCESSFUL_RSI         0.12   210   sigma     v148 RSI loop converged
 *     FIRST_FORK                   0.05   310   meta      fork-1 spawned
 *     LARGEST_SINGLE_IMPROVEMENT   0.14   420   retrieve  σ dropped 0.42 → 0.11
 *     NEW_SKILL_ACQUIRED           0.18   505   code      wrote first v0 kernel
 *     FIRST_ABSTENTION             0.22   580   biology   σ > τ, refused to emit
 *     LARGEST_ERROR_RECOVERY       0.30   690   sigma     detected + rolled back
 *     FIRST_LEGACY_ADOPTED         0.26   780   identity  v233 testament accepted
 *
 *   Sort order: ascending tick (monotone, strictly
 *   increasing).  Every milestone carries its own σ in
 *   [0, 1] and a v135-style Prolog-shaped consistency
 *   ground truth (`consistent_with_prev == true`
 *   whenever the claim does not contradict an earlier
 *   one).  v236 explicitly does NOT invent milestones.
 *
 *   σ_autobiography (utility-weighted consistency):
 *       w_i             = 1 − σ_i          (confidence weight)
 *       total_weight    = Σ w_i
 *       consistent_mass = Σ (w_i · [consistent_i])
 *       σ_autobiography = consistent_mass / total_weight
 *
 *   Fixture contract: every milestone is consistent (no
 *   narrative contradictions), so σ_autobiography == 1.
 *   v0 also stores an anti-fixture run (one milestone
 *   flipped to inconsistent) in the in-header struct
 *   so the dispersion behaviour is checkable; the
 *   primary run only uses the canonical fixture.
 *
 *   Life-story narrative:
 *     The run() pass composes a short, ASCII-only life
 *     summary (first_tick, last_tick, n_milestones,
 *     strongest_domain, weakest_domain) that `cos
 *     autobiography` can ship.  It is *derived*, never
 *     hand-written.
 *
 *   Contracts (v0):
 *     1. n_milestones == 8.
 *     2. Ticks strictly ascending (no duplicates).
 *     3. σ ∈ [0, 1] for every milestone.
 *     4. Every milestone's `consistent_with_prev` is
 *        true (Prolog ground truth on the fixture).
 *     5. σ_autobiography == 1.0 (within 1e-6).
 *     6. strongest_domain has the lowest mean σ;
 *        weakest_domain has the highest (deterministic
 *        tiebreak on alphabetical domain).
 *     7. Narrative summary is non-empty ASCII.
 *     8. FNV-1a chain over every milestone + aggregate
 *        replays byte-identically.
 *
 *   v236.1 (named, not implemented): live v135 Prolog
 *     consistency check against a real journal, Zenodo-
 *     exportable life story, milestone auto-extraction
 *     from the v115 memory timeline.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V236_AUTOBIOGRAPHY_H
#define COS_V236_AUTOBIOGRAPHY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V236_N_MILESTONES 8

typedef enum {
    COS_V236_KIND_FIRST_SIGMA_BELOW_0_10  = 1,
    COS_V236_KIND_FIRST_SUCCESSFUL_RSI    = 2,
    COS_V236_KIND_FIRST_FORK              = 3,
    COS_V236_KIND_LARGEST_IMPROVEMENT     = 4,
    COS_V236_KIND_NEW_SKILL_ACQUIRED      = 5,
    COS_V236_KIND_FIRST_ABSTENTION        = 6,
    COS_V236_KIND_LARGEST_ERROR_RECOVERY  = 7,
    COS_V236_KIND_FIRST_LEGACY_ADOPTED    = 8
} cos_v236_kind_t;

typedef struct {
    int              id;
    cos_v236_kind_t  kind;
    int              tick;
    float            sigma;
    char             domain[16];
    char             content[64];
    bool             consistent_with_prev;
} cos_v236_milestone_t;

typedef struct {
    cos_v236_milestone_t milestones[COS_V236_N_MILESTONES];
    int      n_milestones;
    float    sigma_autobiography;   /* utility-weighted consistency */
    int      first_tick;
    int      last_tick;
    char     strongest_domain[16];  /* lowest mean σ */
    char     weakest_domain  [16];  /* highest mean σ */
    char     narrative[256];        /* ASCII summary  */

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v236_state_t;

void   cos_v236_init(cos_v236_state_t *s, uint64_t seed);
void   cos_v236_run (cos_v236_state_t *s);

size_t cos_v236_to_json(const cos_v236_state_t *s,
                         char *buf, size_t cap);

int    cos_v236_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V236_AUTOBIOGRAPHY_H */
