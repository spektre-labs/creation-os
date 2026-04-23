/*
 * Runtime σ-knowledge graph — SQLite + BSC entities and measured relations.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_KNOWLEDGE_GRAPH_H
#define COS_SIGMA_KNOWLEDGE_GRAPH_H

#include <stdint.h>
#include <stdio.h>

struct sqlite3;

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COS_KG_INF_W
#define COS_KG_INF_W 64
#endif

struct cos_kg_entity {
    uint64_t id;
    char     name[128];
    char     type[64];
    uint64_t bsc_vec[COS_KG_INF_W];
    int64_t  first_seen;
    int64_t  last_seen;
    int      mention_count;
};

struct cos_kg_relation {
    uint64_t source_id;
    uint64_t target_id;
    char     predicate[128];
    float    sigma;
    float    reliability;
    int64_t  valid_from;
    int64_t  valid_until;
    int      verification_count;
};

struct cos_kg_query_result {
    struct cos_kg_entity   entities[16];
    struct cos_kg_relation relations[32];
    int                    n_entities;
    int                    n_relations;
    float                  sigma_mean;
};

int cos_kg_init(void);

/** SQLite handle opened by cos_kg_init (NULL before init). */
struct sqlite3 *cos_kg_db_ptr(void);

/** Create or return existing entity id (UTF-8 name). */
int cos_kg_ensure_entity(const char *name, uint64_t *out_id);

/** Combined dialogue turn: extracts capitalized entities + simple SVO/SVP patterns. */
int cos_kg_extract_and_store(const char *text);

int cos_kg_query(const uint64_t *bsc_query, struct cos_kg_query_result *result);

int cos_kg_add_entity(const struct cos_kg_entity *e);

int cos_kg_add_relation(const struct cos_kg_relation *r);

int cos_kg_find_contradictions(struct cos_kg_relation *contradictions, int max,
                               int *n_found);

int cos_kg_stats(int *n_entities, int *n_relations, float *avg_sigma);

/** Emit entire graph as JSON to open FILE (stdout ok). */
int cos_kg_export_json(FILE *fp);

/** Entity-centric dump: relations touching name match (substring). */
int cos_kg_entity_relations(const char *entity_name,
                            struct cos_kg_query_result *out);

int cos_kg_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_KNOWLEDGE_GRAPH_H */
