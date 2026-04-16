/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "spec_decode.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float cos_spec_epistemic_unit_score(const sigma_decomposed_t *s)
{
    if (!s)
        return 1.0f;
    float e = s->epistemic;
    if (!(e > 0.0f) || !isfinite(e))
        return 0.0f;
    return e / (1.0f + e);
}

int cos_spec_compute_draft_length(sigma_decomposed_t s, int max_k)
{
    sigma_decomposed_t tmp = s;
    float u = cos_spec_epistemic_unit_score(&tmp);
    int k;
    if (u > 0.7f)
        k = 0;
    else if (u > 0.4f)
        k = 2;
    else if (u > 0.2f)
        k = 4;
    else
        k = 8;
    if (max_k >= 0 && k > max_k)
        k = max_k;
    if (k < 0)
        k = 0;
    return k;
}

void cos_spec_routing_defaults(cos_spec_routing_t *r)
{
    if (!r)
        return;
    memset(r, 0, sizeof(*r));
    r->sigma_local_threshold = 0.4f;
    r->sigma_api_threshold = 0.8f;
    r->verifier_abstain_epistemic = 0.7f;
    (void)snprintf(r->draft_name, sizeof r->draft_name, "bitnet-2b4t");
    (void)snprintf(r->verifier_name, sizeof r->verifier_name, "phi-fallback");
}

static void skip_ws(const char **pp)
{
    while (*pp && isspace((unsigned char)**pp))
        (*pp)++;
}

static int extract_json_float(const char *json, const char *key, float *out)
{
    char pat[128];
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

int cos_spec_routing_load(const char *path, cos_spec_routing_t *r)
{
    if (!r)
        return -1;
    cos_spec_routing_defaults(r);
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

    float fl;
    if (extract_json_float(buf, "sigma_local_threshold", &fl) == 0)
        r->sigma_local_threshold = fl;
    if (extract_json_float(buf, "sigma_api_threshold", &fl) == 0)
        r->sigma_api_threshold = fl;
    if (extract_json_float(buf, "verifier_abstain_epistemic", &fl) == 0)
        r->verifier_abstain_epistemic = fl;

    (void)extract_json_string_value(buf, "draft_model", r->draft_name, sizeof r->draft_name);
    (void)extract_json_string_value(buf, "verifier_model", r->verifier_name, sizeof r->verifier_name);

    free(buf);
    return 0;
}

cos_spec_tier_t cos_spec_route_tier(float u, const cos_spec_routing_t *r)
{
    if (!r)
        return COS_SPEC_TIER_API;
    if (u < r->sigma_local_threshold)
        return COS_SPEC_TIER_LOCAL_ONLY;
    if (u < r->sigma_api_threshold)
        return COS_SPEC_TIER_SPEC_LOCAL;
    return COS_SPEC_TIER_API;
}

void cos_spec_verify_with_dual_sigma(const int *draft_agrees,
    const float *verifier_epistemic_per_token,
    int n,
    float abstain_threshold,
    cos_spec_verify_stats_t *out)
{
    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!draft_agrees || !verifier_epistemic_per_token || n <= 0)
        return;

    for (int i = 0; i < n; i++) {
        if (!draft_agrees[i]) {
            out->rejected++;
            break;
        }
        float ve = verifier_epistemic_per_token[i];
        if (ve > abstain_threshold) {
            out->abstained++;
            break;
        }
        out->accepted++;
    }
}
