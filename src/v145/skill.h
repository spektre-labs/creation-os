/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v145 σ-Skill — atomic skill library.
 *
 * The RSI 2026 field consensus (ICLR 2026 "Atomic Skill
 * Acquisition") says self-improving agents should learn small
 * *reusable* skills rather than re-train whole models.  In
 * Creation OS a skill is a triple:
 *
 *     {LoRA-adapter, prompt-template, self-test}
 *
 * stored on disk as (conceptually):
 *
 *     skills/<name>.skill/
 *       adapter.safetensors       # LoRA, rank-R, ~200 KB
 *       template.txt              # "Solve step by step…"
 *       test.jsonl                # ≥ 5 eval items w/ ground truth
 *       meta.toml                 # σ_avg, pass-rate, learned_at
 *
 * v145.0 is the *in-memory, deterministic* kernel that manages
 * this library: acquire (σ-gated install), route (argmin-σ on
 * query-class), stack (LoRA-merge σ-aggregation), share (mesh
 * broadcast of low-σ skills), evolve (CMA-ES stand-in that
 * produces a lower-σ variant), plus JSON serialisation so v128
 * mesh can replicate the library byte-identically.  The on-disk
 * layout, real LoRA-merge, real CMA-ES driver and v128 gossip
 * wire-in are v145.1.
 *
 * Contracts (asserted in the self-test + merge-gate):
 *
 *   S1  Acquiring a skill whose σ_avg ≥ τ_install is *rejected*:
 *       it is not installed into the library (weights-free, but
 *       symbolically gated by σ).
 *   S2  Route(topic) returns the index of the installed skill
 *       with that target_topic whose σ_avg is minimal; -1 if
 *       none installed.
 *   S3  Stack(indices) returns σ_stacked ≤ min(σ_i) — LoRA-merge
 *       empirically reduces σ by √n at most, we use a clamped
 *       approximation σ_stacked = min_σ · 1/√n.
 *   S4  Share(τ) exposes exactly the skills with σ_avg < τ,
 *       setting is_shareable=1; all others keep is_shareable=0.
 *   S5  Evolve(idx) produces a variant σ_variant < σ_current
 *       when the seed strictly improves, and is a no-op
 *       otherwise (non-monotone regressions are never installed).
 */
#ifndef COS_V145_SKILL_H
#define COS_V145_SKILL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V145_MAX_SKILLS   64
#define COS_V145_NAME_CAP     40
#define COS_V145_TOPIC_CAP    24

/* σ-thresholds with repo-wide defaults.  All tunable per-call;
 * these are the values the self-test and merge-gate assume. */
#define COS_V145_TAU_INSTALL  0.60f   /* σ ≥ 0.60 ⇒ do not install */
#define COS_V145_TAU_SHARE    0.35f   /* σ < 0.35 ⇒ shareable       */
#define COS_V145_RANK_DEFAULT 4

typedef struct cos_v145_skill {
    char     name[COS_V145_NAME_CAP];
    char     target_topic[COS_V145_TOPIC_CAP];
    int      rank;                 /* LoRA rank (symbolic, default 4) */
    int      size_kb;              /* serialized size estimate       */
    int      tests_passed;
    int      tests_total;
    float    sigma_avg;            /* skill's own σ on its self-test */
    int      is_shareable;         /* set by cos_v145_share          */
    uint64_t created_seed;         /* PRNG seed used at acquire time */
    int      generation;           /* bumped by cos_v145_evolve      */
} cos_v145_skill_t;

typedef struct cos_v145_library {
    int              n_skills;
    int              n_rejected;   /* σ ≥ τ_install attempts         */
    cos_v145_skill_t skills[COS_V145_MAX_SKILLS];
} cos_v145_library_t;

/* Initialise an empty library. */
void cos_v145_library_init(cos_v145_library_t *lib);

/* Simulate acquiring a skill from an v141-generated curriculum
 * item + v120 distill pair.  difficulty ∈ [0.0, 1.0] scales the
 * resulting σ_avg (harder topic ⇒ higher σ).  Returns 0 on
 * install, 1 on σ-gate rejection (contract S1), <0 on arg error.
 * Deterministic in `seed`. */
int  cos_v145_acquire(cos_v145_library_t *lib,
                      const char *name,
                      const char *target_topic,
                      float difficulty,
                      uint64_t seed,
                      float tau_install,
                      cos_v145_skill_t *out /* optional */);

/* Route a query (identified by its topic string) to an installed
 * skill.  Returns the index of the argmin-σ match, or -1 if the
 * library has no skill with that target_topic.  Contract S2. */
int  cos_v145_route(const cos_v145_library_t *lib,
                    const char *topic);

/* Stack multiple skills (LoRA-merge).  Writes σ_stacked to
 * *out_sigma.  Returns 0 on success, -1 if any index is invalid
 * or n == 0.  Contract S3. */
int  cos_v145_stack(const cos_v145_library_t *lib,
                    const int *indices, int n,
                    float *out_sigma);

/* Mark skills with σ_avg < tau as shareable.  Returns the count
 * of shareable skills.  Contract S4. */
int  cos_v145_share(cos_v145_library_t *lib, float tau);

/* Produce a CMA-ES-style variant of skill[idx] by perturbing
 * rank + threshold deterministically from `seed`.  If the variant
 * has σ_variant < σ_current it replaces the skill in place,
 * generation++; else the library is unchanged.  Returns 1 if an
 * improvement was installed, 0 on no-op, <0 on arg error.
 * Contract S5. */
int  cos_v145_evolve(cos_v145_library_t *lib, int idx, uint64_t seed);

/* Acquire the five default topics (math/code/history/language/logic)
 * with difficulties that reliably install all five on seed 0xC05.
 * Helper for the self-test and CLI demo. */
int  cos_v145_seed_default(cos_v145_library_t *lib, uint64_t seed);

int  cos_v145_skill_to_json(const cos_v145_skill_t *s,
                            char *buf, size_t cap);
int  cos_v145_library_to_json(const cos_v145_library_t *lib,
                              char *buf, size_t cap);

int  cos_v145_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
