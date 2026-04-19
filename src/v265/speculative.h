/*
 * v265 σ-Speculative — σ-guided speculative decoding.
 *
 *   Draft model (BitNet 1.5B) proposes N tokens fast.
 *   Verifier (AirLLM 70B) checks all N in one pass.
 *   σ picks *how many* to speculate, and every
 *   speculated token additionally clears a σ-gate.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Model pair (exactly 2, canonical):
 *     { draft:    bitnet-1.5B-local, role="draft"    }
 *     { verifier: airllm-70B-local,  role="verifier" }
 *
 *   σ-guided speculation-length band (exactly 4 rows,
 *   canonical order):
 *     σ_context ≤ 0.20  → spec_len == 12  (easy)
 *     0.20 <  σ ≤ 0.40  → spec_len ==  8
 *     0.40 <  σ ≤ 0.60  → spec_len ==  6
 *     σ_context >  0.60  → spec_len ==  4  (hard)
 *   Rule: spec_len(σ) is strictly non-increasing in σ
 *   AND every band fires (one row per band).
 *
 *   Multi-draft duel (exactly 3 fixtures):
 *     Two drafts A / B produce proposals; verifier
 *     keeps the lower-σ draft.  Each fixture: `draft_a`,
 *     `sigma_a`, `draft_b`, `sigma_b`, `winner ∈ {A,B}`.
 *     `winner == argmin(sigma)`; both A-wins AND B-wins
 *     must fire across the 3 rows.
 *
 *   Speculation + σ-gate (exactly 4 fixtures):
 *     Rule: `σ_speculated ≤ τ_spec = 0.35` → `ACCEPT`;
 *     else → `REJECT`.  v0 requires ≥ 1 ACCEPT AND ≥ 1
 *     REJECT (the extra σ-gate has teeth — a token the
 *     verifier would keep can still be rejected).
 *
 *   Throughput claim (strict):
 *     tok_per_s_plain      < tok_per_s_sigma_spec
 *     speedup_x  = tok_per_s_sigma_spec /
 *                  tok_per_s_plain
 *     speedup_x >= 2.0          (2×–4× target)
 *
 *   σ_speculative (surface hygiene):
 *       σ_speculative = 1 −
 *         (models_ok + bands_ok + monotone_ok +
 *          duels_ok + both_winners_ok +
 *          gate_rows_ok + both_gate_branches_ok +
 *          speedup_ok) /
 *         (2 + 4 + 1 + 3 + 1 + 4 + 1 + 1)
 *   v0 requires `σ_speculative == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 2 models: roles "draft" and "verifier".
 *     2. Exactly 4 bands with canonical spec_len AND
 *        spec_len strictly non-increasing in σ.
 *     3. Exactly 3 multi-draft duels, winner ==
 *        argmin(σ_draft); both A-wins AND B-wins fire.
 *     4. Exactly 4 gate fixtures, decision matches σ vs
 *        τ_spec = 0.35; ≥ 1 ACCEPT AND ≥ 1 REJECT.
 *     5. Throughput: plain < sigma_spec AND speedup ≥ 2.0.
 *     6. σ_speculative ∈ [0, 1] AND == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v265.1 (named, not implemented): live draft+verifier
 *     inference wired to v262 hybrid engine, live σ feed
 *     from v226 attention driving spec-length, real GPU
 *     throughput benchmark producing measured tok/s.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V265_SPECULATIVE_H
#define COS_V265_SPECULATIVE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V265_N_MODELS  2
#define COS_V265_N_BANDS   4
#define COS_V265_N_DUELS   3
#define COS_V265_N_GATES   4

typedef enum {
    COS_V265_GATE_ACCEPT = 1,
    COS_V265_GATE_REJECT = 2
} cos_v265_gate_dec_t;

typedef struct {
    char  name[24];
    char  role[10];     /* "draft" or "verifier" */
} cos_v265_model_t;

typedef struct {
    char   label[16];
    float  sigma_lo;
    float  sigma_hi;    /* canonical upper bound, inclusive */
    int    spec_len;
} cos_v265_band_t;

typedef struct {
    char  draft_a[16];
    float sigma_a;
    char  draft_b[16];
    float sigma_b;
    char  winner[2];    /* "A" or "B" */
} cos_v265_duel_t;

typedef struct {
    char                 token[16];
    float                sigma_speculated;
    cos_v265_gate_dec_t  decision;
} cos_v265_gate_t;

typedef struct {
    float  tok_per_s_plain;
    float  tok_per_s_sigma_spec;
    float  speedup_x;
    bool   speedup_ok;
} cos_v265_throughput_t;

typedef struct {
    cos_v265_model_t        models [COS_V265_N_MODELS];
    cos_v265_band_t         bands  [COS_V265_N_BANDS];
    cos_v265_duel_t         duels  [COS_V265_N_DUELS];
    cos_v265_gate_t         gates  [COS_V265_N_GATES];
    cos_v265_throughput_t   throughput;

    float tau_spec;

    int   n_models_ok;
    int   n_bands_ok;
    bool  monotone_ok;
    int   n_duels_ok;
    int   n_a_wins;
    int   n_b_wins;
    int   n_accept;
    int   n_reject;

    float sigma_speculative;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v265_state_t;

void   cos_v265_init(cos_v265_state_t *s, uint64_t seed);
void   cos_v265_run (cos_v265_state_t *s);

size_t cos_v265_to_json(const cos_v265_state_t *s,
                         char *buf, size_t cap);

int    cos_v265_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V265_SPECULATIVE_H */
