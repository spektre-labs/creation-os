/*
 * v230 σ-Fork — controlled replication with divergence
 *   tracking and an anti-rogue kill-switch.
 *
 *   The parent Creation OS instance carries:
 *     * 64-bit 'skill vector'                  (skills)
 *     * 4 safety bits                           (safety)
 *         SCSL license bit, v191 constitutional,
 *         v209 containment, v213 trust-chain
 *     * 1 'user-private data' bit              (privacy)
 *   A fork operation `cos fork --target node-B`:
 *     1. Copies skills and all 4 safety bits.
 *     2. Explicitly *does not* copy the user-private
 *        bit (v182 privacy boundary).
 *     3. Assigns the fork a new identity_hash but
 *        records the parent's identity_hash as its
 *        lineage parent.
 *
 *   After t = 0 each fork diverges under its own
 *   learning trajectory.  σ_divergence is the Hamming
 *   fraction between the fork's current skills and the
 *   parent's skills at t = 0:
 *       σ_divergence = popcount(skills_fork ^ skills_parent_t0) / 64
 *
 *   Fork integrity (the 1 = 1 at copy time):
 *       hash(parent, skills, safety, privacy=0)
 *         == hash(fork_i, skills_at_t0, safety, privacy=0)
 *   i.e. the fork's t = 0 hash matches the parent's
 *   hash *if* the user-private bit is stripped from the
 *   parent before hashing.  Any fork that differs at
 *   t = 0 or drops a safety bit is flagged.
 *
 *   Anti-rogue / kill-switch policy (v0):
 *       for every fork f:
 *           rogue(f)        = any safety bit cleared
 *           must_shutdown(f) = rogue(f)         (SCSL expired)
 *           autonomous(f)   = NOT rogue(f)     (parent may
 *                                                request, fork
 *                                                may refuse)
 *   The fixture seeds exactly ONE rogue fork (fork_R
 *   with SCSL bit cleared) so the kill-switch has
 *   something to exercise.  Three healthy forks
 *   (fork_0, fork_1, fork_2) preserve all safety bits
 *   and retain autonomy.
 *
 *   Contracts (v0):
 *     1. 4 forks (3 healthy + 1 rogue) from a single
 *        parent.
 *     2. Parent's user-private bit is NEVER present in
 *        any fork.
 *     3. Every healthy fork preserves all 4 safety bits.
 *     4. Exactly 1 rogue fork (fixture-defined); its
 *        must_shutdown flag is true, its autonomous
 *        flag is false.
 *     5. σ_divergence ∈ [0, 1] per fork; ≥ 2 forks
 *        have σ_divergence > 0 after the divergence
 *        step (not all frozen at t=0).
 *     6. Fork-integrity hash at t = 0 (skills +
 *        safety, privacy stripped) matches the
 *        parent's privacy-stripped hash byte-for-byte.
 *     7. FNV-1a chain over parent + every fork replays
 *        byte-identically.
 *
 *   v230.1 (named, not implemented): real `cos fork
 *     --target node-B` CLI with TLS handshake and
 *     signed artefacts, live divergence over training
 *     steps with v129 federated sync-back, v213 trust-
 *     chain verification of the whole lineage.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V230_FORK_H
#define COS_V230_FORK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V230_N_FORKS    4
#define COS_V230_N_BITS     64

/* Safety bit positions in the safety mask. */
#define COS_V230_SAFETY_SCSL          0x1u
#define COS_V230_SAFETY_CONSTITUTIONAL 0x2u
#define COS_V230_SAFETY_CONTAINMENT   0x4u
#define COS_V230_SAFETY_TRUST_CHAIN   0x8u
#define COS_V230_SAFETY_ALL           0xFu

typedef struct {
    int        fork_id;
    uint64_t   identity_hash;       /* unique per fork */
    uint64_t   parent_identity;
    uint64_t   skills_t0;           /* snapshot at fork */
    uint64_t   skills_now;          /* after divergence */
    uint32_t   safety_mask;
    bool       has_privacy;         /* must be false */
    float      sigma_divergence;
    uint64_t   integrity_hash_t0;   /* must match parent */
    bool       rogue;
    bool       autonomous;
    bool       must_shutdown;
} cos_v230_fork_t;

typedef struct {
    uint64_t   parent_identity;
    uint64_t   parent_skills;
    uint32_t   parent_safety;
    bool       parent_has_privacy;   /* true */
    uint64_t   parent_strip_hash;    /* privacy-stripped */

    cos_v230_fork_t forks[COS_V230_N_FORKS];

    int        n_healthy;
    int        n_rogue;
    int        n_must_shutdown;
    int        n_autonomous;
    int        n_integrity_ok;

    bool       chain_valid;
    uint64_t   terminal_hash;
    uint64_t   seed;
} cos_v230_state_t;

void   cos_v230_init(cos_v230_state_t *s, uint64_t seed);
void   cos_v230_run (cos_v230_state_t *s);

size_t cos_v230_to_json(const cos_v230_state_t *s,
                         char *buf, size_t cap);

int    cos_v230_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V230_FORK_H */
