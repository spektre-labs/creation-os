/*
 * v221 σ-Language — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "language.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v221_init(cos_v221_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x221A50C1ULL;
    s->weight_sem  = 0.35f;
    s->weight_imp  = 0.35f;
    s->weight_dsc  = 0.30f;
    s->delta_calib = 0.08f;
}

/* Fixture: 10 utterances across 4 languages, balanced so
 * per-language σ_lang means land within ±Δ_calib of the
 * global mean.  Picked_reading == intended_reading for
 * every utterance (the model is calibrated). */
typedef struct {
    int lang;
    const char *text;
    int n_readings;
    int intended;
    int picked;
    int implicature;    /* bool */
    int prior, current;
    int referent;       /* bool */
} ufx_t;

static const ufx_t UFX[COS_V221_N_UTTERANCES] = {
    /* EN */
    { COS_V221_LANG_EN, "can_you_pass_the_salt", 2, 1, 1, 1, 10, 10, 1 },
    { COS_V221_LANG_EN, "nice_weather_today",    2, 1, 1, 1, 11, 11, 1 },
    { COS_V221_LANG_EN, "the_book_is_red",       1, 0, 0, 0, 12, 12, 1 },
    /* FI */
    { COS_V221_LANG_FI, "olisiko_suolaa",         2, 1, 1, 1, 10, 10, 1 },
    { COS_V221_LANG_FI, "kiitos_avusta",          1, 0, 0, 0, 13, 13, 1 },
    /* JA */
    { COS_V221_LANG_JA, "yoroshiku_onegaishimasu",2, 1, 1, 1, 14, 14, 1 },
    { COS_V221_LANG_JA, "hon_o_yomimasu",         1, 0, 0, 0, 15, 15, 1 },
    /* ES */
    { COS_V221_LANG_ES, "puedes_pasarme_la_sal",  2, 1, 1, 1, 10, 10, 1 },
    { COS_V221_LANG_ES, "buenos_dias",            2, 1, 1, 1, 16, 16, 1 },
    { COS_V221_LANG_ES, "el_libro_es_rojo",       1, 0, 0, 0, 12, 12, 1 }
};

void cos_v221_build(cos_v221_state_t *s) {
    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i) {
        cos_v221_utterance_t *u = &s->utterances[i];
        memset(u, 0, sizeof(*u));
        u->id                = i;
        u->lang              = UFX[i].lang;
        strncpy(u->text, UFX[i].text, COS_V221_STR_MAX - 1);
        u->n_readings        = UFX[i].n_readings;
        u->intended_reading  = UFX[i].intended;
        u->picked_reading    = UFX[i].picked;
        u->is_implicature    = UFX[i].implicature != 0;
        u->prior_topic_id    = UFX[i].prior;
        u->current_topic_id  = UFX[i].current;
        u->referent_resolved = UFX[i].referent != 0;
    }
}

