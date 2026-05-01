/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v232 σ-Lineage — family tree + ancestor query +
 *   σ-gated merge-back.
 *
 *   Creation OS produces generations in two ways:
 *     * v214 swarm-evolve  (temporal generations)
 *     * v230 σ-fork        (spatial copies)
 *   v232 is the audit layer that keeps the whole
 *   genealogy queryable.
 *
 *   Fixture (three generations, six nodes):
 *
 *       node 0 (gen 0)  ROOT (v229 seed)
 *         ├─ node 1 (gen 1)
 *         │    ├─ node 3 (gen 2)
 *         │    └─ node 4 (gen 2)
 *         └─ node 2 (gen 1)
 *              └─ node 5 (gen 2)
 *
 *   Each node carries a 64-bit skill vector.  Parent-
 *   to-child divergence is a deterministic XOR mask
 *   per edge; σ_divergence_from_parent = popcount /
 *   64.  n_skills = popcount(skills); σ_profile =
 *   n_skills / 64.  uptime_steps is a fixture-provided
 *   per-node counter (older nodes have more uptime).
 *
 *   Ancestor query (per node):
 *       ancestor_path[0] = root id
 *       ancestor_path[k] = parent-chain id at depth k
 *       ancestor_depth    = node's own gen
 *   so `ancestor_path[0 .. ancestor_depth]` walks
 *   root → ... → node in order.  This models
 *       `cos lineage --instance fork-3`
 *   without any filesystem / network dependency.
 *
 *   σ-gated merge-back:
 *       σ_merge(node) = σ_divergence_from_parent
 *       mergeable(node) iff  σ_merge ≤ τ_merge  (0.40)
 *   Exactly three nodes merge back cleanly (low
 *   divergence leaves / children) and two nodes are
 *   blocked (intentionally high-divergence fixtures,
 *   ≥ 0.45).  v0 stops at the verdict; v1 will
 *   actually synchronise skill vectors through v129
 *   federated + v201 diplomacy.
 *
 *   Contracts (v0):
 *     1. 6 nodes, max_gen = 2, exactly one root.
 *     2. Every non-root's parent exists and has a
 *        smaller gen.
 *     3. σ_divergence_from_parent ∈ [0, 1] per node.
 *     4. ancestor_path reproduces root→node chain;
 *        ancestor_depth == node.gen.
 *     5. n_mergeable ≥ 1 AND n_blocked ≥ 1 AND
 *        n_mergeable + n_blocked == n_nodes − 1.
 *     6. FNV-1a chain over every node + ancestor
 *        walk replays byte-identically.
 *
 *   v232.1 (named): web UI /lineage, live v129
 *     federated merge with σ_merge driven by real
 *     skill deltas, v201 diplomacy conflict
 *     resolution, v213 trust-chain proofs per
 *     ancestor edge.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V232_LINEAGE_H
#define COS_V232_LINEAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V232_N_NODES   6
#define COS_V232_MAX_GEN   2
#define COS_V232_N_BITS    64

typedef struct {
    int       id;
    int       parent_idx;        /* -1 for root */
    int       gen;
    uint64_t  skills;
    int       n_skills;          /* popcount(skills) */
    float     sigma_profile;     /* n_skills / N_BITS */
    int       uptime_steps;
    float     sigma_divergence_from_parent;
    float     sigma_merge;
    bool      mergeable;
    int       ancestor_path[COS_V232_MAX_GEN + 1];
    int       ancestor_depth;
} cos_v232_node_t;

typedef struct {
    cos_v232_node_t  nodes[COS_V232_N_NODES];
    int              n_mergeable;
    int              n_blocked;
    float            tau_merge;
    bool             chain_valid;
    uint64_t         terminal_hash;
    uint64_t         seed;
} cos_v232_state_t;

void   cos_v232_init(cos_v232_state_t *s, uint64_t seed);
void   cos_v232_run (cos_v232_state_t *s);

size_t cos_v232_to_json(const cos_v232_state_t *s,
                         char *buf, size_t cap);

int    cos_v232_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V232_LINEAGE_H */
