/*
 * Codebase evolution — generation loop over σ-codegen proposals.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "evolution_engine.h"

#include "codegen.h"
#include "state_ledger.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static void evolve_home(char *buf, size_t cap)
{
    const char *h = getenv("HOME");
    if (!h || !h[0])
        h = "/tmp";
    snprintf(buf, cap, "%s/.cos/evolve_code", h);
}

static int evolve_mkhome(void)
{
    char b[512];
    evolve_home(b, sizeof b);
    return mkdir(b, 0700) == 0 || errno == EEXIST ? 0 : -1;
}

static float fitness_sigma(void)
{
    struct cos_state_ledger L;
    char                   path[768];

    cos_state_ledger_init(&L);
    if (cos_state_ledger_default_path(path, sizeof path) != 0)
        return 0.55f;
    if (cos_state_ledger_load(&L, path) != 0)
        return 0.55f;
    if (L.sigma_mean_session < 0.f || L.sigma_mean_session > 1.f)
        return 0.55f;
    return L.sigma_mean_session;
}

static const char *weakest_module_tag(void)
{
    const char *m = getenv("COS_EVOLUTION_WEAK_MODULE");
    if (m && m[0])
        return m;
    return "ledger_sigma_mean";
}

int cos_evolution_run(int                              n_generations,
                      struct cos_evolution_generation *results)
{
    int    g;
    char   goal[2048];

    if (results == NULL || n_generations <= 0)
        return -1;

    for (g = 0; g < n_generations; ++g) {
        struct cos_codegen_proposal *P;

        memset(&results[g], 0, sizeof results[g]);
        results[g].gen_number = g + 1;

        results[g].fitness_before = fitness_sigma();

        snprintf(goal, sizeof goal,
                 "Improve Creation OS module [%s]: reduce σ_mean; emit "
                 "cos_codegen_candidate_self_test only (pure C11, libc).",
                 weakest_module_tag());

        if (getenv("COS_EVOLUTION_OFFLINE") != NULL
            && getenv("COS_EVOLUTION_OFFLINE")[0] == '1') {
            if (setenv("COS_CODEGEN_OFFLINE", "1", 1) != 0)
                return -2;
        }

        P = &results[g].proposals[0];
        if (cos_codegen_propose(goal, P) != 0) {
            results[g].rejected_count++;
            results[g].fitness_after = results[g].fitness_before;
            results[g].n_proposals   = 0;
            (void)cos_evolution_append_history(&results[g]);
            continue;
        }

        if (cos_codegen_compile_sandbox(P) != 0) {
            results[g].rejected_count++;
            snprintf(P->rejection_reason, sizeof P->rejection_reason,
                     "compile_failed");
            goto gen_done;
        }

        if (cos_codegen_test_sandbox(P) != 0)
            results[g].rejected_count++;

        (void)cos_codegen_frama_check(P);
        (void)cos_codegen_check_regression(P);

        if (cos_codegen_gate(P) != 0) {
            results[g].rejected_count++;
        } else if (cos_codegen_register(P) == 0) {
            results[g].accepted_count++;
            results[g].proposals[0] = *P;
        } else {
            results[g].rejected_count++;
        }

    gen_done:
        results[g].fitness_after =
            fitness_sigma() * (1.0f - 0.05f * (float)results[g].accepted_count);
        if (results[g].fitness_after < 0.f)
            results[g].fitness_after = 0.f;
        results[g].n_proposals = 1;

        (void)cos_evolution_append_history(&results[g]);
    }

    return 0;
}

void cos_evolution_print_report(const struct cos_evolution_generation *results,
                                int                                  n_gen)
{
    int g;

    if (!results || n_gen <= 0)
        return;

    printf("cos evolution (code): %d generation(s)\n", n_gen);
    for (g = 0; g < n_gen; ++g) {
        const struct cos_evolution_generation *r = &results[g];
        printf(
            "  gen %d  fitness %.4f → %.4f  accepted=%d rejected=%d  id=%d\n",
            r->gen_number, (double)r->fitness_before, (double)r->fitness_after,
            r->accepted_count, r->rejected_count,
            r->n_proposals > 0 ? r->proposals[0].proposal_id : 0);
    }
}

int cos_evolution_append_history(const struct cos_evolution_generation *g)
{
    char  path[640];
    FILE *f;

    if (g == NULL)
        return -1;
    evolve_mkhome();
    evolve_home(path, sizeof path);
    snprintf(path + strlen(path), sizeof path - strlen(path), "/history.jsonl");

    f = fopen(path, "a");
    if (!f)
        return -2;
    fprintf(f,
            "{\"gen\":%d,\"fit_before\":%.6f,\"fit_after\":%.6f,"
            "\"acc\":%d,\"rej\":%d,\"prop\":%d}\n",
            g->gen_number, (double)g->fitness_before, (double)g->fitness_after,
            g->accepted_count, g->rejected_count,
            g->n_proposals > 0 ? g->proposals[0].proposal_id : 0);
    fclose(f);
    return 0;
}

#if CREATION_OS_ENABLE_SELF_TESTS
static int evo_fail(const char *m)
{
    fprintf(stderr, "evolution_engine self-test: %s\n", m);
    return 1;
}
#endif

int cos_evolution_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_evolution_generation one[1];

    if (setenv("COS_CODEGEN_OFFLINE", "1", 1) != 0)
        return evo_fail("setenv");
    if (setenv("COS_EVOLUTION_OFFLINE", "1", 1) != 0)
        return evo_fail("setenv2");

    if (cos_evolution_run(1, one) != 0)
        return evo_fail("run");

    cos_evolution_print_report(one, 1);

    fprintf(stderr, "evolution_engine self-test: OK\n");
#endif
    return 0;
}
