/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v150 σ-Swarm-Intelligence — debate + adversarial verify.
 *
 * v114 routes tasks to specialists (one answer per call).  v150
 * runs a three-round debate between N=3 specialists who can see
 * each other's (answer, σ) profiles and adopt better answers.  It
 * also exposes an adversarial verifier loop in which one
 * specialist produces a σ-scored critique of another's answer —
 * a successful critique (low σ_critique) lifts the defender's σ
 * (uncertainty rises), while a failed critique (high σ_critique)
 * vindicates the defender (σ drops).  This is the "resonance not
 * consensus" principle: consensus is measured, not forced.
 *
 *   σ_consensus  = σ(final σ's) / μ(final σ's)
 *                  variance / mean of the post-debate σ values;
 *                  low ⇒ the swarm converged.
 *
 *   σ_collective = geomean(final σ's after adversarial verify)
 *                  the proconductor metric: one number for the
 *                  whole swarm, honest under one bad specialist.
 *
 * Each specialist has a base-skill vector aligned with a topic
 * tag (e.g. "math", "code", "bio").  σ per answer is derived
 * deterministically from the cosine between the question's topic
 * tag and the specialist's affinity, plus a SplitMix64-seeded
 * novelty jitter.  v150.0 is pure C11; v150.1 wires v114
 * swarm-router as the real specialist source, v128 mesh gossip
 * for cross-node debate, and v124 continual for orbital skill
 * specialisation under σ pressure.
 */
#ifndef COS_V150_SWARM_H
#define COS_V150_SWARM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V150_N_SPECIALISTS 3
#define COS_V150_N_ROUNDS      3
#define COS_V150_NAME_MAX      24
#define COS_V150_TOPIC_MAX     24
#define COS_V150_ANSWER_MAX    96
#define COS_V150_QUESTION_MAX  96
#define COS_V150_N_TOPIC_DIM   4

typedef struct cos_v150_specialist {
    char  name [COS_V150_NAME_MAX];
    char  topic[COS_V150_TOPIC_MAX];
    float affinity[COS_V150_N_TOPIC_DIM];  /* unit vector        */
} cos_v150_specialist_t;

typedef struct cos_v150_answer {
    int   specialist_id;
    int   round;
    int   adopted_from;        /* −1 if original                 */
    float sigma;
    char  text[COS_V150_ANSWER_MAX];
} cos_v150_answer_t;

typedef struct cos_v150_critique {
    int   adversary_id;
    int   defender_id;
    float sigma_critique;
    int   rebuttal_found;      /* 1 iff sigma_critique ≤ τ       */
    float defender_sigma_after;/* post-critique σ on defender    */
} cos_v150_critique_t;

typedef struct cos_v150_debate {
    char  question[COS_V150_QUESTION_MAX];
    char  topic   [COS_V150_TOPIC_MAX];
    float topic_tag[COS_V150_N_TOPIC_DIM];
    cos_v150_answer_t  rounds[COS_V150_N_ROUNDS]
                             [COS_V150_N_SPECIALISTS];
    float sigma_final   [COS_V150_N_SPECIALISTS]; /* post-round   */
    float sigma_consensus;
    float sigma_collective;
    int   converged;           /* all three specialists settled  */
    int   best_specialist;     /* argmin final σ                  */
    cos_v150_critique_t critiques[COS_V150_N_SPECIALISTS];
    int   n_critiques;
    uint64_t seed;
} cos_v150_debate_t;

typedef struct cos_v150_swarm {
    cos_v150_specialist_t spec[COS_V150_N_SPECIALISTS];
    /* Emergent specialisation tally: per specialist × topic, the
     * count of debates in which that pair delivered argmin σ.   */
    int   win_count[COS_V150_N_SPECIALISTS][COS_V150_N_TOPIC_DIM];
    int   debates;
    uint64_t prng;
} cos_v150_swarm_t;

void  cos_v150_swarm_init  (cos_v150_swarm_t *s, uint64_t seed);
void  cos_v150_seed_defaults(cos_v150_swarm_t *s);

/* Topic hashing: string → unit vector in R^COS_V150_N_TOPIC_DIM.
 * Deterministic and case-insensitive in the ASCII sense. */
void  cos_v150_topic_vec(const char *topic,
                         float out[COS_V150_N_TOPIC_DIM]);

/* Run a full 3-round debate on `question` with `topic` tag.
 * Populates *debate; returns 0 on success. */
int   cos_v150_debate_run(cos_v150_swarm_t *s,
                          const char *question,
                          const char *topic,
                          cos_v150_debate_t *out);

/* Adversarial verification: each specialist challenges the
 * specialist whose final σ is argmin.  Defender's σ moves
 * according to σ_critique (low σ_critique ⇒ defender σ rises).
 * tau_rebuttal is the critique threshold (default 0.30).
 * Returns number of critiques emitted (≤ N−1). */
int   cos_v150_adversarial_verify(cos_v150_debate_t *debate,
                                  float tau_rebuttal);

/* σ_collective = geomean of *debate->sigma_final.  Idempotent. */
float cos_v150_sigma_collective(const cos_v150_debate_t *d);

/* σ_consensus = stdev(final σ) / mean(final σ).               */
float cos_v150_sigma_consensus (const cos_v150_debate_t *d);

/* Argmin-σ routing: returns specialist index for topic.        */
int   cos_v150_route_by_topic(const cos_v150_swarm_t *s,
                              const char *topic);

int   cos_v150_debate_to_json(const cos_v150_debate_t *d,
                              char *buf, size_t cap);
int   cos_v150_swarm_to_json (const cos_v150_swarm_t *s,
                              char *buf, size_t cap);

int   cos_v150_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
