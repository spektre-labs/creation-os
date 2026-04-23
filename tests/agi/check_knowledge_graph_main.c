/* Self-test driver for σ-knowledge graph (≥ 8 checks). */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "inference_cache.h"
#include "knowledge_graph.h"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static char g_tmp_kg[256];

static void setup_tmp_db(void)
{
    static int seq;
    snprintf(g_tmp_kg, sizeof g_tmp_kg, "/tmp/cos_kg_chk_%ld_%d.db",
        (long)getpid(), seq++);
    remove(g_tmp_kg);
    setenv("COS_KG_DB", g_tmp_kg, 1);
}

static void teardown_tmp_db(void)
{
    remove(g_tmp_kg);
    unsetenv("COS_KG_DB");
}

static int check_ensure_stats(void)
{
    setup_tmp_db();
    if (cos_kg_init() != 0)
        return 1;
    uint64_t a = 0, b = 0;
    if (cos_kg_ensure_entity("NorthPole", &a) != 0
        || cos_kg_ensure_entity("SouthPole", &b) != 0)
        return 2;
    int ne = 0, nr = 0;
    float av = 0.f;
    if (cos_kg_stats(&ne, &nr, &av) != 0 || ne < 2)
        return 3;
    teardown_tmp_db();
    return 0;
}

static int check_extract_query(void)
{
    setup_tmp_db();
    if (cos_kg_init() != 0)
        return 1;
    const char *txt = "Alice requires Bob. Alice causes Change.";
    if (cos_kg_extract_and_store(txt) != 0)
        return 2;
    uint64_t qb[COS_KG_INF_W];
    cos_inference_bsc_encode_prompt("Alice Bob relation", qb);
    struct cos_kg_query_result qr;
    memset(&qr, 0, sizeof qr);
    if (cos_kg_query(qb, &qr) != 0)
        return 3;
    teardown_tmp_db();
    return 0;
}

static int check_entity_relations_export(void)
{
    setup_tmp_db();
    if (cos_kg_init() != 0)
        return 1;
    if (cos_kg_extract_and_store("Zeta is Omega.") != 0)
        return 2;
    struct cos_kg_query_result qr;
    memset(&qr, 0, sizeof qr);
    if (cos_kg_entity_relations("Zeta", &qr) != 0)
        return 3;
    FILE *fp = fopen("/dev/null", "w");
    if (!fp || cos_kg_export_json(fp) != 0) {
        if (fp)
            fclose(fp);
        return 4;
    }
    fclose(fp);
    teardown_tmp_db();
    return 0;
}

static int check_contradictions_api(void)
{
    setup_tmp_db();
    if (cos_kg_init() != 0)
        return 1;
    struct cos_kg_relation cx[4];
    int nc = -1;
    if (cos_kg_find_contradictions(cx, 4, &nc) != 0 || nc < 0)
        return 2;
    teardown_tmp_db();
    return 0;
}

static int check_add_relation_path(void)
{
    setup_tmp_db();
    if (cos_kg_init() != 0)
        return 1;
    uint64_t s = 0, t = 0;
    if (cos_kg_ensure_entity("SrcEnt", &s) != 0
        || cos_kg_ensure_entity("TgtEnt", &t) != 0)
        return 2;
    struct cos_kg_relation r;
    memset(&r, 0, sizeof r);
    r.source_id = s;
    r.target_id = t;
    snprintf(r.predicate, sizeof r.predicate, "%s", "tests");
    r.sigma = 0.25f;
    if (cos_kg_add_relation(&r) != 0)
        return 3;
    int ne = 0, nr = 0;
    float av = 0.f;
    if (cos_kg_stats(&ne, &nr, &av) != 0 || nr < 1)
        return 4;
    teardown_tmp_db();
    return 0;
}

int main(void)
{
    if (cos_kg_self_test() != 0) {
        fputs("check-knowledge-graph: cos_kg_self_test failed\n", stderr);
        return 1;
    }
    int e;
    e = check_ensure_stats();
    if (e) {
        fprintf(stderr, "check-knowledge-graph: ensure/stats failed (%d)\n", e);
        return 1;
    }
    e = check_extract_query();
    if (e) {
        fprintf(stderr, "check-knowledge-graph: extract/query failed (%d)\n", e);
        return 1;
    }
    e = check_entity_relations_export();
    if (e) {
        fprintf(stderr, "check-knowledge-graph: entity/export failed (%d)\n", e);
        return 1;
    }
    e = check_contradictions_api();
    if (e) {
        fprintf(stderr, "check-knowledge-graph: contradictions failed (%d)\n", e);
        return 1;
    }
    e = check_add_relation_path();
    if (e) {
        fprintf(stderr, "check-knowledge-graph: add_relation failed (%d)\n", e);
        return 1;
    }
    /* Checks 7–8: extraction creates entities + stats avg path */
    setup_tmp_db();
    if (cos_kg_init() != 0 || cos_kg_extract_and_store("Alpha causes Beta. Gamma requires Delta.") != 0) {
        fputs("check-knowledge-graph: multi-token extract failed\n", stderr);
        return 1;
    }
    int ne = 0, nr = 0;
    float av = 0.f;
    if (cos_kg_stats(&ne, &nr, &av) != 0 || ne < 1) {
        fputs("check-knowledge-graph: stats after extract failed\n", stderr);
        return 1;
    }
    teardown_tmp_db();

    return 0;
}
