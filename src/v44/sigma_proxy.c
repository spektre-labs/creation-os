/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_proxy.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void v44_engine_raw_free(v44_engine_raw_t *r)
{
    if (!r) {
        return;
    }
    free(r->text);
    free(r->logits_flat);
    memset(r, 0, sizeof *r);
}

void v44_proxy_response_free(v44_proxy_response_t *r)
{
    if (!r) {
        return;
    }
    free(r->text);
    free(r->logits_flat);
    free(r->sigmas);
    memset(r, 0, sizeof *r);
}

void v44_sigma_config_default(v44_sigma_config_t *cfg)
{
    if (!cfg) {
        return;
    }
    memset(cfg, 0, sizeof *cfg);
    for (int i = 0; i < CH_COUNT; i++) {
        cfg->calibrated_threshold[i] = 0.55f;
    }
    cfg->max_resample_attempts = 2;
    cfg->gate_abstain = 0.75f;
    cfg->gate_fallback = 0.85f;
    cfg->gate_cite = 0.50f;
    cfg->calibrated_threshold[CH_EPISTEMIC] = cfg->gate_abstain;
    cfg->calibrated_threshold[CH_ALEATORIC] = cfg->gate_cite;
    cfg->calibrated_threshold[CH_LOGIT_ENTROPY] = cfg->gate_fallback;
}

void v44_aggregate_sigmas(const sigma_decomposed_t *per_token, int n_tokens, sigma_decomposed_t *agg)
{
    if (!agg) {
        return;
    }
    memset(agg, 0, sizeof *agg);
    if (!per_token || n_tokens <= 0) {
        return;
    }
    float te = 0.0f, ta = 0.0f, tt = 0.0f;
    for (int i = 0; i < n_tokens; i++) {
        te += per_token[i].epistemic;
        ta += per_token[i].aleatoric;
        tt += per_token[i].total;
    }
    float inv = 1.0f / (float)n_tokens;
    agg->epistemic = te * inv;
    agg->aleatoric = ta * inv;
    agg->total = tt * inv;
}

void v44_sigma_from_logits_row(const float *logits_row, int vocab_size, sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof *out);
    if (!logits_row || vocab_size < 2) {
        return;
    }
    sigma_decompose_dirichlet_evidence(logits_row, vocab_size, out);
}

void v44_agg_to_syndrome(const sigma_decomposed_t *agg, float *sigma_vec)
{
    if (!agg || !sigma_vec) {
        return;
    }
    for (int i = 0; i < CH_COUNT; i++) {
        sigma_vec[i] = 0.0f;
    }
    sigma_vec[CH_EPISTEMIC] = agg->epistemic;
    sigma_vec[CH_ALEATORIC] = agg->aleatoric;
    sigma_vec[CH_LOGIT_ENTROPY] = agg->total;
}

static uint32_t v44_fnv1a(const char *s)
{
    uint32_t h = 2166136261u;
    if (!s) {
        return h;
    }
    for (; *s; s++) {
        h ^= (uint32_t)(unsigned char)*s;
        h *= 16777619u;
    }
    return h;
}

int v44_stub_engine_generate(const v44_engine_config_t *eng, const char *prompt, v44_engine_raw_t *out)
{
    if (!out) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    const int vocab = 16;
    const char *p = prompt ? prompt : "";
    int spike = (strstr(p, "SPIKE_SIGMA") != NULL);
    int ttc = (strstr(p, "[TTC]") != NULL);
    int n_tok = 3;
    size_t plen = strlen(p);
    if (plen < 4u) {
        n_tok = 1;
    }
    out->vocab_size = vocab;
    out->n_tokens = n_tok;
    out->logits_flat = (float *)calloc((size_t)n_tok * (size_t)vocab, sizeof(float));
    if (!out->logits_flat) {
        return -1;
    }
    for (int t = 0; t < n_tok; t++) {
        uint32_t h = v44_fnv1a(eng ? eng->engine_type : "") ^ v44_fnv1a(eng ? eng->engine_url : "") ^
            v44_fnv1a(p) ^ (uint32_t)(t * 1315423911u);
        if (ttc) {
            h ^= 0xA11CEu;
        }
        int peak = (int)(h % (unsigned)vocab);
        float *row = out->logits_flat + t * vocab;
        for (int j = 0; j < vocab; j++) {
            row[j] = -1.0f;
        }
        row[peak] = spike ? 8.0f : (ttc ? 3.5f : 5.0f);
        if (spike && t == n_tok - 1) {
            row[peak] = 12.0f;
        }
    }
    const char *prefix = "v44_stub:";
    size_t tlen = strlen(prefix) + plen + 1u;
    out->text = (char *)malloc(tlen);
    if (!out->text) {
        free(out->logits_flat);
        out->logits_flat = NULL;
        return -1;
    }
    (void)snprintf(out->text, tlen, "%s%s", prefix, p);
    (void)eng;
    return 0;
}

