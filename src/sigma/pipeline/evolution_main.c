/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Evolution self-test + canonical 8-generation demo. */

#include "evolution.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static float bench_demo(const cos_evo_genome_t *g, void *ctx,
                        cos_evo_benchmark_t *out) {
    (void)ctx;
    float acc = 1.0f - fabsf(g->tau_accept  - 0.80f)
                     - 0.5f * fabsf(g->tau_rethink - 0.40f);
    if (acc < 0.0f) acc = 0.0f;
    if (acc > 1.0f) acc = 1.0f;
    float cost = 0.001f + 0.001f * (float)g->max_rethink;
    float lat  = 0.1f   + 0.0001f * g->engram_ttl;
    float sig  = fabsf(g->ttt_lr - 0.05f) + 0.1f * fabsf(g->codex_weight - 1.0f);
    if (sig < 0.0f) sig = 0.0f;
    if (sig > 1.0f) sig = 1.0f;
    out->accuracy    = acc;
    out->cost_eur    = cost;
    out->latency_s   = lat;
    out->sigma_bench = sig;
    return cos_sigma_evo_fitness(out);
}

int main(int argc, char **argv) {
    int rc = cos_sigma_evo_self_test();

    /* Canonical demo: 6-member population, 8 generations,
     * seed = a deliberately-bad genome.  Deterministic RNG 42. */
    cos_evo_genome_t seed = {
        .tau_accept   = 0.50f,
        .tau_rethink  = 0.80f,
        .ttt_lr       = 0.20f,
        .max_rethink  = 8,
        .codex_weight = 0.50f,
        .engram_ttl   = 1000.0f,
    };
    cos_evo_individual_t pop[6];
    cos_evo_state_t st;
    cos_sigma_evo_init(&st, pop, 6, &seed, 42ULL, 0.05f);

    /* Seed fitness for comparison. */
    cos_evo_benchmark_t b_seed;
    bench_demo(&seed, NULL, &b_seed);
    float f_seed = cos_sigma_evo_fitness(&b_seed);

    cos_evo_genome_t best;
    cos_sigma_evo_run(&st, 8, bench_demo, NULL, &best);

    float f_best = st.best_fitness;
    int improved = (f_best > f_seed) ? 1 : 0;

    printf("{\"kernel\":\"sigma_evolution\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"pop_size\":%d,\"generations\":%d,\"rng_seed\":42,"
             "\"seed\":{"
               "\"tau_accept\":%.2f,\"tau_rethink\":%.2f,"
               "\"ttt_lr\":%.2f,\"max_rethink\":%d,"
               "\"codex_weight\":%.2f,\"engram_ttl\":%.0f},"
             "\"seed_fitness\":%.4f,"
             "\"best\":{"
               "\"tau_accept\":%.4f,\"tau_rethink\":%.4f,"
               "\"ttt_lr\":%.4f,\"max_rethink\":%d,"
               "\"codex_weight\":%.4f,\"engram_ttl\":%.2f},"
             "\"best_fitness\":%.4f,\"best_sigma_bench\":%.4f,"
             "\"improved\":%s,"
             "\"tau_accept_near_optimum\":%s"
             "},"
           "\"pass\":%s}\n",
           rc,
           st.pop_size, st.generation,
           (double)seed.tau_accept, (double)seed.tau_rethink,
           (double)seed.ttt_lr, seed.max_rethink,
           (double)seed.codex_weight, (double)seed.engram_ttl,
           (double)f_seed,
           (double)best.tau_accept, (double)best.tau_rethink,
           (double)best.ttt_lr, best.max_rethink,
           (double)best.codex_weight, (double)best.engram_ttl,
           (double)f_best, (double)st.best_sigma_bench,
           improved ? "true" : "false",
           (fabsf(best.tau_accept - 0.80f) < 0.30f) ? "true" : "false",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Evolution demo:\n");
        fprintf(stderr, "  seed  fitness=%.3f\n", (double)f_seed);
        fprintf(stderr, "  best  fitness=%.3f τ_accept=%.3f σ_bench=%.3f\n",
                (double)f_best, (double)best.tau_accept,
                (double)st.best_sigma_bench);
    }
    return rc;
}