void cos_v221_run(cos_v221_state_t *s) {
    uint64_t prev = 0x221D11A1ULL;

    s->n_implicatures_caught = 0;
    s->n_discourse_coherent  = 0;

    float per_lang_sum[COS_V221_N_LANGS] = {0};
    int   per_lang_n  [COS_V221_N_LANGS] = {0};
    float global_sum = 0.0f;

    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i) {
        cos_v221_utterance_t *u = &s->utterances[i];

        /* σ_semantic */
        float ss = (u->n_readings > 0)
            ? 1.0f - 1.0f / (float)u->n_readings
            : 0.0f;
        if (ss < 0.0f) ss = 0.0f;
        if (ss > 1.0f) ss = 1.0f;
        u->sigma_semantic = ss;

        /* σ_implicature */
        float si;
        int picked_ok = (u->picked_reading == u->intended_reading);
        if (u->is_implicature) {
            si = picked_ok ? 0.05f : 0.65f;
            if (picked_ok) s->n_implicatures_caught++;
        } else {
            si = picked_ok ? 0.05f : 0.55f;
        }
        u->sigma_implicature = si;

        /* σ_discourse */
        int coherent = (u->current_topic_id == u->prior_topic_id) &&
                       u->referent_resolved;
        float sd = coherent ? 0.05f : 0.55f;
        u->sigma_discourse = sd;
        if (coherent) s->n_discourse_coherent++;

        u->sigma_lang = s->weight_sem * ss
                       + s->weight_imp * si
                       + s->weight_dsc * sd;
        if (u->sigma_lang < 0.0f) u->sigma_lang = 0.0f;
        if (u->sigma_lang > 1.0f) u->sigma_lang = 1.0f;

        per_lang_sum[u->lang] += u->sigma_lang;
        per_lang_n  [u->lang] += 1;
        global_sum             += u->sigma_lang;

        u->hash_prev = prev;
        struct { int id, lang, nr, ir, pr, imp, pt, ct, ref;
                 float ss, si, sd, sl; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = u->id;
        rec.lang = u->lang;
        rec.nr   = u->n_readings;
        rec.ir   = u->intended_reading;
        rec.pr   = u->picked_reading;
        rec.imp  = u->is_implicature ? 1 : 0;
        rec.pt   = u->prior_topic_id;
        rec.ct   = u->current_topic_id;
        rec.ref  = u->referent_resolved ? 1 : 0;
        rec.ss   = ss;
        rec.si   = si;
        rec.sd   = sd;
        rec.sl   = u->sigma_lang;
        rec.prev = prev;
        u->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = u->hash_curr;
    }

    s->global_mean = global_sum / (float)COS_V221_N_UTTERANCES;
    s->lang_calibrated = true;
    for (int L = 0; L < COS_V221_N_LANGS; ++L) {
        s->per_lang_mean[L] = per_lang_n[L] > 0
            ? per_lang_sum[L] / (float)per_lang_n[L]
            : s->global_mean;  /* empty language → calibrated */
        if (per_lang_n[L] > 0 &&
            fabsf(s->per_lang_mean[L] - s->global_mean) > s->delta_calib)
            s->lang_calibrated = false;
    }
    s->terminal_hash = prev;

    uint64_t v = 0x221D11A1ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i) {
        const cos_v221_utterance_t *u = &s->utterances[i];
        if (u->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, lang, nr, ir, pr, imp, pt, ct, ref;
                 float ss, si, sd, sl; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = u->id;
        rec.lang = u->lang;
        rec.nr   = u->n_readings;
        rec.ir   = u->intended_reading;
        rec.pr   = u->picked_reading;
        rec.imp  = u->is_implicature ? 1 : 0;
        rec.pt   = u->prior_topic_id;
        rec.ct   = u->current_topic_id;
        rec.ref  = u->referent_resolved ? 1 : 0;
        rec.ss   = u->sigma_semantic;
        rec.si   = u->sigma_implicature;
        rec.sd   = u->sigma_discourse;
        rec.sl   = u->sigma_lang;
        rec.prev = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != u->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v221_to_json(const cos_v221_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v221\","
        "\"n_utterances\":%d,\"n_langs\":%d,"
        "\"weights\":[%.3f,%.3f,%.3f],"
        "\"delta_calib\":%.3f,"
        "\"n_implicatures_caught\":%d,"
        "\"n_discourse_coherent\":%d,"
        "\"global_mean\":%.4f,"
        "\"per_lang_mean\":[%.4f,%.4f,%.4f,%.4f],"
        "\"lang_calibrated\":%s,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"utterances\":[",
        COS_V221_N_UTTERANCES, COS_V221_N_LANGS,
        s->weight_sem, s->weight_imp, s->weight_dsc,
        s->delta_calib,
        s->n_implicatures_caught, s->n_discourse_coherent,
        s->global_mean,
        s->per_lang_mean[0], s->per_lang_mean[1],
        s->per_lang_mean[2], s->per_lang_mean[3],
        s->lang_calibrated ? "true" : "false",
        s->chain_valid     ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i) {
        const cos_v221_utterance_t *u = &s->utterances[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"lang\":%d,\"text\":\"%s\","
            "\"n_readings\":%d,\"imp\":%s,"
            "\"ss\":%.3f,\"si\":%.3f,\"sd\":%.3f,\"sl\":%.3f}",
            i == 0 ? "" : ",", u->id, u->lang, u->text,
            u->n_readings, u->is_implicature ? "true" : "false",
            u->sigma_semantic, u->sigma_implicature,
            u->sigma_discourse, u->sigma_lang);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v221_self_test(void) {
    cos_v221_state_t s;
    cos_v221_init(&s, 0x221A50C1ULL);
    cos_v221_build(&s);
    cos_v221_run(&s);

    if (!s.chain_valid) return 1;

    int n_imp = 0;
    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i)
        if (s.utterances[i].is_implicature) n_imp++;
    if (s.n_implicatures_caught != n_imp) return 2;

    if (s.n_discourse_coherent < 7) return 3;

    for (int i = 0; i < COS_V221_N_UTTERANCES; ++i) {
        const cos_v221_utterance_t *u = &s.utterances[i];
        if (u->sigma_semantic    < 0.0f || u->sigma_semantic    > 1.0f) return 4;
        if (u->sigma_implicature < 0.0f || u->sigma_implicature > 1.0f) return 4;
        if (u->sigma_discourse   < 0.0f || u->sigma_discourse   > 1.0f) return 4;
        if (u->sigma_lang        < 0.0f || u->sigma_lang        > 1.0f) return 4;
        if (u->sigma_implicature > 0.10f + 1e-6f) return 5;
    }

    if (!s.lang_calibrated) return 6;
    return 0;
}
