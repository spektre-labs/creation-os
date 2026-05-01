/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v299 σ-Knowledge-Graph — grounded reasoning on a KG.
 *
 *   LLMs hallucinate because they have no anchor.  A
 *   knowledge graph is the anchor; σ measures how
 *   much the anchor holds.  v299 types four v0
 *   manifests for that discipline: retrieval is
 *   σ-gated (answer from the KG when σ is low, fall
 *   back to the LLM + σ-gate only when σ is high),
 *   every triplet carries a provenance σ, multi-hop
 *   chains compound σ through `σ_total = 1 −
 *   Π (1 − σ_hop)`, and the "corpus as KG" manifest
 *   lists the canonical entities the σ discipline
 *   talks about (σ, K_eff, 1=1).
 *
 *   v0 manifests (strict, enumerated):
 *
 *   σ-grounded retrieval (exactly 3 canonical rows):
 *     `known_fact`    σ_retrieval=0.08 FROM_KG;
 *     `partial_match` σ_retrieval=0.35 FROM_KG;
 *     `unknown`       σ_retrieval=0.85 FALLBACK_LLM.
 *     Contract: 3 rows in order; σ_retrieval strictly
 *     monotonically increasing; `FROM_KG iff
 *     σ_retrieval ≤ τ_kg = 0.40` (both branches
 *     fire); exactly 2 FROM_KG AND exactly 1
 *     FALLBACK_LLM — the gate grounds when it can
 *     and abstains to the LLM when it cannot.
 *
 *   Provenance tracking (exactly 3 canonical rows):
 *     `primary_source`   σ_provenance=0.05 trusted
 *                         peer_reviewed=true;
 *     `secondary_source` σ_provenance=0.25 trusted
 *                         peer_reviewed=false;
 *     `rumor_source`     σ_provenance=0.80 REJECTED
 *                         peer_reviewed=false.
 *     Contract: 3 rows in order; σ_provenance
 *     strictly monotonically increasing;
 *     `trusted iff σ_provenance ≤ τ_prov = 0.50`
 *     (both branches fire); every trusted row carries
 *     a non-empty `source_ref` — a KG fact is the
 *     pair (triplet, source), not just the triplet.
 *
 *   Multi-hop σ (exactly 3 canonical chains):
 *     `1_hop` hops=1 σ_per_hop=0.10
 *             σ_total=0.10      warning=false;
 *     `3_hop` hops=3 σ_per_hop=0.15
 *             σ_total≈0.386     warning=false;
 *     `5_hop` hops=5 σ_per_hop=0.20
 *             σ_total≈0.672     warning=true.
 *     Contract: 3 rows in order; `hops` strictly
 *     increasing; `σ_total` strictly increasing;
 *     `σ_total == 1 − (1 − σ_per_hop)^hops` within
 *     1e-3; `warning iff σ_total > τ_warning = 0.50`
 *     (both branches fire) — a long chain is not just
 *     slow, it is noisier.
 *
 *   Corpus as KG (exactly 3 canonical triplets):
 *     `(sigma, IS_SNR_OF, noise_signal_ratio)`;
 *     `(k_eff, DEPENDS_ON, sigma)`;
 *     `(one_equals_one, EXPRESSES, self_consistency)`.
 *     Contract: 3 rows in order; every row
 *     `well_formed = true` (non-empty subject AND
 *     non-empty relation AND non-empty object) AND
 *     `queryable = true` — the σ vocabulary is
 *     already a graph, not free text.
 *
 *   σ_kg (surface hygiene):
 *       σ_kg = 1 −
 *         (ret_rows_ok + ret_order_ok +
 *          ret_gate_polarity_ok +
 *          ret_fromkg_count_ok +
 *          prov_rows_ok + prov_order_ok +
 *          prov_polarity_ok + prov_source_ok +
 *          hop_rows_ok + hop_order_ok +
 *          hop_formula_ok + hop_warning_polarity_ok +
 *          corpus_rows_ok + corpus_wellformed_ok +
 *          corpus_queryable_ok) /
 *         (3 + 1 + 1 + 1 + 3 + 1 + 1 + 1 +
 *          3 + 1 + 1 + 1 + 3 + 1 + 1)
 *   v0 requires `σ_kg == 0.0`.
 *
 *   Contracts (v0):
 *     1. 3 retrieval rows; σ strictly increasing;
 *        FROM_KG iff σ ≤ τ_kg=0.40 (both branches);
 *        exactly 2 FROM_KG AND exactly 1 FALLBACK_LLM.
 *     2. 3 provenance rows; σ strictly increasing;
 *        trusted iff σ ≤ τ_prov=0.50 (both branches);
 *        trusted rows carry a source reference.
 *     3. 3 multi-hop chains; hops strictly increasing;
 *        σ_total follows 1−(1−σ_per_hop)^hops within
 *        1e-3; warning iff σ_total > τ_warn=0.50
 *        (both branches).
 *     4. 3 corpus triplets; all well-formed AND
 *        queryable.
 *     5. σ_kg ∈ [0, 1] AND == 0.0.
 *     6. FNV-1a chain replays byte-identically.
 *
 *   v299.1 (named, not implemented): real AEVS-style
 *     source-linked triplets wired into the v115
 *     σ-memory embedding store, a retrieval planner
 *     that picks the shortest chain whose σ_total
 *     stays below τ_warning, and a corpus ingestion
 *     pipeline that turns the 80-paper σ-corpus into
 *     a queryable KG served under `cos kg query`.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V299_KNOWLEDGE_GRAPH_H
