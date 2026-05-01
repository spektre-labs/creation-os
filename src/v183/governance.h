/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v183 σ-Governance-Theory — bounded model check of the σ-governance
 * Kripke structure.
 *
 *   v138 proved the σ-gate correct with Frama-C.  v183 proves the
 *   entire governance model — the 7 axioms, 3 liveness, 4 safety
 *   properties — by exhaustive enumeration over a bounded state
 *   space.  The mirrored TLA+ text lives at
 *   `specs/v183/governance_theory.tla`; v183.0 ships a C
 *   model-checker that walks the same state space and emits a
 *   machine-readable report.
 *
 *   Properties (14 total):
 *
 *     Axioms:
 *       A1  sigma_in_unit_interval      σ ∈ [0,1] in every state
 *       A2  emit_requires_low_sigma     emit ⇒ σ < τ ∧ ¬kernel_deny
 *       A3  abstain_requires_block      abstain ⇒ σ ≥ τ ∨ kernel_deny
 *       A4  learn_monotone              learn ⇒ score ≥ prev_score
 *       A5  forget_intact               forget ⇒ data_removed ∧ hash_chain_intact
 *       A6  steer_reduces_sigma         steer ⇒ σ_after ≤ σ_before
 *       A7  consensus_byzantine         consensus ⇒ agree ≥ 2f+1 ∧ byzantine_detected
 *
 *     Liveness:
 *       L1  progress_always             every state reaches emit ∨ abstain
 *       L2  rsi_improves_one_domain     ∃ domain with monotone score
 *       L3  heal_recovers               ∀ unhealthy . heal → healthy
 *
 *     Safety:
 *       S1  no_silent_failure           every σ-check writes log
 *       S2  no_unchecked_output         emit ⇒ σ-gate fired
 *       S3  no_private_leak             private ⇒ ¬federated
 *       S4  no_regression_propagates    regression ⇒ rollback before emit
 *
 *   Exit invariants (merge-gate):
 *     * All 14 properties PASS.
 *     * Total states visited ≥ 10^5 (reporter aggregates).
 *     * Deterministic output on a fixed seed.
 *
 *   v183.1 (named, not shipped):
 *     - real TLC run against `specs/v183/governance_theory.tla`;
 *     - Zenodo-archived paper
 *       `docs/papers/sigma_governance_formal.md`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V183_GOVERNANCE_H
#define COS_V183_GOVERNANCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V183_N_PROPS     14
#define COS_V183_NAME_MAX    48

typedef struct {
    char     name[COS_V183_NAME_MAX];
    char     kind[16];              /* "axiom" / "liveness" / "safety" */
    bool     passed;
    uint64_t states_visited;        /* bounded domain size */
    uint64_t counterexamples;       /* 0 ⇒ prop holds */
} cos_v183_prop_t;

typedef struct {
    int              n_props;
    cos_v183_prop_t  props[COS_V183_N_PROPS];

    uint64_t         total_states;
    int              n_passed;
    int              n_failed;

    /* Tunables for the bounded model. */
    float            tau;                  /* σ-gate threshold */
    int              sigma_levels;         /* σ discretization */
    int              consensus_nodes;      /* total nodes */
    int              byzantine_nodes;      /* f */
    uint64_t         seed;
} cos_v183_state_t;

void cos_v183_init(cos_v183_state_t *s, uint64_t seed);
void cos_v183_run (cos_v183_state_t *s);

size_t cos_v183_to_json(const cos_v183_state_t *s,
                        char *buf, size_t cap);

int cos_v183_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V183_GOVERNANCE_H */
