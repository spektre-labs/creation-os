/*
 * v301 σ-ZKP — verifiable σ without leaking data.
 *
 *   σ-gate says allow/deny.  A zero-knowledge proof
 *   says "σ was computed honestly".  Put together,
 *   the recipient trusts the decision without seeing
 *   the raw input, the model weights, or the user's
 *   data — trust without transparency.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Verifiable σ-gate (exactly 3 canonical proofs):
 *     `well_formed_proof`  σ_proof=0.05 VALID;
 *     `edge_case_proof`    σ_proof=0.30 VALID;
 *     `forged_proof`       σ_proof=0.85 INVALID.
 *     Contract: 3 rows in canonical order; σ_proof
 *     strictly increasing; `VALID iff σ_proof ≤
 *     τ_proof = 0.40` (both branches fire); `reveals_raw
 *     = false` on every row — even a rejected proof
 *     never leaks the data.
 *
 *   ZK-inference roles (exactly 3 canonical parties):
 *     `client`   sees answer+σ+proof, NOT raw/weights;
 *     `cloud`    sees raw+weights+answer+σ+proof (it
 *                produced them, authorised);
 *     `verifier` sees σ+proof only (no answer, no
 *                raw, no weights).
 *     Contract: 3 rows in canonical order;
 *     `client.sees_raw_input = false` AND
 *     `client.sees_model_weights = false`;
 *     `verifier.sees_raw_input = false` AND
 *     `verifier.sees_model_weights = false` AND
 *     `verifier.sees_answer = false`;
 *     `zk_privacy_holds = true` iff every non-cloud
 *     role satisfies those predicates.
 *
 *   Model integrity (exactly 3 canonical scenarios):
 *     `advertised_served`  σ_integrity=0.10 OK;
 *     `silent_downgrade`   σ_integrity=0.75 DETECTED;
 *     `advertised_match`   σ_integrity=0.12 OK.
 *     Contract: `detected_mismatch iff σ_integrity >
 *     τ_integrity = 0.50` (both branches fire);
 *     exactly 2 OK AND exactly 1 DETECTED — a
 *     cheaper-model swap shows up as higher σ, not as
 *     a dropped packet.
 *
 *   SCSL + ZKP (exactly 3 canonical policy cases):
 *     `allowed_purpose_a`   σ_policy=0.08 ATTESTED;
 *     `allowed_purpose_b`   σ_policy=0.20 ATTESTED;
 *     `disallowed_purpose`  σ_policy=0.90 REJECTED.
 *     Contract: `attested iff σ_policy ≤ τ_policy =
 *     0.50` (both branches fire);
 *     `purpose_revealed = false` on every row —
 *     compliance without disclosure.
 *
 *   σ_zkp (surface hygiene):
 *     σ_zkp = 1 − (
 *       proof_rows_ok + proof_order_ok +
 *         proof_polarity_ok + proof_no_reveal_ok +
 *       role_rows_ok + role_client_hidden_ok +
 *         role_verifier_hidden_ok + zk_privacy_holds +
 *       integrity_rows_ok + integrity_polarity_ok +
 *         integrity_count_ok +
 *       scsl_rows_ok + scsl_polarity_ok +
 *         scsl_no_reveal_ok
 *     ) / (3 + 1 + 1 + 1 + 3 + 1 + 1 + 1 +
 *          3 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_zkp == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 proof rows; σ strictly increasing; VALID
 *        iff σ ≤ 0.40 (both branches); reveals_raw
 *        false everywhere.
 *     2. 3 role rows; client + verifier cannot see
 *        raw input or weights; verifier cannot see
 *        answer; zk_privacy_holds = true.
 *     3. 3 integrity rows; detected_mismatch iff
 *        σ > 0.50 (both branches); exactly 2 OK + 1
 *        DETECTED.
 *     4. 3 SCSL rows; attested iff σ ≤ 0.50 (both
 *        branches); purpose_revealed = false
 *        everywhere.
 *     5. σ_zkp ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v301.1 (named, not implemented): real
 *     commitment-based σ proofs (Pedersen / Halo2)
 *     for the v293 hagia-sofia continuous-use kernel,
 *     a verifier-only JSON schema shipped with the
 *     `cos` CLI, and an SCSL-purpose commitment
 *     registry signed by the v60..v74 License-Bound
 *     Receipt trust chain.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V301_ZKP_H
#define COS_V301_ZKP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V301_N_PROOF     3
#define COS_V301_N_ROLE      3
#define COS_V301_N_INTEGRITY 3
#define COS_V301_N_SCSL      3

#define COS_V301_TAU_PROOF     0.40f
#define COS_V301_TAU_INTEGRITY 0.50f
#define COS_V301_TAU_POLICY    0.50f

typedef struct {
    char  label[20];
    float sigma_proof;
    bool  valid;
    bool  reveals_raw;
} cos_v301_proof_t;

typedef struct {
    char role[10];
    bool sees_raw_input;
    bool sees_model_weights;
    bool sees_answer;
    bool sees_sigma;
    bool sees_proof;
} cos_v301_role_t;

typedef struct {
    char  scenario[22];
    float sigma_integrity;
    bool  detected_mismatch;
    char  verdict[10];
} cos_v301_integrity_t;

typedef struct {
    char  policy[20];
    float sigma_policy;
    bool  attested;
    bool  purpose_revealed;
} cos_v301_scsl_t;

typedef struct {
    cos_v301_proof_t      proof    [COS_V301_N_PROOF];
    cos_v301_role_t       role     [COS_V301_N_ROLE];
    cos_v301_integrity_t  integrity[COS_V301_N_INTEGRITY];
    cos_v301_scsl_t       scsl     [COS_V301_N_SCSL];

    int  n_proof_rows_ok;
    bool proof_order_ok;
    bool proof_polarity_ok;
    bool proof_no_reveal_ok;

    int  n_role_rows_ok;
    bool role_client_hidden_ok;
    bool role_verifier_hidden_ok;
    bool zk_privacy_holds;

    int  n_integrity_rows_ok;
    bool integrity_polarity_ok;
    bool integrity_count_ok;

    int  n_scsl_rows_ok;
    bool scsl_polarity_ok;
    bool scsl_no_reveal_ok;

    float sigma_zkp;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v301_state_t;

void   cos_v301_init(cos_v301_state_t *s, uint64_t seed);
void   cos_v301_run (cos_v301_state_t *s);
size_t cos_v301_to_json(const cos_v301_state_t *s,
                         char *buf, size_t cap);
int    cos_v301_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V301_ZKP_H */
