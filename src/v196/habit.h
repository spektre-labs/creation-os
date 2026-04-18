/*
 * v196 σ-Habit — pattern detection + compiled routines with a
 * cortex/cerebellum σ-switch.
 *
 *   Human brains habituate repetitive tasks into fast,
 *   automatic routines (cerebellum).  v196 mirrors that:
 *     * audit log (v181) is scanned for recurring prompt
 *       patterns;
 *     * patterns with ≥ τ_repeat occurrences become habit
 *       candidates;
 *     * candidates are “compiled” (v0: closed-form cycle-count
 *       model of a v137 LLVM path) into a habit slot that
 *       answers the prompt in `cycles_compiled` ticks instead
 *       of `cycles_reasoning`;
 *     * σ is the cortex/cerebellum switch — if σ on the
 *       compiled answer exceeds τ_break, control returns to
 *       full reasoning (cortex); otherwise the habit
 *       (cerebellum) is taken.
 *
 *   Contracts (v0):
 *     1. At least 3 distinct habits compile.
 *     2. Each habit has speedup ≥ 10× (cycles_reasoning /
 *        cycles_compiled).
 *     3. σ stays below τ_break for the steady-state portion
 *        of the trajectory (routine mode).
 *     4. On an injected σ spike, a habit break-out fires
 *        and control returns to cortex — not a silent
 *        miscompile.
 *     5. Habit library is hash-chained and replays.
 *     6. Byte-deterministic.
 *
 * v196.1 (named): live ~/.creation-os/habits/ directory
 *   listing, real v137 LLVM compile, streaming v181 scan.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V196_HABIT_H
#define COS_V196_HABIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V196_N_PATTERNS   8
#define COS_V196_N_HABITS     4
#define COS_V196_N_TRACE     32
#define COS_V196_STR_MAX     32

typedef enum {
    COS_V196_MODE_CORTEX     = 0,   /* full reasoning */
    COS_V196_MODE_CEREBELLUM = 1    /* compiled habit */
} cos_v196_mode_t;

typedef struct {
    char   name[COS_V196_STR_MAX];   /* "weather_check", etc. */
    int    occurrences;              /* audit-log hit count */
    bool   is_habit;                 /* ≥ τ_repeat */
    int    cycles_reasoning;         /* full-reasoning cost */
    int    cycles_compiled;          /* habit cost */
    float  speedup;                  /* cycles_reasoning / cycles_compiled */
    float  sigma_steady;             /* σ under steady inputs */
    uint64_t habit_hash;             /* chained */
} cos_v196_pattern_t;

typedef struct {
    int    t;
    int    pattern_id;
    float  sigma;
    int    mode;                     /* cos_v196_mode_t */
    int    cycles;                   /* cycles actually spent */
    bool   break_out;                /* habit aborted → cortex */
} cos_v196_tick_t;

typedef struct {
    int                 n_patterns;
    cos_v196_pattern_t  patterns[COS_V196_N_PATTERNS];

    int                 n_habits;
    float               tau_repeat;          /* e.g. 5 */
    float               tau_break;           /* σ switch threshold */
    float               min_speedup;         /* e.g. 10x */

    int                 n_ticks;
    cos_v196_tick_t     trace[COS_V196_N_TRACE];
    int                 n_break_outs;

    bool                chain_valid;
    uint64_t            terminal_hash;

    uint64_t            seed;
} cos_v196_state_t;

void   cos_v196_init(cos_v196_state_t *s, uint64_t seed);
void   cos_v196_build(cos_v196_state_t *s);
void   cos_v196_run(cos_v196_state_t *s);

size_t cos_v196_to_json(const cos_v196_state_t *s,
                         char *buf, size_t cap);

int    cos_v196_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V196_HABIT_H */
