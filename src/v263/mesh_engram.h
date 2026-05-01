/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v263 σ-Mesh-Engram — distributed O(1) hash table.
 *
 *   v260's Engram is one box.  v263 shards the hash
 *   table over a mesh so more nodes = more memory,
 *   adds σ-consistent replication for critical facts,
 *   types the memory hierarchy (L1 SQLite → L2 Engram
 *   → L3 mesh-Engram → L4 API), and wires σ-driven
 *   forgetting.
 *
 *   v0 manifests (strict, enumerated):
 *
 *   Mesh nodes (exactly 3, canonical order A, B, C):
 *     Each row: `node_id`, `shard_lo`, `shard_hi`
 *     (uint8 range boundary of a hash's first byte,
 *     inclusive low / exclusive high except C which
 *     wraps to 0x100 so union is exactly [0, 256)).
 *     Shards are non-overlapping AND contiguous AND
 *     cover [0, 256) exactly once.  v0 is not a
 *     deployment topology; it is a typed, verifiable
 *     manifest.
 *
 *   Lookup fixtures (exactly 4, canonical order):
 *     Each row: `entity`, `hash8` (= hash of entity
 *     truncated to 1 byte), `expected_node` (the node
 *     whose shard contains `hash8`), `served_by`,
 *     `lookup_ns ≤ 100` (O(1) on the right node).
 *     `served_by` MUST equal `expected_node`.  Across
 *     the 4 fixtures, EVERY node in {A, B, C} appears
 *     at least once as `served_by` so routing has teeth.
 *
 *   σ-consistent replication (exactly 4 rows):
 *     Critical facts replicated to 3 nodes.  Each row:
 *     `fact`, `replicas` (exactly 3), `sigma_replication
 *     ∈ [0, 1]` (quorum drift), `quorum_ok` (==
 *     `sigma_replication ≤ τ_quorum = 0.25`).  At least
 *     one row has `quorum_ok == false` AND at least one
 *     row has `quorum_ok == true` (both branches fire).
 *
 *   Memory hierarchy (exactly 4 tiers, canonical order):
 *     L1: local_sqlite     —    10 ns,      10 MB
 *     L2: engram_dram      —    80 ns,   8 192 MB
 *     L3: mesh_engram      — 2 000 ns,  65 536 MB
 *     L4: api_search       — 40 000 000 ns (40 ms), ∞
 *   Contracts:  latency strictly ascending AND capacity
 *   strictly ascending across L1→L4.  (Capacity here is
 *   typed as MB; L4 is encoded as UINT32_MAX to mean
 *   "unbounded".)
 *
 *   Forgetting with σ (exactly 4 rows):
 *     Each row: `item`, `sigma_relevance ∈ [0, 1]`,
 *     `action ∈ {KEEP_L1, MOVE_L2, MOVE_L3, DROP}`.
 *     Rule:
 *       σ_relevance ≤ 0.20 → KEEP_L1   (hot)
 *       0.20 <  σ  ≤ 0.50 → MOVE_L2   (warm)
 *       0.50 <  σ  ≤ 0.80 → MOVE_L3   (cold)
 *       σ_relevance >  0.80 → DROP      (forget)
 *     Every rule branch fires at least once.
 *
 *   σ_mesh_engram (surface hygiene):
 *       σ_mesh_engram = 1 −
 *         (shards_ok + lookups_ok + nodes_covered_ok +
 *          replication_rows_ok + both_quorum_branches_ok +
 *          hierarchy_ok + forgetting_rows_ok +
 *          forgetting_branches_ok) /
 *         (1 + 4 + 1 + 4 + 1 + 1 + 4 + 1)
 *   v0 requires `σ_mesh_engram == 0.0`.
 *
 *   Contracts (v0):
 *     1. Exactly 3 mesh nodes with contiguous,
 *        non-overlapping shards covering [0, 256).
 *     2. Exactly 4 lookup fixtures where `served_by ==
 *        expected_node`; nodes {A, B, C} each appear ≥ 1×.
 *     3. Exactly 4 replication rows, `replicas == 3`;
 *        ≥ 1 `quorum_ok==true` AND ≥ 1 `==false`.
 *     4. Exactly 4 hierarchy tiers in canonical order,
 *        latency_ns strictly ascending, capacity_mb
 *        strictly ascending (L4 encoded as UINT32_MAX).
 *     5. Exactly 4 forgetting rows, action matches σ
 *        rule byte-for-byte; every branch fires ≥ 1×.
 *     6. σ_mesh_engram ∈ [0, 1] AND == 0.0.
 *     7. FNV-1a chain replays byte-identically.
 *
 *   v263.1 (named, not implemented): live gossip on
 *     v128 mesh, live quorum via v216, live σ-driven
 *     eviction wired to v115 / v242 storage backends.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V263_MESH_ENGRAM_H
#define COS_V263_MESH_ENGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V263_N_NODES       3
#define COS_V263_N_LOOKUPS     4
#define COS_V263_N_REPLICATION 4
#define COS_V263_N_TIERS       4
#define COS_V263_N_FORGET      4

typedef enum {
    COS_V263_ACT_KEEP_L1 = 1,
    COS_V263_ACT_MOVE_L2 = 2,
    COS_V263_ACT_MOVE_L3 = 3,
    COS_V263_ACT_DROP    = 4
} cos_v263_action_t;

typedef struct {
    char     node_id[4];
    uint32_t shard_lo;   /* inclusive */
    uint32_t shard_hi;   /* exclusive; last node ends at 256 */
} cos_v263_node_t;

typedef struct {
    char     entity[24];
    uint8_t  hash8;
    char     expected_node[4];
    char     served_by    [4];
    int      lookup_ns;
} cos_v263_lookup_t;

typedef struct {
    char  fact[24];
    int   replicas;
    float sigma_replication;
    bool  quorum_ok;
} cos_v263_replication_t;

typedef struct {
    char      tier[12];      /* L1 / L2 / L3 / L4 */
    char      backend[16];
    uint64_t  latency_ns;
    uint32_t  capacity_mb;   /* UINT32_MAX = unbounded */
} cos_v263_tier_t;

typedef struct {
    char              item[20];
    float             sigma_relevance;
    cos_v263_action_t action;
} cos_v263_forget_t;

typedef struct {
    cos_v263_node_t         nodes        [COS_V263_N_NODES];
    cos_v263_lookup_t       lookups      [COS_V263_N_LOOKUPS];
    cos_v263_replication_t  replication  [COS_V263_N_REPLICATION];
    cos_v263_tier_t         hierarchy    [COS_V263_N_TIERS];
    cos_v263_forget_t       forgetting   [COS_V263_N_FORGET];

    float tau_quorum;
    int   lookup_ns_budget;

    bool  shards_ok;
    int   n_lookups_ok;
    int   n_nodes_covered;
    int   n_replication_ok;
    int   n_quorum_true;
    int   n_quorum_false;
    bool  hierarchy_ok;
    int   n_forget_ok;
    int   n_keep;
    int   n_l2;
    int   n_l3;
    int   n_drop;

    float sigma_mesh_engram;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v263_state_t;

void   cos_v263_init(cos_v263_state_t *s, uint64_t seed);
void   cos_v263_run (cos_v263_state_t *s);

size_t cos_v263_to_json(const cos_v263_state_t *s,
                         char *buf, size_t cap);

int    cos_v263_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V263_MESH_ENGRAM_H */
