/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-error attribution implementation. */
#include "error_attribution.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef COS_ERR_HI
#define COS_ERR_HI 0.55f
#endif
#ifndef COS_ERR_LO
#define COS_ERR_LO 0.35f
#endif

static float clamp01f(float x) {
    if (x < 0.f) return 0.f;
    if (x > 1.f) return 1.f;
    return x;
}

static void fill(struct cos_error_attribution *a,
                 enum cos_error_source src,
                 float conf,
                 const char *reason,
                 const char *fix) {
    memset(a, 0, sizeof(*a));
    a->source = src;
    a->confidence = clamp01f(conf);
    snprintf(a->reason, sizeof(a->reason), "%s", reason ? reason : "");
    snprintf(a->fix, sizeof(a->fix), "%s", fix ? fix : "");
}

struct cos_error_attribution cos_error_attribute(
    float sigma_logprob,
    float sigma_entropy,
    float sigma_consistency,
    float meta_perception) {
    struct cos_error_attribution a;
    float lp = clamp01f(sigma_logprob);
    float en = clamp01f(sigma_entropy);
    float cs = clamp01f(sigma_consistency);
    float mp = clamp01f(meta_perception);

    int low_lp = (lp < COS_ERR_LO);
    int high_lp = (lp > COS_ERR_HI);
    int high_en = (en > COS_ERR_HI);
    int low_cs = (cs < COS_ERR_LO);
    int high_cs = (cs > COS_ERR_HI);
    int high_mp = (mp > COS_ERR_HI);

    /* Priority order follows the specification narrative. */
    if (lp < COS_ERR_LO && en < COS_ERR_LO && cs < COS_ERR_LO &&
        mp < COS_ERR_LO) {
        fill(&a, COS_ERR_NONE, 0.92f,
             "σ channels nominal — no attribution fault",
             "continue");
        return a;
    }

    if (high_lp && high_en) {
        fill(&a, COS_ERR_EPISTEMIC, 0.78f,
             "high σ_logprob and σ_entropy — model uncertainty dominates",
             "escalate or retrieve from engram");
        return a;
    }

    if (low_lp && high_en) {
        fill(&a, COS_ERR_ALEATORIC, 0.74f,
             "low σ_logprob with high σ_entropy — diffuse output mass",
             "present multiple answers");
        return a;
    }

    if (high_lp && low_cs) {
        fill(&a, COS_ERR_REASONING, 0.72f,
             "high σ_logprob with low σ_consistency — unstable reasoning",
             "rethink with different temperature");
        return a;
    }

    if (low_lp && high_cs && !high_mp) {
        fill(&a, COS_ERR_MEMORY, 0.68f,
             "calibrated tokens yet mismatch pattern — retrieval gap",
             "escalate to API");
        return a;
    }

    if (lp < COS_ERR_LO && en < COS_ERR_LO && cs < COS_ERR_LO &&
        high_mp) {
        fill(&a, COS_ERR_NOVEL_DOMAIN, 0.70f,
             "input underspecified vs calm σ stack — unfamiliar domain cue",
             "flag for calibration");
        return a;
    }

    /* Fallback blend */
    if (high_lp) {
        fill(&a, COS_ERR_EPISTEMIC, 0.55f,
             "elevated σ_logprob — conservative epistemic attribution",
             "escalate or retrieve from engram");
    } else if (high_en) {
        fill(&a, COS_ERR_ALEATORIC, 0.52f,
             "elevated σ_entropy — diffuse likelihood surface",
             "present multiple answers");
    } else {
        fill(&a, COS_ERR_NONE, 0.50f,
             "no dominant uncertainty factor",
             "monitor");
    }
    return a;
}

char *cos_error_attribution_to_json(const struct cos_error_attribution *a) {
    if (a == NULL) return NULL;
    char *buf = (char *)malloc(768);
    if (!buf) return NULL;
    snprintf(buf, 768,
             "{\"source\":%d,\"confidence\":%.4f,"
             "\"reason\":\"%.240s\",\"fix\":\"%.240s\"}",
             (int)a->source, (double)a->confidence,
             a->reason, a->fix);
    return buf;
}

int cos_error_attribution_self_test(void) {
    struct cos_error_attribution a;

    a = cos_error_attribute(0.9f, 0.9f, 0.5f, 0.1f);
    if (a.source != COS_ERR_EPISTEMIC) return 1;

    a = cos_error_attribute(0.2f, 0.8f, 0.5f, 0.1f);
    if (a.source != COS_ERR_ALEATORIC) return 2;

    a = cos_error_attribute(0.85f, 0.4f, 0.15f, 0.2f);
    if (a.source != COS_ERR_REASONING) return 3;

    a = cos_error_attribute(0.25f, 0.3f, 0.85f, 0.15f);
    if (a.source != COS_ERR_MEMORY) return 4;

    a = cos_error_attribute(0.15f, 0.15f, 0.15f, 0.85f);
    if (a.source != COS_ERR_NOVEL_DOMAIN) return 5;

    a = cos_error_attribute(0.05f, 0.05f, 0.05f, 0.05f);
    if (a.source != COS_ERR_NONE) return 6;

    /* Edge: boundary LP */
    a = cos_error_attribute(COS_ERR_HI + 0.01f, COS_ERR_HI + 0.01f,
                            0.5f, 0.1f);
    if (a.source != COS_ERR_EPISTEMIC) return 7;

    /* Edge: tie toward epistemic when LP dominates */
    a = cos_error_attribute(0.7f, 0.5f, 0.5f, 0.4f);
    if (a.source != COS_ERR_EPISTEMIC && a.source != COS_ERR_REASONING)
        return 8;

    /* JSON smoke */
    char *js = cos_error_attribution_to_json(&a);
    if (!js || strstr(js, "\"source\"") == NULL) {
        free(js);
        return 9;
    }
    free(js);

    /* Memory branch distinct from novel */
    a = cos_error_attribute(0.25f, 0.25f, 0.9f, 0.2f);
    if (a.source != COS_ERR_MEMORY) return 10;

    return 0;
}
