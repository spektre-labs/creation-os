/*
 * v186 σ-Continual-Architecture — architecture that grows and
 * prunes itself with σ.
 *
 *   v163 evolve-architecture optimises *existing* kernels.
 *   v177 compress deletes kernels.  Neither GROWS new ones.
 *   v186 closes the loop: v133 meta watches σ_mean per
 *   domain; when it trends up, the architecture is judged
 *   under-capacity and v146 genesis proposes a new kernel;
 *   v163 evolve accepts or rejects; v177 compress trims;
 *   every change is logged to v181 audit so the architecture
 *   history is rewindable.
 *
 *       capacity monitor ─► σ_mean trend by domain
 *                         ─► starved domain + severity
 *       genesis proposer ─► candidate kernel (synthetic)
 *       evolve acceptor  ─► accept/reject on Δσ
 *       compress pruner  ─► inactive kernels dropped
 *       audit logger     ─► every decision hashed + chained
 *
 *   The exit invariants (exercised in the merge-gate):
 *
 *     1. The monitor identifies ≥ 1 starved domain where
 *        σ_mean is strictly rising across the fixture.
 *     2. Growth proposes at least one new kernel; evolve
 *        accepts it iff it reduces the starved-domain σ_mean;
 *        at least one acceptance happens in the run.
 *     3. Pruning removes at least one kernel whose σ_mean
 *        and utility gain are both below threshold.
 *     4. Every architecture change is appended to the audit
 *        log with a deterministic hash; replaying the log
 *        in order re-derives the final architecture.
 *     5. Net result: |kernels_final| ≠ |kernels_initial|
 *        (architecture actually changes over the cycle).
 *
 * v186.0 (this file) ships a deterministic, weights-free
 * fixture: 6 initial kernels × 4 domains × 6 epochs.  Growth
 * and pruning use closed-form σ trend analysis.
 *
 * v186.1 (named, not shipped):
 *   - real v146 genesis wiring
 *   - real v163 evolve acceptance loop
 *   - real v181 audit signing
 *   - Web-UI architecture-history viewer
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V186_GROW_H
#define COS_V186_GROW_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V186_MAX_KERNELS     32
#define COS_V186_N_DOMAINS        4
#define COS_V186_N_EPOCHS         6
#define COS_V186_MAX_CHANGES     64
#define COS_V186_STR_MAX         48

typedef enum {
    COS_V186_CHANGE_INIT    = 0,
    COS_V186_CHANGE_GROW    = 1,
    COS_V186_CHANGE_PRUNE   = 2,
    COS_V186_CHANGE_ACCEPT  = 3,
    COS_V186_CHANGE_REJECT  = 4
} cos_v186_change_kind_t;

typedef struct {
    int    id;
    char   name[COS_V186_STR_MAX];
    int    domain;                        /* primary domain */
    int    created_epoch;
    bool   active;
    float  sigma_mean;                    /* most recent */
    float  utility;                       /* ∝ 1 - σ_mean */
} cos_v186_kernel_t;

typedef struct {
    int    epoch;
    int    kind;                          /* cos_v186_change_kind_t */
    int    kernel_id;
    int    domain;
    float  sigma_before;
    float  sigma_after;
    char   reason[COS_V186_STR_MAX];
    uint64_t hash;
    uint64_t prev_hash;
} cos_v186_change_t;

typedef struct {
    int                 n_kernels;
    cos_v186_kernel_t   kernels[COS_V186_MAX_KERNELS];

    int                 n_changes;
    cos_v186_change_t   changes[COS_V186_MAX_CHANGES];

    /* Per-domain rolling σ_mean across epochs. */
    float   sigma_history[COS_V186_N_DOMAINS][COS_V186_N_EPOCHS];
    bool    starved      [COS_V186_N_DOMAINS];

    /* Aggregate metrics. */
    int     n_initial_kernels;
    int     n_final_kernels;
    int     n_grown;
    int     n_pruned;
    int     n_accepted;
    int     n_rejected;

    float   tau_growth;                   /* σ-trend slope ≥ ⇒ grow */
    float   tau_prune_utility;            /* utility ≤ ⇒ prune */
    uint64_t seed;
} cos_v186_state_t;

void   cos_v186_init  (cos_v186_state_t *s, uint64_t seed);
void   cos_v186_run   (cos_v186_state_t *s);

uint64_t cos_v186_replay_chain(const cos_v186_state_t *s);

size_t cos_v186_to_json(const cos_v186_state_t *s,
                         char *buf, size_t cap);

int    cos_v186_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V186_GROW_H */
