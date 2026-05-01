/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v298 σ-Rosetta — universal translatability.
 *
 *   The Rosetta Stone carries the same text in three
 *   scripts.  2000 years later, hieroglyphics could
 *   be read again.  Code that explains itself —
 *   mathematically, in multiple languages, and in a
 *   human-readable log — survives the same way.  v298
 *   types four v0 manifests: every σ measurement
 *   carries a reason, the spec exists in three
 *   languages, logs are both binary and text, and the
 *   core definition is a timeless formula.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Self-documenting σ (exactly 3 canonical emissions):
 *     `σ=0.35 domain=LLM    reason="entropy high,
 *                                    top_token_prob
 *                                    0.23, 4
 *                                    competing tokens"`;
 *     `σ=0.15 domain=SENSOR reason="noise floor 0.02,
 *                                    signal 0.13,
 *                                    SNR low"`;
 *     `σ=0.60 domain=ORG    reason="disagreement
 *                                    between 3 voters,
 *                                    quorum not met"`.
 *     Contract: 3 rows; 3 DISTINCT domains in order;
 *     every row has `reason_present == true` AND
 *     `reason_length ≥ reason_min_length = 20` — a σ
 *     without a reason is forbidden on the wire.
 *
 *   Multi-language spec (exactly 3 canonical bindings):
 *     `C`      REFERENCE maintained semantic_match=true;
 *     `Python` ADOPTION  maintained semantic_match=true;
 *     `Rust`   SAFETY    maintained semantic_match=true.
 *     Contract: 3 rows in order; 3 DISTINCT roles;
 *     every row `maintained == true` AND
 *     `semantic_match_to_c == true` — if one language
 *     dies, two remain; the C reference binds them.
 *
 *   Human-readable log format (exactly 3 canonical
 *   formats):
 *     `binary` machine=true human=false;
 *     `csv`    machine=true human=true;
 *     `json`   machine=true human=true.
 *     Contract: 3 rows in order; every row
 *     `machine_readable == true`; exactly 1 row with
 *     `human_readable_forever == false` (binary)
 *     AND exactly 2 rows with
 *     `human_readable_forever == true` — the σ-log
 *     is always available both as a fast binary and
 *     as a text format a future reader can decode
 *     without our binary.
 *
 *   Mathematical specification (exactly 3 canonical
 *   invariants):
 *     `sigma_definition`    σ=noise/(signal+noise)
 *                           ages_out=false;
 *     `pythagoras_2500_yr`  a^2+b^2=c^2
 *                           ages_out=false;
 *     `arithmetic_invariant` (a+b)+c=a+(b+c)
 *                           ages_out=false.
 *     Contract: 3 rows in order; every row
 *     `formal_expression_present == true` AND
 *     `ages_out == false` — mathematics does not
 *     expire; σ lives on the same shelf as Pythagoras.
 *
 *   σ_rosetta (surface hygiene):
 *       σ_rosetta = 1 −
 *         (emit_rows_ok + emit_domain_distinct_ok +
 *          emit_reason_ok +
 *          spec_rows_ok + spec_role_distinct_ok +
 *          spec_maintained_ok + spec_match_ok +
 *          fmt_rows_ok + fmt_machine_ok +
 *          fmt_human_count_ok +
 *          math_rows_ok + math_expression_ok +
 *          math_ages_out_ok) /
 *         (3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1 +
 *          3 + 1 + 1)
 *   v0 requires `σ_rosetta == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 σ emissions; 3 distinct domains; every
 *        emission carries a non-empty reason.
 *     2. 3 language bindings; 3 distinct roles;
 *        all maintained; all semantic-match to C.
 *     3. 3 log formats; all machine-readable;
 *        exactly 2 human-readable-forever (CSV and
 *        JSON); exactly 1 not (binary).
 *     4. 3 mathematical invariants; all have a
 *        formal expression; none age out.
 *     5. σ_rosetta ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v298.1 (named, not implemented): live σ-log
 *     double-writes (binary + CSV) wired to the
 *     v268 continuous-batch emitter, a `cos
 *     --explain` flag on the proxy that returns
 *     the reason string verbatim, and CI jobs that
 *     diff the Python / Rust bindings against the
 *     C reference on every merge.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V298_ROSETTA_H
#define COS_V298_ROSETTA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V298_N_EMIT  3
#define COS_V298_N_SPEC  3
#define COS_V298_N_FMT   3
#define COS_V298_N_MATH  3

#define COS_V298_REASON_MIN_LEN 20

typedef struct {
    float sigma;
    char  domain[10];
    char  reason[96];
    bool  reason_present;
} cos_v298_emit_t;

typedef struct {
    char  language[10];
    char  role[12];
    bool  maintained;
    bool  semantic_match_to_c;
} cos_v298_spec_t;

typedef struct {
    char  format[8];
    bool  machine_readable;
    bool  human_readable_forever;
} cos_v298_fmt_t;

typedef struct {
    char  name[24];
    char  formula[32];
    bool  formal_expression_present;
    bool  ages_out;
} cos_v298_math_t;

typedef struct {
    cos_v298_emit_t  emit[COS_V298_N_EMIT];
    cos_v298_spec_t  spec[COS_V298_N_SPEC];
    cos_v298_fmt_t   fmt [COS_V298_N_FMT];
    cos_v298_math_t  math[COS_V298_N_MATH];

    int   n_emit_rows_ok;
    bool  emit_domain_distinct_ok;
    bool  emit_reason_ok;

    int   n_spec_rows_ok;
    bool  spec_role_distinct_ok;
    bool  spec_maintained_ok;
    bool  spec_match_ok;

    int   n_fmt_rows_ok;
    bool  fmt_machine_ok;
    bool  fmt_human_count_ok;

    int   n_math_rows_ok;
    bool  math_expression_ok;
    bool  math_ages_out_ok;

    float sigma_rosetta;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v298_state_t;

void   cos_v298_init(cos_v298_state_t *s, uint64_t seed);
void   cos_v298_run (cos_v298_state_t *s);

size_t cos_v298_to_json(const cos_v298_state_t *s,
                         char *buf, size_t cap);

int    cos_v298_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V298_ROSETTA_H */
