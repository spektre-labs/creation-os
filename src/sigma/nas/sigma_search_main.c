#include "sigma_search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0)
            return cos_ultra_sigma_search_self_test();
    }
    int generations = 50;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--generations") == 0 && i + 1 < argc)
            generations = atoi(argv[++i]);
    }
    cos_ultra_nas_genome_t best;
    cos_ultra_nas_evolve(20, generations, &best);
    printf("ULTRA-6 σ-guided architecture search (toy genome)\n");
    printf("  generations=%d  population=20\n", generations);
    printf("  best: n_layers=%d n_experts=%d d_model=%d n_heads=%d\n",
           best.n_layers, best.n_experts, best.d_model, best.n_heads);
    printf("        tau_accept=%.3f tau_rethink=%.3f ttt_lr=%.6f engram_ttl=%d\n",
           (double)best.tau_accept, (double)best.tau_rethink,
           (double)best.ttt_lr, best.engram_ttl);
    return 0;
}