static int v44_execute_action(sigma_action_t act, v44_proxy_response_t *r, const sigma_decomposed_t *agg)
{
    if (!r) {
        return -1;
    }
    switch (act) {
    case ACTION_EMIT:
        return 0;
    case ACTION_ABSTAIN: {
        free(r->text);
        r->text = strdup("[sigma-abstain: confidence too low to answer reliably]");
        if (!r->text) {
            return -1;
        }
        return 0;
    }
    case ACTION_RESAMPLE:
        /* handled by caller recursion */
        return 0;
    case ACTION_FALLBACK: {
        free(r->text);
        char buf[256];
        (void)snprintf(buf, sizeof buf,
            "[sigma-fallback: route to configured larger model; aggregate_epistemic=%.3f]",
            agg ? (double)agg->epistemic : 0.0);
        r->text = strdup(buf);
        return r->text ? 0 : -1;
    }
    case ACTION_DECOMPOSE: {
        char *old = r->text;
        size_t ol = old ? strlen(old) : 0u;
        size_t need = ol + strlen(" [sigma-decompose: split query]") + 1u;
        char *n = (char *)malloc(need);
        if (!n) {
            return -1;
        }
        if (old) {
            (void)memcpy(n, old, ol + 1u);
        } else {
            n[0] = '\0';
        }
        (void)strcat(n, " [sigma-decompose: split query]");
        free(old);
        r->text = n;
        return 0;
    }
    case ACTION_CITE: {
        char *old = r->text;
        size_t ol = old ? strlen(old) : 0u;
        char suf[128];
        (void)snprintf(suf, sizeof suf, " [sigma-cite: epistemic=%.3f]", agg ? (double)agg->epistemic : 0.0);
        size_t need = ol + strlen(suf) + 1u;
        char *n = (char *)malloc(need);
        if (!n) {
            return -1;
        }
        if (old) {
            (void)memcpy(n, old, ol + 1u);
        } else {
            n[0] = '\0';
        }
        (void)strcat(n, suf);
        free(old);
        r->text = n;
        return 0;
    }
    default:
        return 0;
    }
}

int v44_sigma_proxy_generate(const v44_engine_config_t *eng, const char *prompt, const v44_sigma_config_t *sigma_cfg,
    int depth, v44_proxy_response_t *out)
{
    if (!out || !sigma_cfg) {
        return -1;
    }
    memset(out, 0, sizeof *out);
    v44_engine_raw_t raw;
    memset(&raw, 0, sizeof raw);
    if (v44_stub_engine_generate(eng, prompt, &raw) != 0) {
        return -1;
    }
    out->n_tokens = raw.n_tokens;
    out->vocab_size = raw.vocab_size;
    out->text = raw.text;
    raw.text = NULL;
    if (raw.logits_flat) {
        size_t nfloat = (size_t)raw.n_tokens * (size_t)raw.vocab_size;
        out->logits_flat = (float *)malloc(nfloat * sizeof(float));
        if (!out->logits_flat) {
            v44_engine_raw_free(&raw);
            v44_proxy_response_free(out);
            return -1;
        }
        (void)memcpy(out->logits_flat, raw.logits_flat, nfloat * sizeof(float));
    }
    v44_engine_raw_free(&raw);

    out->sigmas = (sigma_decomposed_t *)calloc((size_t)out->n_tokens, sizeof(sigma_decomposed_t));
    if (!out->sigmas) {
        v44_proxy_response_free(out);
        return -1;
    }
    for (int i = 0; i < out->n_tokens; i++) {
        const float *row = out->logits_flat + i * out->vocab_size;
        v44_sigma_from_logits_row(row, out->vocab_size, &out->sigmas[i]);
    }
    sigma_decomposed_t agg;
    v44_aggregate_sigmas(out->sigmas, out->n_tokens, &agg);
    float svec[CH_COUNT];
    v44_agg_to_syndrome(&agg, svec);
    sigma_action_t act = decode_sigma_syndrome(svec, CH_COUNT, sigma_cfg->calibrated_threshold);
    out->action = act;
    out->resample_depth = depth;

    if (act == ACTION_RESAMPLE) {
        if (depth >= sigma_cfg->max_resample_attempts) {
            act = ACTION_ABSTAIN;
            out->action = act;
        } else {
            v44_proxy_response_free(out);
            return v44_sigma_proxy_generate(eng, prompt, sigma_cfg, depth + 1, out);
        }
    }

    if (v44_execute_action(act, out, &agg) != 0) {
        v44_proxy_response_free(out);
        return -1;
    }
    return 0;
}
