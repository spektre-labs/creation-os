/*
 * v229 σ-Seed — minimal seed architecture with a
 *   deterministic growth protocol.
 *
 *   Creation OS is 228 kernels.  v229 compresses the
 *   whole tree down to a five-kernel seed:
 *
 *       v029  σ-aggregate     (measurement)
 *       v101  bridge          (inference)
 *       v106  server          (API)
 *       v124  continual       (learning)
 *       v148  sovereign       (RSI loop)
 *
 *   Everything else re-grows from that seed by a
 *   σ-gated protocol:
 *       1. seed measures itself              (v029)
 *       2. detects a missing capability      (v133 meta)
 *       3. generates a candidate kernel      (v146 genesis)
 *       4. benchmarks it                     (v143 bench)
 *       5. integrates if σ_growth < τ_growth (v162 compose)
 *   repeat until the growth queue is exhausted.
 *
 *   Growth σ: for every candidate we compute
 *       σ_growth = 0.60·σ_raw + 0.40·σ_depth_penalty
 *   where σ_raw is a per-candidate risk drawn from the
 *   fixture (how novel / how far from the seed) and
 *   σ_depth_penalty rises with the chain length from
 *   the seed.  Accept iff σ_growth ≤ τ_growth (0.40).
 *
 *   Deterministic regrowth (the 1 = 1 claim):
 *       seed + fixture + τ_growth ⇒ same grown system
 *   The self-test runs the full growth protocol twice
 *   and asserts `terminal_hash_a == terminal_hash_b`.
 *   This is the offline stand-in for the README's
 *   "SHA256 over the grown system" — v229 uses the
 *   stack-canonical FNV-1a chain.
 *
 *   Contracts (v0):
 *     1. Seed set: exactly 5 kernels, ids match the
 *        declared quintet above.
 *     2. Grown set: seed + accepted ≥ 15 kernels.
 *     3. Every accepted kernel has σ_growth ≤ τ_growth.
 *     4. Every accepted kernel has parent already in
 *        the system at the time of integration.
 *     5. At least one candidate is REJECTED (the
 *        σ-gate has teeth).
 *     6. terminal_hash_a == terminal_hash_b on repeat
 *        runs (deterministic regrowth).
 *     7. FNV-1a chain over seed + every growth step
 *        replays byte-identically.
 *
 *   v229.1 (named): live v146 genesis hookup, SHA-256
 *     over a real filesystem tree, `cos seed --verify`
 *     CLI with end-to-end rebuild, continuous v229 →
 *     v148 RSI loop that can shrink the grown set.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V229_SEED_H
#define COS_V229_SEED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V229_N_SEED         5
#define COS_V229_N_CANDIDATES  13
#define COS_V229_N_MAX         (COS_V229_N_SEED + COS_V229_N_CANDIDATES)

typedef struct {
    int   version;          /* e.g. 29, 101, 106, 124, 148 */
    char  name[24];         /* short name */
    int   parent_version;   /* -1 for seed */
    float sigma_raw;
    float sigma_depth;
    float sigma_growth;
    int   depth;            /* 0 for seed */
    bool  accepted;
    bool  is_seed;
} cos_v229_kernel_t;

typedef struct {
    cos_v229_kernel_t  pool[COS_V229_N_MAX];
    int                n_accepted;       /* includes seed */
    int                n_rejected;

    float              tau_growth;       /* 0.40 */

    uint64_t           terminal_hash;
    uint64_t           terminal_hash_verify;  /* second run */
    bool               chain_valid;
    uint64_t           seed;
} cos_v229_state_t;

void   cos_v229_init(cos_v229_state_t *s, uint64_t seed);
void   cos_v229_run (cos_v229_state_t *s);

size_t cos_v229_to_json(const cos_v229_state_t *s,
                         char *buf, size_t cap);

int    cos_v229_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V229_SEED_H */
