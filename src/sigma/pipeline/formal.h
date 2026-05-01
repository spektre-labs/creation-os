/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Formal — runtime proof harness for the σ-gate.
 *
 * The evidence ledger in docs/RESEARCH_AND_THESIS_ARCHITECTURE.md
 * recorded 2/6 theorems discharged by v259.  H4 discharges the
 * remaining four (T3–T6) at the C layer.  These are "runtime
 * proofs": the harness generates many witnesses, asserts the
 * property on each, and emits a pinned ledger.  They are
 * mechanically reproducible but not machine-checked in Lean —
 * that upgrade is called out in the paper (H5).
 *
 * Theorems:
 *
 *   T3 (monotonicity of the gate over τ):
 *       ∀ σ ∈ [0, 1], τ₁, τ₂ ∈ [0, 1]:
 *           τ₁ < τ₂ ∧ gate(σ, τ₁) = ACCEPT
 *         ⇒ gate(σ, τ₂) = ACCEPT.
 *
 *   T4 (commutativity of independent gates):
 *       ∀ σ₁, σ₂, τ:
 *           (gate(σ₁, τ) ∧ gate(σ₂, τ))
 *         = (gate(σ₂, τ) ∧ gate(σ₁, τ)).
 *       (Trivially true for the ref gate; we still exhaust
 *       because A-series composable gates may reorder.)
 *
 *   T5 (encode/decode idempotence of the wire protocol):
 *       ∀ m ∈ payloads:
 *           decode(encode(decode(encode(m)))) = decode(encode(m)).
 *       Shown by bit-for-bit round-trip on the σ-Protocol frames
 *       from D5 across every message type.
 *
 *   T6 (per-call latency bound):
 *       ∀ (σ, τ):  execution_time(gate(σ, τ)) < COS_FORMAL_T6_NS_BOUND
 *       (default 250ns per call on commodity CPUs — well above
 *       the 10ns aspirational bound in the rule card; the harness
 *       records the observed median and 99th-percentile).
 *
 * Evidence ledger output:
 *   { "T3": { "witnesses": N, "violations": 0, "discharged": true },
 *     "T4": ... }
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_FORMAL_H
#define COS_SIGMA_FORMAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Default bound: 250 nanoseconds per gate call.  Chosen as a
 * loose upper bound that holds on every supported target
 * including -O0 builds.  Tighter bounds live in the paper. */
#ifndef COS_FORMAL_T6_NS_BOUND
#define COS_FORMAL_T6_NS_BOUND 250
#endif

typedef struct {
    const char *theorem_id;   /* "T3", "T4", "T5", "T6"            */
    const char *description;
    uint32_t    witnesses;
    uint32_t    violations;
    int         discharged;
    /* Extra numeric evidence per theorem (unused fields are 0). */
    uint64_t    t6_median_ns;
    uint64_t    t6_p99_ns;
    uint64_t    t6_bound_ns;
} cos_formal_result_t;

int  cos_sigma_formal_check_T3(cos_formal_result_t *out);
int  cos_sigma_formal_check_T4(cos_formal_result_t *out);
int  cos_sigma_formal_check_T5(cos_formal_result_t *out);
int  cos_sigma_formal_check_T6(cos_formal_result_t *out);

/* Run all four.  Returns 0 if every theorem is discharged,
 * otherwise the id of the first violating theorem
 * (3/4/5/6). */
int  cos_sigma_formal_check_all(cos_formal_result_t out[4]);

int  cos_sigma_formal_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_FORMAL_H */
