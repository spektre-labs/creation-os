/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
#include "world_model.h"

#include "knowledge_graph.h"

#include <math.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

int cos_world_query_belief(const char *subject, struct cos_world_belief *beliefs,
    int max, int *n_found)
{
    if (n_found)
        *n_found = 0;
    if (!subject || !beliefs || max <= 0)
        return -1;
    if (cos_kg_init() != 0)
        return -1;
    sqlite3 *db = cos_kg_db_ptr();
    if (!db)
        return -1;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT e1.name,r.predicate,e2.name,r.sigma,r.valid_from "
        "FROM kg_relation r JOIN kg_entity e1 ON e1.id=r.source "
        "JOIN kg_entity e2 ON e2.id=r.target "
        "WHERE e1.name=? COLLATE NOCASE AND r.valid_until=0 "
        "ORDER BY r.sigma ASC LIMIT ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(st, 1, subject, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 2, max);
    int n = 0;
    while (sqlite3_step(st) == SQLITE_ROW && n < max) {
        snprintf(beliefs[n].subject, sizeof beliefs[n].subject, "%s",
            (const char *)sqlite3_column_text(st, 0));
        snprintf(beliefs[n].predicate, sizeof beliefs[n].predicate, "%s",
            (const char *)sqlite3_column_text(st, 1));
        snprintf(beliefs[n].object, sizeof beliefs[n].object, "%s",
            (const char *)sqlite3_column_text(st, 2));
        beliefs[n].sigma          = (float)sqlite3_column_double(st, 3);
        beliefs[n].last_verified = sqlite3_column_int64(st, 4);
        n++;
    }
    sqlite3_finalize(st);
    if (n_found)
        *n_found = n;
    return 0;
}

float cos_world_confidence(const char *subject)
{
    if (!subject || !subject[0])
        return 1.0f;
    if (cos_kg_init() != 0)
        return 1.0f;
    sqlite3 *db = cos_kg_db_ptr();
    if (!db)
        return 1.0f;

    sqlite3_stmt *st = NULL;
    const char *sql =
        "SELECT AVG(r.sigma) FROM kg_relation r JOIN kg_entity e ON e.id=r.source "
        "WHERE e.name=? COLLATE NOCASE AND r.valid_until=0;";
    if (sqlite3_prepare_v2(db, sql, -1, &st, NULL) != SQLITE_OK)
        return 1.0f;
    sqlite3_bind_text(st, 1, subject, -1, SQLITE_TRANSIENT);
    float out = 1.0f;
    if (sqlite3_step(st) == SQLITE_ROW && sqlite3_column_type(st, 0) != SQLITE_NULL)
        out = (float)sqlite3_column_double(st, 0);
    sqlite3_finalize(st);
    if (!isfinite(out))
        out = 1.0f;
    return out;
}

int cos_world_update_belief(const char *subject, const char *predicate,
    const char *object, float sigma)
{
    if (!subject || !predicate || !object || sigma < 0.0f || sigma > 1.0f)
        return -1;
    if (cos_kg_init() != 0)
        return -1;
    uint64_t sid = 0, oid = 0;
    if (cos_kg_ensure_entity(subject, &sid) != 0)
        return -1;
    if (cos_kg_ensure_entity(object, &oid) != 0)
        return -1;
    struct cos_kg_relation r;
    memset(&r, 0, sizeof r);
    r.source_id          = sid;
    r.target_id          = oid;
    snprintf(r.predicate, sizeof r.predicate, "%s", predicate);
    r.sigma              = sigma;
    r.reliability        = 1.0f;
    r.valid_until        = 0;
    r.verification_count = 1;
    return cos_kg_add_relation(&r);
}

#ifdef CREATION_OS_ENABLE_SELF_TESTS
#include <errno.h>

void cos_world_self_test(void)
{
    char tpl[] = "/tmp/cos_wm_test_XXXXXX";
    int fd = mkstemp(tpl);
    if (fd < 0) {
        fprintf(stderr, "world_model self-test: mkstemp failed\n");
        abort();
    }
    close(fd);
    setenv("COS_KG_DB", tpl, 1);
    cos_kg_init();

    if (cos_world_update_belief("Core", "uses", "Sigma", 0.1f) != 0) {
        fprintf(stderr, "world_model: update failed\n");
        abort();
    }
    struct cos_world_belief b[4];
    int n = 0;
    if (cos_world_query_belief("Core", b, 4, &n) != 0 || n != 1
        || strcasecmp(b[0].object, "Sigma") != 0
        || fabsf(b[0].sigma - 0.1f) > 0.02f) {
        fprintf(stderr, "world_model: query belief failed\n");
        abort();
    }
    float cf = cos_world_confidence("Core");
    if (fabsf(cf - 0.1f) > 0.05f) {
        fprintf(stderr, "world_model: confidence avg failed\n");
        abort();
    }
    if (cos_world_update_belief("Core", "depends_on", "Kernel", 0.5f) != 0) {
        fprintf(stderr, "world_model: second update failed\n");
        abort();
    }
    if (cos_world_query_belief("Core", b, 4, &n) != 0 || n != 2) {
        fprintf(stderr, "world_model: multi belief failed\n");
        abort();
    }
    unlink(tpl);
    unsetenv("COS_KG_DB");
}
#endif
