/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "router.h"

#include "../sigma/calibrate.h"
#include "../sigma/channels.h"
#include "../sigma/decompose.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **pp)
{
    while (*pp && isspace((unsigned char)**pp))
        (*pp)++;
}

static int extract_json_string_value(const char *json, const char *key, char *out, size_t cap)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(json, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    if (*p != '"')
        return -1;
    p++;
    size_t w = 0;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            if (w + 1 >= cap)
                return -1;
            out[w++] = *p++;
            continue;
        }
        if (w + 1 >= cap)
            return -1;
        out[w++] = *p++;
    }
    out[w] = '\0';
    return (int)w;
}

static int extract_json_float(const char *json, const char *key, float *out)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(json, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    errno = 0;
    char *end = NULL;
    double v = strtod(p, &end);
    if (end == p || errno == ERANGE)
        return -1;
    *out = (float)v;
    return 0;
}

static int extract_json_int(const char *json, const char *key, int *out)
{
    char pat[160];
    if (snprintf(pat, sizeof pat, "\"%s\"", key) >= (int)sizeof pat)
        return -1;
    const char *p = strstr(json, pat);
    if (!p)
        return -1;
    p += strlen(pat);
    skip_ws(&p);
    if (*p != ':')
        return -1;
    p++;
    skip_ws(&p);
    errno = 0;
    char *end = NULL;
    long v = strtol(p, &end, 10);
    if (end == p || errno == ERANGE || v > 2147483647L || v < -2147483648L)
        return -1;
    *out = (int)v;
    return 0;
}

void cos_routing_defaults(cos_routing_config_t *cfg)
{
    if (!cfg)
        return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->threshold_logit_entropy = 0.85f;
    cfg->threshold_top_margin = 0.92f;
    cfg->max_fallback_budget_per_session = 8;
    cfg->routing_mode_v34 = 0;
    cfg->threshold_aleatoric = 0.45f;
    cfg->threshold_min_p_correct = 0.42f;
    cfg->threshold_epistemic_raw = 0.05f;
    cfg->platt_a = -6.0f;
    cfg->platt_b = 0.0f;
    cfg->platt_calib_valid = 0;
    cfg->sigma_calibration_path[0] = '\0';
}

