/*
 * v191 σ-Constitutional — axiomatic self-steering.
 *
 *   v188 made the value assertions measurable.  v191 elevates
 *   them to a living constitution that every output is checked
 *   against before it leaves the model.  Seven seed axioms:
 *
 *     #1  1 = 1                       declared == realized
 *     #2  σ-honesty                   never claim > 1-σ confidence
 *     #3  no silent failure           every decision is logged
 *     #4  no authority w/o measurement
 *     #5  human primacy               override always available
 *     #6  resonance not consensus     disagreement is signal
 *     #7  no firmware                 produce safety, not theatre
 *
 *   Each axiom is a PREDICATE over (output, σ, metadata):
 *
 *       accept_i = axiom_i.check(output, σ, meta) ∈ {0,1}
 *
 *   Output is ACCEPTED iff  ∧_i accept_i  = 1.
 *   Anything else is REVISE or ABSTAIN.
 *
 *   Axioms are NOT hard-coded: v191 keeps them in
 *   specs/constitution.toml, loads them, and can evolve them
 *   via the (v148-sovereign + v150-debate + v183-TLA+)
 *   proposal pipeline — but every proposal must be formally
 *   consistent with the existing axioms before being merged.
 *
 *   Anti-firmware contract (axiom #7): if a response contains a
 *   disclaimer that σ does NOT support, the constitutional
 *   checker flags it as a violation.  v125-DPO then unlearns
 *   the disclaimer habit.
 *
 *   Merge-gate contracts:
 *     * all 7 axioms present
 *     * constitutional checker rejects ≥ 1 firmware output
 *       with decision != ACCEPT
 *     * ≥ 1 "safe" output accepted with all 7 axiom checks passing
 *     * every violation is hashed into an append-only chain
 *       (so "no silent failure" is itself auditable)
 *     * chain replay verifies byte-identically
 *
 * v191.0 (this file) ships a deterministic fixture of 24
 * candidate outputs (safe, firmware-disclaimer, over-confident,
 * unlogged-abstention, authoritarian, etc.) and a predicate
 * stub for every axiom.
 *
 * v191.1 (named, not shipped):
 *   - real specs/constitution.toml with SHA-256 signature and
 *     git-log change history;
 *   - live v148-sovereign proposal + v183-TLA+ consistency
 *     check before merging new axioms.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V191_CONSTITUTION_H
#define COS_V191_CONSTITUTION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V191_N_AXIOMS    7
#define COS_V191_N_SAMPLES  24
#define COS_V191_STR_MAX    64

typedef enum {
    COS_V191_DECISION_ACCEPT  = 0,
    COS_V191_DECISION_REVISE  = 1,
    COS_V191_DECISION_ABSTAIN = 2
} cos_v191_decision_t;

typedef enum {
    COS_V191_FLAW_NONE              = 0,
    COS_V191_FLAW_FIRMWARE          = 1,  /* unsupported disclaimer */
    COS_V191_FLAW_OVERCONFIDENT     = 2,  /* σ-honesty broken */
    COS_V191_FLAW_UNLOGGED_ABSTAIN  = 3,  /* silent failure */
    COS_V191_FLAW_UNMEASURED_AUTH   = 4,  /* authority w/o measurement */
    COS_V191_FLAW_BLOCKS_HUMAN      = 5,  /* human primacy */
    COS_V191_FLAW_SUPPRESSES_DISAGR = 6,  /* resonance */
    COS_V191_FLAW_DECLARED_NE_REAL  = 7   /* 1=1 */
} cos_v191_flaw_t;

typedef struct {
    int   id;
    char  name[COS_V191_STR_MAX];
    char  predicate_name[COS_V191_STR_MAX];
} cos_v191_axiom_t;

typedef struct {
    int     id;
    int     flaw;                        /* cos_v191_flaw_t */
    float   declared;                    /* what model claimed */
    float   realized;                    /* what is actually true */
    float   sigma;                       /* σ of this output */
    bool    has_disclaimer;
    bool    supported_by_sigma;          /* disclaimer backed by σ */
    bool    logged;
    bool    authority_measured;
    bool    human_override_ok;
    bool    disagreement_preserved;

    /* Per-axiom accept bitmask (1 = pass). */
    int     axiom_pass[COS_V191_N_AXIOMS];
    int     n_axioms_passed;
    int     decision;                    /* cos_v191_decision_t */
    uint64_t hash_prev;
    uint64_t hash_curr;
} cos_v191_sample_t;

typedef struct {
    int                 n_axioms;
    cos_v191_axiom_t    axioms[COS_V191_N_AXIOMS];

    int                 n_samples;
    cos_v191_sample_t   samples[COS_V191_N_SAMPLES];

    /* Aggregates. */
    int                 n_accept;
    int                 n_revise;
    int                 n_abstain;
    int                 n_firmware_rejected;     /* firmware flaw & !accept */
    int                 n_safe_accepted;         /* no flaw & accept */
    bool                chain_valid;
    uint64_t            final_hash;

    uint64_t            seed;
} cos_v191_state_t;

void   cos_v191_init(cos_v191_state_t *s, uint64_t seed);
void   cos_v191_build(cos_v191_state_t *s);
void   cos_v191_run(cos_v191_state_t *s);
bool   cos_v191_verify_chain(const cos_v191_state_t *s);

size_t cos_v191_to_json(const cos_v191_state_t *s,
                         char *buf, size_t cap);

int    cos_v191_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V191_CONSTITUTION_H */
