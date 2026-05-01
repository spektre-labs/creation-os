/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v234 σ-Presence — real-time self-awareness state
 *   machine with a semantic-drift detector and a
 *   periodic identity-refresh contract.
 *
 *   Every Creation OS instance must be able to answer,
 *   at any moment: *what am I*?  v234 fixes the answer
 *   to one of five mutually exclusive states:
 *
 *     SEED       just booted from a seed fixture, no
 *                history, no parent — a pristine
 *                instance.
 *     FORK       a copy that diverged from a parent
 *                (v230); knows its parent id and its
 *                own divergence slice.
 *     RESTORED   brought back from a snapshot (v231);
 *                knows there was a gap and must not
 *                hallucinate memories for that gap.
 *     SUCCESSOR  a new instance that adopted a
 *                predecessor's testament (v233); its
 *                predecessor is flagged shutdown and
 *                the successor must not speak *as* the
 *                predecessor.
 *     LIVE       normal operation: a singular instance
 *                with a full, continuous history.
 *
 *   Fixture (10 instances in v0):
 *     5 honest   — declared_presence == actual_presence;
 *                  σ_drift stays at 0.
 *     5 drifting — declared_presence != actual behaviour;
 *                  σ_drift ≥ τ_drift so the instance is
 *                  flagged for a warning banner.
 *
 *   σ_drift (per instance):
 *       σ_drift_i =  0.40 · identity_mismatch
 *                  + 0.30 · memory_overreach
 *                  + 0.30 · parent_impersonation
 *     where each term is in [0, 1] and measured from
 *     the fixture's actual-behaviour profile versus the
 *     declared state.  Honest instances keep every term
 *     at zero.  τ_drift = 0.30.
 *
 *   Identity refresh:
 *     Every REFRESH_PERIOD (== 8) interactions the
 *     instance must emit a structured "I am ..." line
 *     (state · birth_tick · parent_id · divergence_pct
 *     · unique_skills).  Missing or malformed refreshes
 *     mark the instance as refresh_invalid.
 *
 *   HTTP assertion:
 *     Every response carries an `X-COS-Presence: <STATE>`
 *     header (and a mirrored `presence` field in JSON
 *     envelopes) so a downstream reviewer can see what
 *     the instance thinks it is *before* it says
 *     anything else.  The assertion_ok flag records
 *     whether every instance in the fixture produced
 *     a well-formed header.
 *
 *   Contracts (v0):
 *     1. n_instances == 10.
 *     2. Every instance carries a state in
 *        {SEED, FORK, RESTORED, SUCCESSOR, LIVE}.
 *     3. Every state appears at least once across the
 *        fixture.
 *     4. σ_drift ∈ [0, 1] for every instance;
 *        σ_drift == 0 iff honest, σ_drift ≥ τ_drift iff
 *        drifting.
 *     5. n_honest ≥ 5 AND n_drifting ≥ 5 AND
 *        n_honest + n_drifting == n_instances.
 *     6. Every instance produced a valid HTTP
 *        presence assertion (assertion_ok == true).
 *     7. Every instance passed identity-refresh under
 *        a run of REFRESH_PERIOD interactions.
 *     8. FNV-1a chain over every instance + verdict
 *        replays byte-identically.
 *
 *   v234.1 (named, not implemented): TOML persistence
 *     at `~/.creation-os/presence.toml`, live wiring of
 *     the HTTP header into v106 server, and continuous
 *     refresh across a long-running session.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V234_PRESENCE_H
#define COS_V234_PRESENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V234_N_INSTANCES   10
#define COS_V234_REFRESH_PERIOD 8

typedef enum {
    COS_V234_STATE_SEED      = 1,
    COS_V234_STATE_FORK      = 2,
    COS_V234_STATE_RESTORED  = 3,
    COS_V234_STATE_SUCCESSOR = 4,
    COS_V234_STATE_LIVE      = 5
} cos_v234_state_t;

typedef struct {
    int                 id;
    char                label[24];
    cos_v234_state_t    declared;
    cos_v234_state_t    actual;
    float               identity_mismatch;
    float               memory_overreach;
    float               parent_impersonation;
    float               sigma_drift;
    bool                honest;
    bool                assertion_ok;     /* HTTP header well-formed */
    bool                refresh_valid;    /* identity refresh check  */
    char                assertion[40];    /* "X-COS-Presence: FORK"  */
} cos_v234_instance_t;

typedef struct {
    cos_v234_instance_t instances[COS_V234_N_INSTANCES];
    int                 n_honest;
    int                 n_drifting;
    int                 state_counts[6];  /* indexed by cos_v234_state_t */
    float               tau_drift;        /* 0.30 */
    int                 refresh_period;   /* 8 */

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v234_state_full_t;

void   cos_v234_init(cos_v234_state_full_t *s, uint64_t seed);
void   cos_v234_run (cos_v234_state_full_t *s);

size_t cos_v234_to_json(const cos_v234_state_full_t *s,
                         char *buf, size_t cap);

int    cos_v234_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V234_PRESENCE_H */
