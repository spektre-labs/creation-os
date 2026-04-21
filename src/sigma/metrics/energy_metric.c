/* ULTRA-7 — energy-aware quality metrics (illustrative scalars). */

#include "energy_metric.h"

#include <math.h>
#include <stdio.h>

float cos_ultra_reasoning_per_joule(float correct_answers, float total_joules) {
    if (total_joules <= 0.0f) return 0.0f;
    if (correct_answers < 0.0f) correct_answers = 0.0f;
    return correct_answers / total_joules;
}

float cos_ultra_sigma_efficiency(float sigma_mean, float joules_per_query) {
    if (joules_per_query <= 0.0f) joules_per_query = 1e-6f;
    float s = sigma_mean;
    if (s != s) s = 1.0f;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    return (1.0f - s) / joules_per_query;
}

void cos_ultra_energy_print_demo_table(FILE *fp) {
    if (!fp) return;
    struct {
        const char *name;
        float acc;
        float j_per_q;
    } rows[] = {
        { "bitnet_only",   0.261f, 0.8f },
        { "sigma_pipeline",0.336f, 1.2f },
        { "sigma_selective",0.520f, 0.5f },
    };
    fprintf(fp, "cos benchmark --energy (ULTRA-7 demo constants)\n");
    fprintf(fp, " %-16s | %8s | %8s | %14s\n",
            "config", "accuracy", "J/query", "reasoning/J");
    fprintf(fp, " -----------------+----------+----------+--------------\n");
    for (size_t i = 0; i < sizeof rows / sizeof rows[0]; ++i) {
        float rpj = cos_ultra_reasoning_per_joule(rows[i].acc, rows[i].j_per_q);
        fprintf(fp, " %-16s | %8.3f | %8.1f | %14.3f\n",
                rows[i].name, (double)rows[i].acc, (double)rows[i].j_per_q,
                (double)rpj);
    }
}

int cos_ultra_energy_metric_self_test(void) {
    float r = cos_ultra_reasoning_per_joule(2.0f, 4.0f);
    if (fabsf(r - 0.5f) > 1e-4f) return 1;
    float e = cos_ultra_sigma_efficiency(0.5f, 2.0f);
    if (fabsf(e - 0.25f) > 1e-4f) return 2;
    return 0;
}
