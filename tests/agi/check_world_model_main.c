/* Self-test driver for σ-world_model (≥ 4 checks). */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "knowledge_graph.h"
#include "world_model.h"

static int chk_query(void)
{
    struct cos_world_belief b[8];
    int n = 0;
    if (cos_world_query_belief("Core", b, 8, &n) != 0 || n != 2)
        return 1;
    return 0;
}

static int chk_confidence(void)
{
    float c = cos_world_confidence("Core");
    if (fabsf(c - 0.3f) > 0.15f)
        return 1;
    return 0;
}

static int chk_validation(void)
{
    if (cos_world_update_belief("", "p", "o", 0.5f) == 0)
        return 1;
    if (cos_world_confidence("") != 1.0f)
        return 2;
    if (cos_world_update_belief("X", "y", "z", 1.5f) == 0)
        return 3;
    return 0;
}

static int chk_missing_subject_query(void)
{
    struct cos_world_belief b[4];
    int n = -1;
    if (cos_world_query_belief(NULL, b, 4, &n) != -1)
        return 1;
    return 0;
}

int main(void)
{
    char tpl[] = "/tmp/cos_wm_chk_XXXXXX";
    int fd = mkstemp(tpl);
    if (fd < 0) {
        perror("mkstemp");
        return 1;
    }
    close(fd);
    setenv("COS_KG_DB", tpl, 1);
    if (cos_kg_init() != 0) {
        fputs("check-world-model: kg init failed\n", stderr);
        return 1;
    }

    if (cos_world_update_belief("Core", "uses", "Sigma", 0.1f) != 0
        || cos_world_update_belief("Core", "depends_on", "Kernel", 0.5f) != 0) {
        fputs("check-world-model: seed updates failed\n", stderr);
        return 1;
    }
    if (chk_query() != 0) {
        fputs("check-world-model: query count failed\n", stderr);
        return 1;
    }
    if (chk_confidence() != 0) {
        fputs("check-world-model: confidence avg failed\n", stderr);
        return 1;
    }
    if (chk_validation() != 0) {
        fputs("check-world-model: validation failed\n", stderr);
        return 1;
    }
    if (chk_missing_subject_query() != 0) {
        fputs("check-world-model: null guard failed\n", stderr);
        return 1;
    }

    unlink(tpl);
    unsetenv("COS_KG_DB");

    return 0;
}