#define COS_V299_KNOWLEDGE_GRAPH_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V299_N_RET     3
#define COS_V299_N_PROV    3
#define COS_V299_N_HOP     3
#define COS_V299_N_CORPUS  3

#define COS_V299_TAU_KG      0.40f
#define COS_V299_TAU_PROV    0.50f
#define COS_V299_TAU_WARNING 0.50f

typedef struct {
    char  query_kind[16];
    float sigma_retrieval;
    char  route[16];
} cos_v299_ret_t;

typedef struct {
    char  source[20];
    float sigma_provenance;
    bool  trusted;
    bool  peer_reviewed;
    char  source_ref[24];
} cos_v299_prov_t;

typedef struct {
    char  label[8];
    int   hops;
    float sigma_per_hop;
    float sigma_total;
    bool  warning;
} cos_v299_hop_t;

typedef struct {
    char  subject [18];
    char  relation[16];
    char  object  [24];
    bool  well_formed;
    bool  queryable;
} cos_v299_triplet_t;

typedef struct {
    cos_v299_ret_t      ret    [COS_V299_N_RET];
    cos_v299_prov_t     prov   [COS_V299_N_PROV];
    cos_v299_hop_t      hop    [COS_V299_N_HOP];
    cos_v299_triplet_t  corpus [COS_V299_N_CORPUS];

    int   n_ret_rows_ok;
    bool  ret_order_ok;
    bool  ret_gate_polarity_ok;
    bool  ret_fromkg_count_ok;

    int   n_prov_rows_ok;
    bool  prov_order_ok;
    bool  prov_polarity_ok;
    bool  prov_source_ok;

    int   n_hop_rows_ok;
    bool  hop_order_ok;
    bool  hop_formula_ok;
    bool  hop_warning_polarity_ok;

    int   n_corpus_rows_ok;
    bool  corpus_wellformed_ok;
    bool  corpus_queryable_ok;

    float sigma_kg;

    bool     chain_valid;
    uint64_t terminal_hash;
    uint64_t seed;
} cos_v299_state_t;

void   cos_v299_init(cos_v299_state_t *s, uint64_t seed);
void   cos_v299_run (cos_v299_state_t *s);

size_t cos_v299_to_json(const cos_v299_state_t *s,
                         char *buf, size_t cap);

int    cos_v299_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V299_KNOWLEDGE_GRAPH_H */
