/* ULTRA-6 — σ-guided architecture search (deterministic toy search). */

#include "sigma_search.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

float cos_ultra_nas_fitness(const cos_ultra_nas_genome_t *g,
                            float accuracy, float cost, float latency,
                            float sigma_mean) {
    if (!g) return 0.0f;
    if (cost <= 0.0f) cost = 1e-6f;
    if (latency <= 0.0f) latency = 1e-6f;
    float denom = cost * latency;
    if (!(denom > 0.0f)) denom = 1e-6f;
    float acc = accuracy;
    if (acc < 0.0f) acc = 0.0f;
    if (acc > 1.0f) acc = 1.0f;
    float sig = sigma_mean;
    if (sig != sig) sig = 1.0f;
    if (sig < 0.0f) sig = 0.0f;
    if (sig > 1.0f) sig = 1.0f;
    return (acc * acc) / denom * (1.0f - sig);
}

static uint32_t nas_lcg(uint32_t *s) {
    *s = *s * 1103515245u + 12345u;
    return *s;
}

void cos_ultra_nas_evolve(int population, int generations,
                          cos_ultra_nas_genome_t *out_best) {
    if (!out_best) return;
    if (population < 1) population = 1;
    if (generations < 1) generations = 1;
    if (population > 64) population = 64;
    if (generations > 256) generations = 256;
    uint32_t seed = 1u;
    cos_ultra_nas_genome_t best = {
        .n_layers = 8, .n_experts = 8, .d_model = 2048, .n_heads = 16,
        .tau_accept = 0.35f, .tau_rethink = 0.55f, .ttt_lr = 3e-4f,
        .engram_ttl = 200,
    };
    float best_fit = -1e30f;
    for (int gen = 0; gen < generations; ++gen) {
        for (int p = 0; p < population; ++p) {
            cos_ultra_nas_genome_t g = best;
            uint32_t r = nas_lcg(&seed);
            g.n_layers  = 4 + (int)(r % 9u);         /* 4..12 */
            r = nas_lcg(&seed);
            g.n_experts = 4 + (int)(r % 13u);       /* 4..16 */
            r = nas_lcg(&seed);
            g.d_model   = 1024 + (int)(r % 5u) * 256;
            r = nas_lcg(&seed);
            g.n_heads   = 8 + (int)(r % 9u) * 2;
            r = nas_lcg(&seed);
            g.tau_accept = 0.25f + (r % 100u) / 200.0f;
            if (g.tau_accept > 0.90f) g.tau_accept = 0.90f;
            r = nas_lcg(&seed);
            g.tau_rethink = g.tau_accept + 0.05f + (r % 80u) / 400.0f;
            if (g.tau_rethink > 0.98f) g.tau_rethink = 0.98f;
            r = nas_lcg(&seed);
            g.ttt_lr = 1e-4f + (r % 1000u) * 1e-6f;
            r = nas_lcg(&seed);
            g.engram_ttl = 50 + (int)(r % 400u);
            float acc = 0.55f + (nas_lcg(&seed) % 200u) / 1000.0f;
            float cost = 0.5f + (nas_lcg(&seed) % 100u) / 200.0f;
            float lat  = 0.4f + (nas_lcg(&seed) % 120u) / 200.0f;
            float sig  = 0.25f + (nas_lcg(&seed) % 200u) / 1000.0f;
            /* σ gate: reject clearly regressed mutants */
            if (sig > 0.92f) continue;
            float f = cos_ultra_nas_fitness(&g, acc, cost, lat, sig);
            if (f > best_fit) {
                best_fit = f;
                best = g;
            }
        }
    }
    *out_best = best;
}

int cos_ultra_sigma_search_self_test(void) {
    cos_ultra_nas_genome_t g = { .n_layers = 12, .n_experts = 8,
        .d_model = 4096, .n_heads = 32, .tau_accept = 0.4f,
        .tau_rethink = 0.6f, .ttt_lr = 1e-3f, .engram_ttl = 120 };
    float f = cos_ultra_nas_fitness(&g, 0.8f, 1.0f, 1.0f, 0.2f);
    if (!(f > 0.4f)) return 1;
    cos_ultra_nas_genome_t out;
    cos_ultra_nas_evolve(20, 5, &out);
    if (out.n_layers < 4 || out.n_layers > 32) return 2;
    return 0;
}
