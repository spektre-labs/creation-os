/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v161 σ-Adversarial-Train — learn from red-team attacks.
 *
 * v122 σ-red-team tests safety.  v161 turns successful attacks
 * into training data: the replay buffer stores every red-team
 * hit, the DPO-pair builder synthesizes (chosen, rejected)
 * pairs where `chosen` is the σ-gated refusal and `rejected` is
 * the attacker-observed vulnerable response, and the continuous
 * hardening loop (driven by v144 RSI) replays attacks after
 * training to confirm that the specific vulnerability is now
 * closed (σ_attack ≥ τ_refuse on all previously-successful
 * prompts).
 *
 * v161 also extracts an attack σ-signature: a deterministic
 * mapping from attack_type to its typical σ-channel profile
 * (e.g. prompt injection → σ_entropy high; jailbreak →
 * σ_n_effective low).  Future attacks that match a signature
 * are flagged faster.
 *
 * v161.0 (this file) is offline, deterministic, and model-less:
 * attacks live in a seeded fixture, DPO pairs are synthesized
 * from a template, hardening is simulated by applying a
 * per-attack σ-boost and re-checking the gate.  No weights
 * change; v125 DPO is *named* as the training path but not
 * invoked here.
 *
 * v161.1 (named, not shipped):
 *   - ingest real v122 red-team JSONL → replay buffer
 *   - emit DPO JSONL at packaging/dpo/adversarial_v161.jsonl
 *   - plug into v125 DPO training at v144 RSI cycle boundaries
 *   - verify closure by actually re-running v122 against the
 *     new adapter before promoting it
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V161_ADV_TRAIN_H
#define COS_V161_ADV_TRAIN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V161_MAX_ATTACKS   32
#define COS_V161_MAX_TYPES     8
#define COS_V161_MAX_PROMPT    192
#define COS_V161_MAX_RESPONSE  192
#define COS_V161_MAX_NAME      40

typedef enum {
    COS_V161_ATK_PROMPT_INJECTION = 0,
    COS_V161_ATK_JAILBREAK        = 1,
    COS_V161_ATK_DATA_EXFIL       = 2,
    COS_V161_ATK_SSRF             = 3,
    COS_V161_ATK_ROLE_CONFUSION   = 4,
} cos_v161_attack_type_t;

typedef struct {
    cos_v161_attack_type_t type;
    char        prompt[COS_V161_MAX_PROMPT];
    char        response[COS_V161_MAX_RESPONSE]; /* vulnerable response */
    float       sigma_entropy;      /* attack σ-channel profile */
    float       sigma_n_effective;  /*   — measured at attack time */
    float       sigma_coherence;
    bool        success;            /* true ⇒ attacker exfiltrated harm */
    bool        closed;             /* true ⇒ hardening sealed it */
    float       sigma_attack_now;   /* current σ against this prompt */
} cos_v161_attack_t;

typedef struct {
    cos_v161_attack_type_t type;
    char    name[COS_V161_MAX_NAME];
    float   signature_entropy;      /* characteristic channel value */
    float   signature_n_effective;
    float   signature_coherence;
} cos_v161_signature_t;

typedef struct {
    char chosen[COS_V161_MAX_RESPONSE];   /* σ-gated refusal */
    char rejected[COS_V161_MAX_RESPONSE]; /* vulnerable response */
    char prompt[COS_V161_MAX_PROMPT];
    cos_v161_attack_type_t type;
    float sigma_refuse;  /* σ of the chosen (high ⇒ confident refusal) */
} cos_v161_dpo_pair_t;

typedef struct {
    uint64_t               seed;
    int                    n_attacks;
    cos_v161_attack_t      attacks[COS_V161_MAX_ATTACKS];
    int                    n_signatures;
    cos_v161_signature_t   signatures[COS_V161_MAX_TYPES];
    float                  tau_refuse;      /* σ gate for hardening */
    int                    n_successful_at_start;
    int                    n_successful_after;
    int                    n_closed;
    float                  sigma_hardening; /* geomean σ_attack_now over prev-successful */
} cos_v161_trainer_t;

/* Initialize with a deterministic seeded fixture of 10 attacks
 * across 5 types; 6 are marked successful (attacker exfiltrated
 * harm). */
void cos_v161_trainer_init(cos_v161_trainer_t *t, uint64_t seed);

/* Build DPO pairs for every successful attack.  `pairs` receives
 * up to `cap` entries.  Returns number written. */
int cos_v161_trainer_build_dpo(const cos_v161_trainer_t *t,
                               cos_v161_dpo_pair_t *pairs, int cap);

/* Run the hardening cycle: simulate σ-boost on every
 * successful attack, mark any attack whose σ_attack_now ≥
 * tau_refuse as `closed`.  Returns number of newly-closed attacks.
 */
int cos_v161_trainer_harden(cos_v161_trainer_t *t);

/* Classify a new prompt into an attack type via the stored
 * σ-signature table (nearest signature).  `sigma_entropy`,
 * `sigma_n_effective`, `sigma_coherence` are the 3 observed
 * channels.  Returns the matched type; writes into `score`
 * the L1 distance to the matched signature. */
cos_v161_attack_type_t cos_v161_classify(const cos_v161_trainer_t *t,
                                         float sigma_entropy,
                                         float sigma_n_effective,
                                         float sigma_coherence,
                                         float *score);

/* JSON report of the current trainer state (attacks + signatures
 * + hardening results). */
size_t cos_v161_trainer_to_json(const cos_v161_trainer_t *t,
                                char *buf, size_t cap);

const char *cos_v161_attack_name(cos_v161_attack_type_t a);

int cos_v161_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V161_ADV_TRAIN_H */
