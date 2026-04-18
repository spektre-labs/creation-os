/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v146 σ-Genesis — automated kernel-skeleton generation.
 *
 * Every v-layer in Creation OS has the same six-file structure:
 *
 *     src/vN/kernel.h           (API + contracts)
 *     src/vN/kernel.c           (implementation)
 *     src/vN/main.c             (CLI --self-test / --demo)
 *     tests/vN/test.c           (≥ 5 deterministic tests)
 *     docs/vN/README.md         (honest-claims page)
 *     Makefile entries          (check-vN-* → check-vN)
 *
 * v146 formalises that template and emits the skeleton
 * automatically from a *gap descriptor* produced by v133 meta
 * (weakness axis) + v140 causal attribution (which channels
 * lack coverage) + v143 benchmark (concrete failing category).
 *
 * The σ-gate is on the *generated code*, not on a live model:
 * σ_code measures how complete / internally consistent the
 * emitted skeleton is (all six files present, every declared
 * symbol has a stub, tests compile, names match).  Low σ_code
 * ⇒ merge candidate; high σ_code ⇒ reject.  Three consecutive
 * rejections latch the genesis loop into *paused* — a contract
 * shared with v144 RSI and surfaced in the v148 sovereign loop.
 *
 * v146.0 is deliberately offline and weights-free:
 *
 *   - Skeletons are produced from a *deterministic* template
 *     parameterised by (name, version, gap_descriptor, seed).
 *   - σ_code is computed from the template's own completeness
 *     plus a seed-dependent jitter so the merge-gate can assert
 *     "on seed 0xDEEDF00D, σ_code = 0.X ± ε" byte-identically.
 *   - No file I/O unless the caller opts into cos_v146_write();
 *     the in-memory skeleton is enough for σ-gating + HITL.
 *
 * v146.1 wires v114 swarm (8B code specialist) as the actual
 * generator, v135 prolog consistency checker on the emitted AST,
 * and a webhook to v108 UI so the user sees pending candidates.
 */
#ifndef COS_V146_GENESIS_H
#define COS_V146_GENESIS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V146_MAX_CANDIDATES   16
#define COS_V146_NAME_CAP         32
#define COS_V146_GAP_CAP          96
#define COS_V146_SKELETON_CAP     4096
#define COS_V146_PAUSE_AFTER       3    /* consecutive rejections    */
#define COS_V146_TAU_MERGE         0.35f /* σ_code < τ ⇒ candidate  */

typedef enum {
    COS_V146_PENDING  = 0,
    COS_V146_APPROVED = 1,
    COS_V146_REJECTED = 2,
    COS_V146_GATED_OUT = 3   /* σ_code ≥ τ at generation time      */
} cos_v146_status_t;

typedef struct cos_v146_candidate {
    char             name[COS_V146_NAME_CAP];
    char             gap[COS_V146_GAP_CAP];
    int              version;          /* target vN (e.g. 149)    */
    uint64_t         seed;
    float            sigma_code;       /* quality / completeness  */
    int              n_tests;          /* tests emitted (≥ 5)     */
    int              bytes_header;
    int              bytes_source;
    int              bytes_test;
    int              bytes_readme;
    cos_v146_status_t status;
} cos_v146_candidate_t;

typedef struct cos_v146_state {
    int                   n_candidates;
    int                   n_approved;
    int                   n_rejected;
    int                   n_gated_out;
    int                   consecutive_rejects;
    int                   paused;           /* 1 iff ≥ PAUSE_AFTER */
    cos_v146_candidate_t  candidates[COS_V146_MAX_CANDIDATES];
} cos_v146_state_t;

void cos_v146_init(cos_v146_state_t *g);

/* Attempt to generate a kernel skeleton for (name, version, gap,
 * seed).  Returns:
 *   >= 0   index of the candidate stored in g->candidates[],
 *          which may be PENDING (σ-gate passed) or GATED_OUT
 *          (σ_code ≥ τ_merge — candidate recorded but not
 *          offered for merge).
 *   -1     argument error / state full.
 *
 * If `out_skeleton` is non-NULL it is filled with a concatenation
 * of the four emitted files separated by `---FILE: path---\n`
 * markers (convenient for writing to disk or piping to a
 * reviewer).  The buffer must have room for ~4 KB. */
int  cos_v146_generate(cos_v146_state_t *g,
                       int version,
                       const char *name,
                       const char *gap_descriptor,
                       uint64_t seed,
                       float tau_merge,
                       char *out_skeleton, size_t cap);

/* Optionally persist an already-generated candidate to disk under
 * out_dir/vN/.  Writes kernel.h, kernel.c, test.c, README.md in
 * the template layout.  Returns 0 on success.  A zero-length
 * out_dir is treated as "current directory".  */
int  cos_v146_write_candidate(const cos_v146_state_t *g, int idx,
                              const char *out_dir);

/* HITL controls. */
int  cos_v146_approve(cos_v146_state_t *g, int idx);
int  cos_v146_reject (cos_v146_state_t *g, int idx);
void cos_v146_resume (cos_v146_state_t *g);

int  cos_v146_state_to_json(const cos_v146_state_t *g,
                            char *buf, size_t cap);
int  cos_v146_candidate_to_json(const cos_v146_candidate_t *c,
                                char *buf, size_t cap);

int  cos_v146_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
