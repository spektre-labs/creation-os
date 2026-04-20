/* σ-MoE self-test binary. */

#include "moe.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_moe_self_test();

    /* Canonical demo: 4 experts, σ sweep shows the width gate. */
    float logits[4] = {0.10f, 0.90f, 0.30f, 0.70f};
    float sigmas[4] = {0.05f, 0.20f, 0.40f, 0.80f};
    cos_moe_activation_t log[4];
    for (int i = 0; i < 4; ++i)
        log[i] = cos_sigma_moe_route(sigmas[i], logits, 4, NULL);
    float saved = cos_sigma_moe_compute_saved(log, 4);

    /* Top-K @ σ=0.05 → all widths Q; K=2. */
    cos_moe_activation_t topk[2];
    int n_k = cos_sigma_moe_top_k_route(
        0.05f, logits, 4, 2, NULL, topk);

    printf("{\"kernel\":\"sigma_moe\","
           "\"self_test_rc\":%d,"
           "\"cutoffs\":{\"q\":%.4f,\"h\":%.4f,\"tq\":%.4f},"
           "\"demo\":["
             "{\"sigma\":%.2f,\"expert_id\":%d,\"width\":\"%s\","
              "\"width_frac\":%.2f},"
             "{\"sigma\":%.2f,\"expert_id\":%d,\"width\":\"%s\","
              "\"width_frac\":%.2f},"
             "{\"sigma\":%.2f,\"expert_id\":%d,\"width\":\"%s\","
              "\"width_frac\":%.2f},"
             "{\"sigma\":%.2f,\"expert_id\":%d,\"width\":\"%s\","
              "\"width_frac\":%.2f}],"
           "\"compute_saved\":%.4f,"
           "\"top_k\":{\"k\":%d,\"experts\":[%d,%d]},"
           "\"pass\":%s}\n",
           rc,
           (double)COS_MOE_CUTOFFS_DEFAULT.c_quarter,
           (double)COS_MOE_CUTOFFS_DEFAULT.c_half,
           (double)COS_MOE_CUTOFFS_DEFAULT.c_three_quarter,
           (double)sigmas[0], log[0].expert_id,
           cos_sigma_moe_width_label(log[0].width),
           (double)log[0].width_frac,
           (double)sigmas[1], log[1].expert_id,
           cos_sigma_moe_width_label(log[1].width),
           (double)log[1].width_frac,
           (double)sigmas[2], log[2].expert_id,
           cos_sigma_moe_width_label(log[2].width),
           (double)log[2].width_frac,
           (double)sigmas[3], log[3].expert_id,
           cos_sigma_moe_width_label(log[3].width),
           (double)log[3].width_frac,
           (double)saved, n_k, topk[0].expert_id, topk[1].expert_id,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-MoE demo:\n");
        fprintf(stderr,
            "  cutoffs: Q<%.2f H<%.2f TQ<%.2f (else F)\n",
            (double)COS_MOE_CUTOFFS_DEFAULT.c_quarter,
            (double)COS_MOE_CUTOFFS_DEFAULT.c_half,
            (double)COS_MOE_CUTOFFS_DEFAULT.c_three_quarter);
        for (int i = 0; i < 4; ++i)
            fprintf(stderr,
                "  σ=%.2f → expert %d @ %s (%.2fx)\n",
                (double)sigmas[i], log[i].expert_id,
                cos_sigma_moe_width_label(log[i].width),
                (double)log[i].width_frac);
        fprintf(stderr, "  FFN FLOPs saved: %.1f%%\n",
            100.0 * (double)saved);
    }
    return rc;
}
