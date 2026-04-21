/* ULTRA-6 — σ-filtered discrete architecture search (toy genome). */

#ifndef COS_ULTRA_SIGMA_SEARCH_H
#define COS_ULTRA_SIGMA_SEARCH_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int n_layers;
    int n_experts;
    int d_model;
    int n_heads;
    float tau_accept;
    float tau_rethink;
    float ttt_lr;
    int   engram_ttl;
} cos_ultra_nas_genome_t;

float cos_ultra_nas_fitness(const cos_ultra_nas_genome_t *g,
                            float accuracy, float cost, float latency,
                            float sigma_mean);

/* Deterministic evolution stub: writes best into `out_best`. */
void cos_ultra_nas_evolve(int population, int generations,
                          cos_ultra_nas_genome_t *out_best);

int cos_ultra_sigma_search_self_test(void);

#ifdef __cplusplus
}
#endif
#endif
