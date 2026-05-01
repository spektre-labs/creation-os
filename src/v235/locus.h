/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v235 σ-Locus — locus of agency in a distributed mesh.
 *
 *   Where is "I" when Creation OS runs on a mesh (v128)?
 *   v235 refuses the easy answer ("the master").  The
 *   locus of agency is dynamic: for every topic, the
 *   instance with the **lowest σ** speaks.  The rest of
 *   the mesh is still the same "self" — like the two
 *   hemispheres of one mind — but authority migrates.
 *
 *   Fixture (4 mesh nodes × 3 topics = 12 σ-samples):
 *     - per topic, argmin-σ node wins the locus;
 *       ties break on the lower node_id.
 *     - one migration event is seeded in every topic
 *       (pre-snapshot vs post-snapshot σ_before /
 *       σ_after) so locus demonstrably moves.
 *
 *   σ_locus_unity (per-topic):
 *       σ_locus_unity = 1 − mean_dispersion
 *       mean_dispersion = mean( |σ_i − σ_min| ) / 1.0
 *     High unity (close to 1) ⇒ every node agrees; low
 *     unity ⇒ the mesh is split on this topic and the
 *     declared locus matters more.
 *
 *   Anti-split-brain:
 *     Two partitions appear after a simulated network
 *     cut.  Each partition emits an audit chain of
 *     length `chain_len`.  On reconnect v178 Byzantine
 *     consensus declares the partition with the longer
 *     chain as the *true* branch; the other is flagged
 *     fork (v230).  The fixture uses chain lengths 17
 *     and 11 — the 17-length branch wins.
 *
 *   Contracts (v0):
 *     1. n_nodes == 4, n_topics == 3.
 *     2. Per topic: locus_node_id == argmin σ
 *        (tiebreak: lowest node_id).
 *     3. σ ∈ [0, 1] on every sample; σ_locus_unity ∈
 *        [0, 1] per topic.
 *     4. n_migrations ≥ 1 (locus moved at least once
 *        across the fixture's pre→post snapshot).
 *     5. Split-brain: winner partition has strictly
 *        greater `chain_len` than loser; loser is
 *        flagged fork.
 *     6. FNV-1a chain over the whole result replays
 *        byte-identically.
 *
 *   v235.1 (named, not implemented): real v128 mesh
 *     hookup, live audit-chain tracking via v213
 *     trust-chain, and an "agency moved to node-B
 *     because σ(B) < σ(A) on this topic" migration
 *     banner in the server response.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V235_LOCUS_H
#define COS_V235_LOCUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V235_N_NODES   4
#define COS_V235_N_TOPICS  3
#define COS_V235_N_SAMPLES (COS_V235_N_NODES * COS_V235_N_TOPICS)

typedef struct {
    int    topic_id;
    char   topic_name[24];

    /* σ per node (before + after one migration event). */
    float  sigma_before[COS_V235_N_NODES];
    float  sigma_after [COS_V235_N_NODES];

    int    locus_before;         /* argmin node id — before */
    int    locus_after;          /* argmin node id — after  */
    bool   migrated;             /* locus_before != locus_after */

    float  sigma_min_after;      /* lowest σ at t = after */
    float  sigma_locus_unity;    /* 1 − mean dispersion (after) */
} cos_v235_topic_t;

typedef struct {
    int    partition_a_nodes;    /* count in partition A */
    int    partition_b_nodes;    /* count in partition B */
    int    chain_len_a;          /* audit chain length A */
    int    chain_len_b;          /* audit chain length B */
    int    winner_partition;     /* 0 = A, 1 = B */
    bool   loser_flagged_fork;
} cos_v235_split_t;

typedef struct {
    cos_v235_topic_t topics[COS_V235_N_TOPICS];
    cos_v235_split_t split_brain;

    int     n_migrations;
    float   sigma_locus_mean_unity;  /* mean across topics */

    bool    chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v235_state_t;

void   cos_v235_init(cos_v235_state_t *s, uint64_t seed);
void   cos_v235_run (cos_v235_state_t *s);

size_t cos_v235_to_json(const cos_v235_state_t *s,
                         char *buf, size_t cap);

int    cos_v235_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V235_LOCUS_H */
