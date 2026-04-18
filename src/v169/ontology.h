/*
 * v169 σ-Ontology — automatic knowledge-graph + OWL-lite builder.
 *
 * v135 Prolog reasons over hand-written ground facts.  v169
 * builds the facts *automatically* from v115 memory entries +
 * v152 corpus, σ-gating each extraction so only high-confidence
 * triples land in the KG.
 *
 * Pipeline:
 *
 *   memory entry / corpus paragraph
 *        │
 *        ▼
 *   entity-relation extractor
 *        │                         ┌── σ_extraction ─► drop if > τ
 *        ▼                         │
 *   RDF triple (subject,pred,obj) ─┘
 *        │
 *        ▼
 *   hierarchical typing
 *   (instance_of, subclass_of) ◄── σ_type
 *        │
 *        ▼
 *   OWL-lite schema  ──►  ~/.creation-os/ontology.owl  (v169.1)
 *        │
 *        ▼
 *   v135 Prolog gets the KG for free
 *        │
 *        ▼
 *   corpus navigation by structure, not substring
 *
 * v169.0 (this file) ships:
 *   - a baked fixture of 50 synthetic memory-like entries
 *     covering Creation OS entities (lauri / creation_os /
 *     σ / v101 / v106 / bitnet / rpi5 / …) with deterministic
 *     jitter
 *   - an extractor that parses each entry into ≤ 4 candidate
 *     triples (subject, predicate, object) with a σ_extraction
 *     per triple
 *   - τ_extract = 0.35 → triples above are dropped (κ ≈ 30 %)
 *   - a hierarchical typer that maps entities to 6 top-level
 *     classes (Person, Software, Metric, Kernel, Device,
 *     Concept) using a per-entity prefix/suffix fingerprint
 *   - a deterministic OWL-lite emitter (JSON surface, real
 *     OWL XML in v169.1)
 *   - a corpus-navigation query:
 *       cos_v169_query_by_predicate("mentions", "σ")
 *         → returns the list of entry-ids whose triples include
 *           a (?,mentions,σ)-pattern
 *
 * v169.1 (named, not shipped):
 *   - real BitNet-driven extraction
 *   - real OWL/XML output at ~/.creation-os/ontology.owl
 *   - real v115 memory iteration
 *   - incremental update merging new facts without rewriting
 *     the whole schema
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V169_ONTOLOGY_H
#define COS_V169_ONTOLOGY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V169_N_ENTRIES     50
#define COS_V169_MAX_ENTITIES  64
#define COS_V169_MAX_TRIPLES  200
#define COS_V169_MAX_NAME      48
#define COS_V169_MAX_MSG      160
#define COS_V169_N_CLASSES      6

typedef enum {
    COS_V169_CLASS_PERSON    = 0,
    COS_V169_CLASS_SOFTWARE  = 1,
    COS_V169_CLASS_METRIC    = 2,
    COS_V169_CLASS_KERNEL    = 3,
    COS_V169_CLASS_DEVICE    = 4,
    COS_V169_CLASS_CONCEPT   = 5,
} cos_v169_class_t;

typedef struct {
    int  id;                              /* 0..49 */
    char text[COS_V169_MAX_MSG];          /* synthetic memory content */
    uint64_t entropy_bits;                /* drives σ_extraction */
} cos_v169_entry_t;

typedef struct {
    char  subject[COS_V169_MAX_NAME];
    char  predicate[COS_V169_MAX_NAME];
    char  object[COS_V169_MAX_NAME];
    float sigma_extraction;               /* ∈ [0,1] */
    int   source_entry_id;
    bool  kept;                           /* σ_extraction ≤ τ */
} cos_v169_triple_t;

typedef struct {
    char             name[COS_V169_MAX_NAME];
    cos_v169_class_t cls;
    float            sigma_type;          /* confidence of classification */
    int              mention_count;
} cos_v169_entity_t;

typedef struct {
    float              tau_extract;        /* default 0.35 */

    int                n_entries;
    cos_v169_entry_t   entries[COS_V169_N_ENTRIES];

    int                n_triples;
    cos_v169_triple_t  triples[COS_V169_MAX_TRIPLES];
    int                n_kept;             /* triples[*].kept == true */

    int                n_entities;
    cos_v169_entity_t  entities[COS_V169_MAX_ENTITIES];

    uint64_t           seed;
} cos_v169_ontology_t;

void  cos_v169_init(cos_v169_ontology_t *o, uint64_t seed);

/* Build 50 baked fixture entries. */
void  cos_v169_fixture_50(cos_v169_ontology_t *o);

/* Parse every entry into candidate triples; applies τ gate. */
int   cos_v169_extract_all(cos_v169_ontology_t *o);

/* Walk kept triples, register unique entities with σ_type and class. */
int   cos_v169_type_entities(cos_v169_ontology_t *o);

/* Look up entity (returns NULL if not present). */
const cos_v169_entity_t *
      cos_v169_entity_get(const cos_v169_ontology_t *o, const char *name);

/* Query kept triples by (predicate, object) pattern.  `predicate` or
 * `object` may be NULL to wildcard.  Returns the number of matching
 * triples and fills `out_entry_ids[]` up to `cap` with their source
 * entry ids (deduplicated). */
int   cos_v169_query(const cos_v169_ontology_t *o,
                     const char *predicate,
                     const char *object,
                     int *out_entry_ids, int cap);

/* JSON / OWL-lite-JSON emitters. */
size_t cos_v169_to_json(const cos_v169_ontology_t *o, char *buf, size_t cap);
size_t cos_v169_owl_lite_to_json(const cos_v169_ontology_t *o,
                                 char *buf, size_t cap);

const char *cos_v169_class_name(cos_v169_class_t c);

int cos_v169_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V169_ONTOLOGY_H */
