/*
 * cos graph — inspect and query the runtime σ-knowledge graph (SQLite).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "cos_graph.h"

#include "inference_cache.h"
#include "knowledge_graph.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void print_query_result(const struct cos_kg_query_result *qr)
{
    printf("entities (%d):\n", qr->n_entities);
    for (int i = 0; i < qr->n_entities; i++)
        printf("  [%llu] %s (%s)\n",
            (unsigned long long)qr->entities[i].id,
            qr->entities[i].name, qr->entities[i].type);
    printf("relations (%d), mean σ=%.4f:\n", qr->n_relations,
        (double)qr->sigma_mean);
    for (int i = 0; i < qr->n_relations; i++) {
        printf("  %llu --[%s]--> %llu  σ=%.4f\n",
            (unsigned long long)qr->relations[i].source_id,
            qr->relations[i].predicate,
            (unsigned long long)qr->relations[i].target_id,
            (double)qr->relations[i].sigma);
    }
}

static void print_entity_bundle(const struct cos_kg_query_result *qr)
{
    print_query_result(qr);
}

int cos_graph_main(int argc, char **argv)
{
    if (argc >= 1 && strcmp(argv[0], "--help") == 0) {
        fputs(
            "cos graph — σ-knowledge graph\n"
            "  (default)           entity/relation counts + avg σ\n"
            "  --query TEXT        BSC-neighbour query + local relations\n"
            "  --contradictions    list active contradiction-flagged edges\n"
            "  --entity NAME       relations touching entity name\n"
            "  --export            JSON dump to stdout\n"
            "  COS_KG_DB           override SQLite path (~/.cos/knowledge_graph.db)\n",
            stdout);
        return 0;
    }

    if (cos_kg_init() != 0) {
        fprintf(stderr, "cos graph: database open failed (COS_KG_DB or ~/.cos)\n");
        return 1;
    }

    if (argc >= 1 && strcmp(argv[0], "--export") == 0)
        return cos_kg_export_json(stdout) != 0 ? 2 : 0;

    if (argc >= 2 && strcmp(argv[0], "--query") == 0) {
        uint64_t qb[COS_KG_INF_W];
        cos_inference_bsc_encode_prompt(argv[1], qb);
        struct cos_kg_query_result qr;
        memset(&qr, 0, sizeof qr);
        if (cos_kg_query(qb, &qr) != 0) {
            fprintf(stderr, "cos graph: query failed\n");
            return 2;
        }
        print_query_result(&qr);
        return 0;
    }

    if (argc >= 1 && strcmp(argv[0], "--contradictions") == 0) {
        struct cos_kg_relation cx[32];
        int nc = 0;
        if (cos_kg_find_contradictions(cx, 32, &nc) != 0) {
            fprintf(stderr, "cos graph: contradiction scan failed\n");
            return 2;
        }
        printf("contradictions (flagged rows): %d\n", nc);
        for (int i = 0; i < nc; i++)
            printf("  %llu --[%s]--> %llu  σ=%.4f\n",
                (unsigned long long)cx[i].source_id,
                cx[i].predicate,
                (unsigned long long)cx[i].target_id,
                (double)cx[i].sigma);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[0], "--entity") == 0) {
        struct cos_kg_query_result qr;
        memset(&qr, 0, sizeof qr);
        if (cos_kg_entity_relations(argv[1], &qr) != 0) {
            fprintf(stderr, "cos graph: entity lookup failed\n");
            return 2;
        }
        print_entity_bundle(&qr);
        return 0;
    }

    int ne = 0, nr = 0;
    float av = 0.f;
    if (cos_kg_stats(&ne, &nr, &av) != 0) {
        fprintf(stderr, "cos graph: stats failed\n");
        return 2;
    }
    printf("σ-knowledge graph\n  entities: %d\n  relations: %d\n  avg σ: %.4f\n",
        ne, nr, (double)av);
    return 0;
}

#ifdef COS_GRAPH_STANDALONE_MAIN
int main(int argc, char **argv)
{
    return cos_graph_main(argc - 1, argv + 1);
}
#endif
