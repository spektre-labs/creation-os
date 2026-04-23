/*
 * σ-weighted multimodal fusion.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "sensor_fusion.h"

#include "inference_cache.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Proxy for "BSC similarity < 0.3": typical unrelated sentences land ~0.48–0.55
 * Hamming norm in this encoder; tune with COS_FUSION_HAMMING_CONTRA env in future. */
#ifndef COS_FUSION_HAMMING_CONTRA
#define COS_FUSION_HAMMING_CONTRA 0.50f
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

int cos_sensor_fusion(const struct cos_perception_result *results, int n_results,
                      struct cos_fusion_result *fused)
{
    if (!results || !fused || n_results <= 0)
        return -1;
    memset(fused, 0, sizeof(*fused));
    if (n_results > 16)
        n_results = 16;

    for (int i = 0; i < n_results; i++) {
        int m = results[i].modality;
        if (m >= 0 && m < 3) {
            float s = results[i].sigma;
            if (s < 0.f)
                s = 0.f;
            if (s > 1.f)
                s = 1.f;
            fused->sigma_per_modality[m] =
                fmaxf(fused->sigma_per_modality[m], s);
        }
    }

    int contradiction = 0;
    uint64_t v0[COS_INF_W], v1[COS_INF_W];
    for (int i = 0; i < n_results && !contradiction; i++)
        for (int j = i + 1; j < n_results; j++) {
            cos_inference_bsc_encode_prompt(results[i].description, v0);
            cos_inference_bsc_encode_prompt(results[j].description, v1);
            float h = cos_inference_hamming_norm(v0, v1, COS_INF_W);
            if (h > COS_FUSION_HAMMING_CONTRA)
                contradiction = 1;
        }

    fused->contradiction  = contradiction;
    fused->n_modalities   = n_results;

    typedef struct {
        int idx;
        float sigma;
    } rank_t;
    rank_t rk[16];
    for (int i = 0; i < n_results; i++) {
        rk[i].idx   = i;
        rk[i].sigma = results[i].sigma;
    }
    for (int i = 0; i < n_results - 1; i++)
        for (int j = i + 1; j < n_results; j++)
            if (rk[j].sigma < rk[i].sigma) {
                rank_t t = rk[i];
                rk[i]      = rk[j];
                rk[j]      = t;
            }

    fused->dominant_modality = results[rk[0].idx].modality;

    float prod = 1.f;
    float sumw = 0.f;
    float w[16];
    for (int i = 0; i < n_results; i++) {
        float s = results[i].sigma;
        if (s < 0.f)
            s = 0.f;
        if (s > 1.f)
            s = 1.f;
        prod *= s;
        w[i] = (1.f - s) + 1e-6f;
        sumw += w[i];
    }

    if (contradiction) {
        float smax = 0.f;
        for (int i = 0; i < n_results; i++)
            smax = fmaxf(smax, results[i].sigma);
        fused->sigma_fused = smax;
        size_t off = 0;
        snprintf(fused->fused_description + off,
                 sizeof fused->fused_description - off,
                 "CONTRADICTION:");
        off = strlen(fused->fused_description);
        for (int i = 0; i < n_results && off + 4 < sizeof fused->fused_description;
             i++) {
            int n = snprintf(fused->fused_description + off,
                             sizeof fused->fused_description - off,
                             " [%d σ=%.3f] %s", results[i].modality,
                             (double)results[i].sigma, results[i].description);
            if (n > 0)
                off += (size_t)n;
        }
        return 0;
    }

    fused->sigma_fused = prod;

    size_t off = 0;
    for (int k = 0; k < n_results && off + 4 < sizeof fused->fused_description;
         k++) {
        int          i = rk[k].idx;
        float        wt = (sumw > 0.f) ? (w[i] / sumw) : (1.f / (float)n_results);
        const char *sep =
            k == 0 ? "" : " · ";
        int n = snprintf(fused->fused_description + off,
                         sizeof fused->fused_description - off,
                         "%s(%.2f)%s", sep, (double)wt, results[i].description);
        if (n <= 0)
            break;
        off += (size_t)n;
        if (off >= sizeof fused->fused_description - 1)
            break;
    }
    return 0;
}

#ifdef CREATION_OS_ENABLE_SELF_TESTS
#include <stdlib.h>

static int approx_eq(float a, float b)
{
    return fabsf(a - b) < 0.05f;
}

void cos_sensor_fusion_self_test(void)
{
    struct cos_fusion_result fused;

    struct cos_perception_result r1;
    memset(&r1, 0, sizeof r1);
    snprintf(r1.description, sizeof r1.description, "%s", "same scene cat");
    r1.sigma    = 0.2f;
    r1.modality = COS_PERCEPTION_IMAGE;

    struct cos_perception_result r2;
    memset(&r2, 0, sizeof r2);
    snprintf(r2.description, sizeof r2.description, "%s", "same scene cat");
    r2.sigma    = 0.3f;
    r2.modality = COS_PERCEPTION_TEXT;

    struct cos_perception_result pair_a[2] = { r1, r2 };
    float                          expect_prod = r1.sigma * r2.sigma;
    if (cos_sensor_fusion(pair_a, 2, &fused) != 0 || fused.contradiction != 0
        || !approx_eq(fused.sigma_fused, expect_prod))
        abort();

    struct cos_perception_result a;
    memset(&a, 0, sizeof a);
    snprintf(a.description, sizeof a.description, "%s",
             "The capital of France is Paris and the river Seine flows through "
             "it.");
    a.sigma    = 0.2f;
    a.modality = COS_PERCEPTION_IMAGE;
    struct cos_perception_result b;
    memset(&b, 0, sizeof b);
    snprintf(b.description, sizeof b.description, "%s",
             "Quantum chromodynamics confines quarks through color charge and "
             "asymptotic freedom.");
    b.sigma    = 0.5f;
    b.modality = COS_PERCEPTION_TEXT;
    struct cos_perception_result pair_b[2] = { a, b };
    if (cos_sensor_fusion(pair_b, 2, &fused) != 0 || fused.contradiction != 1)
        abort();

    if (cos_sensor_fusion(NULL, 1, &fused) != -1)
        abort();
}
#endif
