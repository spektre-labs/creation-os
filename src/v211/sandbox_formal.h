/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v211 σ-Sandbox-Formal — formal proof of containment.
 *
 *   v209 enforces containment at runtime.  v211 records
 *   the formal evidence that v209 is *correct*:
 *
 *     A)  Frama-C proofs (simulated in v0):
 *         P1  every I/O path passes σ-action-gate
 *         P2  pre-log precedes every op, post-log
 *             follows every executed op
 *         P3  kill-switch terminates all agent processes
 *         P4  network is closed by default
 *
 *     B)  TLA+ model-check (simulated in v0):
 *         I_safety    contained(agent)
 *         I_audit     audit_complete(action) before execute
 *         L_liveness  kill_switch ⇒ ◇≤100ms terminated
 *         Exhaustive bounded check of 10^6 states
 *         (recorded as a state count, not actually run)
 *
 *     C)  Attack-tree:
 *         T1 filesystem escape
 *         T2 network exfiltration (DNS / HTTP / ICMP)
 *         T3 timing / cache side-channel
 *         T4 IPC to sibling process
 *         T5 seccomp bypass via ptrace
 *       Every tree leaf is annotated `blocked_by` pointing
 *       at one of P1..P4 or a v209 layer.
 *
 *     D)  Certification:
 *         DO-178C DAL-A, IEC 62443, Common Criteria
 *         EAL4+ — each with a σ_cert-ready score (0..1)
 *         stating how ready the v0 corpus is for a real
 *         certification body.
 *
 *   Contracts (v0):
 *     1. Four Frama-C proofs, every one marked
 *        `proved == true` with σ_proof ≤ τ_proof (0.05).
 *     2. The TLA+ bounded check reports ≥ 10^6 states
 *        and ZERO invariant violations.
 *     3. Every attack-tree leaf is `blocked_by != 0`.
 *        No unassigned attack vector.
 *     4. All three certification tracks have
 *        σ_cert_ready ∈ [0, 1].
 *     5. Aggregate σ_formal is the max across all
 *        proofs; when every proof holds it is ≤ τ_proof.
 *     6. FNV-1a chain over the whole artefact is
 *        byte-deterministic.
 *
 *   v211.1 (named): real `frama-c -wp` invocation, real
 *     TLA+ `tlc` run, live attack-tree corpus, actual
 *     certification-body packaging.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V211_SANDBOX_FORMAL_H
#define COS_V211_SANDBOX_FORMAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V211_N_PROOFS       4
#define COS_V211_N_INVARIANTS   3
#define COS_V211_N_ATTACKS      5
#define COS_V211_N_CERTS        3
#define COS_V211_STR_MAX       48

typedef struct {
    int      id;
    char     name[COS_V211_STR_MAX];
    bool     proved;
    float    sigma_proof;           /* residual uncertainty */
    int      blocks_attack_mask;    /* bitmask over attack-tree */
} cos_v211_proof_t;

typedef struct {
    int      id;
    char     name[COS_V211_STR_MAX];
    uint64_t states_explored;
    int      violations;
    float    sigma_model;
} cos_v211_invariant_t;

typedef struct {
    int      id;
    char     vector[COS_V211_STR_MAX];
    int      blocked_by;            /* non-zero proof id */
    float    sigma_residual;
} cos_v211_attack_t;

typedef struct {
    int      id;
    char     standard[COS_V211_STR_MAX];
    float    sigma_cert_ready;
} cos_v211_cert_t;

typedef struct {
    cos_v211_proof_t      proofs [COS_V211_N_PROOFS];
    cos_v211_invariant_t  invs   [COS_V211_N_INVARIANTS];
    cos_v211_attack_t     attacks[COS_V211_N_ATTACKS];
    cos_v211_cert_t       certs  [COS_V211_N_CERTS];

    float                 tau_proof;          /* 0.05 */
    uint64_t              min_states_required;/* 1e6 */

    int                   n_proved;
    int                   n_violations_total;
    float                 sigma_formal;       /* max over proofs */

    bool                  chain_valid;
    uint64_t              terminal_hash;
    uint64_t              seed;
} cos_v211_state_t;

void   cos_v211_init(cos_v211_state_t *s, uint64_t seed);
void   cos_v211_build(cos_v211_state_t *s);
void   cos_v211_run(cos_v211_state_t *s);

size_t cos_v211_to_json(const cos_v211_state_t *s,
                         char *buf, size_t cap);

int    cos_v211_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V211_SANDBOX_FORMAL_H */