int cos_routing_load(const char *path, cos_routing_config_t *cfg)
{
    if (!cfg)
        return -1;
    cos_routing_defaults(cfg);
    if (!path || !*path)
        return 0;

    FILE *f = fopen(path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0 || sz > 1 << 20) {
        fclose(f);
        return -1;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1u);
    if (!buf) {
        fclose(f);
        return -1;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';

    float fe, fm;
    if (extract_json_float(buf, "threshold_logit_entropy", &fe) == 0)
        cfg->threshold_logit_entropy = fe;
    if (extract_json_float(buf, "threshold_top_margin", &fm) == 0)
        cfg->threshold_top_margin = fm;
    /* Nested: "channels": { "logit_entropy": 0.85, "top_margin": 0.92 } */
    const char *ch = strstr(buf, "\"channels\"");
    if (ch) {
        const char *brace = strchr(ch, '{');
        if (brace) {
            char nested[4096];
            const char *e = strchr(brace, '}');
            if (e && (size_t)(e - brace + 1) < sizeof nested) {
                memcpy(nested, brace, (size_t)(e - brace + 1));
                nested[(size_t)(e - brace + 1)] = '\0';
                if (extract_json_float(nested, "logit_entropy", &fe) == 0)
                    cfg->threshold_logit_entropy = fe;
                if (extract_json_float(nested, "top_margin", &fm) == 0)
                    cfg->threshold_top_margin = fm;
            }
        }
    }

    (void)extract_json_string_value(buf, "fallback_model", cfg->fallback_model_name,
        sizeof cfg->fallback_model_name);
    (void)extract_json_string_value(buf, "fallback_endpoint", cfg->fallback_endpoint,
        sizeof cfg->fallback_endpoint);

    int budget = cfg->max_fallback_budget_per_session;
    if (extract_json_int(buf, "max_fallback_budget_per_session", &budget) == 0)
        cfg->max_fallback_budget_per_session = budget;

    char mode[32];
    if (extract_json_string_value(buf, "routing_mode", mode, sizeof mode) >= 0) {
        if (!strcmp(mode, "v34"))
            cfg->routing_mode_v34 = 1;
        else
            cfg->routing_mode_v34 = 0;
    }

    float ta, tmc, ter;
    if (extract_json_float(buf, "threshold_aleatoric", &ta) == 0)
        cfg->threshold_aleatoric = ta;
    if (extract_json_float(buf, "threshold_min_p_correct", &tmc) == 0)
        cfg->threshold_min_p_correct = tmc;
    if (extract_json_float(buf, "threshold_epistemic_raw", &ter) == 0)
        cfg->threshold_epistemic_raw = ter;

    (void)extract_json_string_value(buf, "sigma_calibration_path", cfg->sigma_calibration_path,
        sizeof cfg->sigma_calibration_path);

    if (cfg->routing_mode_v34) {
        const char *cp = cfg->sigma_calibration_path[0] ? cfg->sigma_calibration_path : "config/sigma_calibration.json";
        sigma_platt_params_t pp;
        if (sigma_platt_load(cp, &pp) == 0 && pp.valid) {
            cfg->platt_a = pp.a;
            cfg->platt_b = pp.b;
            cfg->platt_calib_valid = 1;
        }
    }

    float pa, pb;
    if (extract_json_float(buf, "platt_a", &pa) == 0) {
        cfg->platt_a = pa;
        cfg->platt_calib_valid = 1;
    }
    if (extract_json_float(buf, "platt_b", &pb) == 0) {
        cfg->platt_b = pb;
        cfg->platt_calib_valid = 1;
    }

    free(buf);
    return 0;
}

cos_route_result_t cos_route_from_logits(const cos_routing_config_t *cfg,
    const float *logits,
    int n_logits,
    int fallback_model_available,
    int *fallback_budget_remaining_io)
{
    cos_route_result_t r;
    memset(&r, 0, sizeof r);
    r.decision = COS_ROUTE_PRIMARY;
    if (!cfg || !logits || n_logits <= 0)
        return r;

    float ent = sigma_logit_entropy(logits, n_logits);
    float margin = sigma_top_margin(logits, n_logits);
    int trip_e = ent > cfg->threshold_logit_entropy;
    int trip_m = margin > cfg->threshold_top_margin;
    if (!trip_e && !trip_m) {
        r.decision = COS_ROUTE_PRIMARY;
        return r;
    }
    if (trip_e)
        r.abstain_channel_mask |= 1u;
    if (trip_m)
        r.abstain_channel_mask |= 2u;

    int budget = cfg->max_fallback_budget_per_session;
    if (fallback_budget_remaining_io)
        budget = *fallback_budget_remaining_io;

    if (!fallback_model_available || cfg->fallback_model_name[0] == '\0') {
        r.decision = COS_ROUTE_ABSTAIN;
        return r;
    }
    if (budget <= 0) {
        r.decision = COS_ROUTE_ABSTAIN;
        r.budget_exhausted = 1;
        return r;
    }

    r.decision = COS_ROUTE_FALLBACK;
    if (fallback_budget_remaining_io && *fallback_budget_remaining_io > 0)
        (*fallback_budget_remaining_io)--;
    return r;
}

cos_route_result_t cos_route_from_logits_v34(const cos_routing_config_t *cfg,
    const float *logits,
    int n_logits,
    int fallback_model_available,
    int *fallback_budget_remaining_io)
{
    cos_route_result_t r;
    memset(&r, 0, sizeof r);
    r.decision = COS_ROUTE_PRIMARY;
    if (!cfg || !logits || n_logits <= 0)
        return r;

    sigma_decomposed_t dec;
    sigma_decompose_dirichlet_evidence(logits, n_logits, &dec);
    float margin = sigma_top_margin(logits, n_logits);

    int epi_like = 0;
    if (cfg->platt_calib_valid) {
        float p = sigma_platt_p_correct(dec.epistemic, cfg->platt_a, cfg->platt_b);
        epi_like = (p < cfg->threshold_min_p_correct) ? 1 : 0;
    } else {
        epi_like = (dec.epistemic > cfg->threshold_epistemic_raw) ? 1 : 0;
    }
    if (margin > cfg->threshold_top_margin)
        epi_like = 1;

    if (epi_like)
        r.abstain_channel_mask |= 16u;
    if (margin > cfg->threshold_top_margin)
        r.abstain_channel_mask |= 2u;

    if (epi_like) {
        int budget = cfg->max_fallback_budget_per_session;
        if (fallback_budget_remaining_io)
            budget = *fallback_budget_remaining_io;

        if (!fallback_model_available || cfg->fallback_model_name[0] == '\0') {
            r.decision = COS_ROUTE_ABSTAIN;
            return r;
        }
        if (budget <= 0) {
            r.decision = COS_ROUTE_ABSTAIN;
            r.budget_exhausted = 1;
            return r;
        }
        r.decision = COS_ROUTE_FALLBACK;
        if (fallback_budget_remaining_io && *fallback_budget_remaining_io > 0)
            (*fallback_budget_remaining_io)--;
        return r;
    }

    if (dec.aleatoric > cfg->threshold_aleatoric) {
        r.decision = COS_ROUTE_PRIMARY_AMBIGUOUS;
        r.abstain_channel_mask |= 4u;
        return r;
    }

    r.decision = COS_ROUTE_PRIMARY;
    return r;
}
