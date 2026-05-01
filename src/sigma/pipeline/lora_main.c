/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-LoRA demo — trains two tiny adapters ("factual", "code"),
 * reports σ_before/σ_after + σ_improvement, demonstrates the
 * multi-adapter registry, and prints a deterministic JSON
 * envelope for benchmarks/sigma_pipeline/check_sigma_lora.sh.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "lora.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fill(float *buf, int n, float base, float step) {
    for (int i = 0; i < n; i++) buf[i] = base + step * (float)i;
}

static int demo_train(cos_lora_adapter_t *a,
                      const float *x, const float *t,
                      float lr, int steps) {
    float base[COS_LORA_MAX_OUT_DIM] = {0};
    for (int s = 0; s < steps; s++) {
        int rc = cos_lora_train_step(a, x, t, base, lr, NULL);
        if (rc != COS_LORA_OK) return rc;
    }
    return COS_LORA_OK;
}

int main(void) {
    int st = cos_lora_self_test();

    cos_lora_adapter_t factual, code;
    cos_lora_init(&factual, "factual_v1", 16, 8,  8,  16.0f, 0xFAC7FAC7FAC7FAC7ULL);
    cos_lora_init(&code,    "code_v1",    16, 8, 16,  32.0f, 0xC0DEC0DEC0DEC0DEULL);

    float x_fact[16], t_fact[8], x_code[16], t_code[8];
    fill(x_fact, 16, -0.5f, 0.07f);
    fill(t_fact,  8,  0.2f, -0.05f);
    fill(x_code, 16,  0.3f, -0.04f);
    fill(t_code,  8, -0.1f,  0.08f);

    demo_train(&factual, x_fact, t_fact, 0.05f, 300);
    demo_train(&code,    x_code, t_code, 0.05f, 300);

    cos_lora_registry_t reg;
    cos_lora_registry_init(&reg);
    cos_lora_registry_bind(&reg, "factual", &factual);
    cos_lora_registry_bind(&reg, "code",    &code);

    const cos_lora_adapter_t *pick_fact = cos_lora_route(&reg, "factual");
    const cos_lora_adapter_t *pick_code = cos_lora_route(&reg, "code");
    const cos_lora_adapter_t *pick_base = cos_lora_route(&reg, "general");

    cos_lora_stats_t sf, sc;
    cos_lora_stats(&factual, &sf);
    cos_lora_stats(&code,    &sc);

    printf("{\n");
    printf("  \"module\": \"sigma_lora\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"adapters\": [\n");
    printf("    { \"name\": \"%s\", \"rank\": %d, \"in_dim\": %d, \"out_dim\": %d,"
           " \"alpha\": %.4f, \"bytes\": %zu, \"steps\": %d,"
           " \"sigma_before\": %.4f, \"sigma_after\": %.4f,"
           " \"sigma_improvement\": %.4f },\n",
           factual.name, factual.rank, factual.in_dim, factual.out_dim,
           (double)factual.alpha, sf.bytes_weights, sf.steps,
           (double)sf.sigma_before, (double)sf.sigma_after,
           (double)sf.sigma_improvement);
    printf("    { \"name\": \"%s\", \"rank\": %d, \"in_dim\": %d, \"out_dim\": %d,"
           " \"alpha\": %.4f, \"bytes\": %zu, \"steps\": %d,"
           " \"sigma_before\": %.4f, \"sigma_after\": %.4f,"
           " \"sigma_improvement\": %.4f }\n",
           code.name, code.rank, code.in_dim, code.out_dim,
           (double)code.alpha, sc.bytes_weights, sc.steps,
           (double)sc.sigma_before, (double)sc.sigma_after,
           (double)sc.sigma_improvement);
    printf("  ],\n");
    printf("  \"routing\": {\n");
    printf("    \"factual\": \"%s\",\n", pick_fact ? pick_fact->name : "");
    printf("    \"code\":    \"%s\",\n", pick_code ? pick_code->name : "");
    printf("    \"general\": \"%s\"\n",  pick_base ? pick_base->name : "");
    printf("  }\n");
    printf("}\n");

    cos_lora_free(&factual);
    cos_lora_free(&code);
    return st == 0 ? 0 : 1;
}
