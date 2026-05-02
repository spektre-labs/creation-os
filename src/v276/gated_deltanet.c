/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * v276 σ-Gated-DeltaNet — deterministic manifest + σ_deltanet closure (offline).
 */

#include "gated_deltanet.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define COS_V276_DENOMINATOR 18

static void zcpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) {
        dst[n] = src[n];
    }
    dst[n] = '\0';
}

static uint64_t fnv1a_bytes(const void *data, size_t n, uint64_t h) {
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static cos_v276_dec_t gate_decide(float sg, float tau) {
    return (sg <= tau) ? COS_V276_DEC_LINEAR : COS_V276_DEC_FALLBACK_FULL;
}

static cos_v276_back_t argmin_backend(float sm, float sd, float st) {
    float m   = sm;
    int   idx = 0;
    if (sd < m) {
        m   = sd;
        idx = 1;
    }
    if (st < m) {
        idx = 2;
    }
    return (cos_v276_back_t)(idx + 1);
}

static float sigma_for_back(cos_v276_back_t b, float sm, float sd, float st) {
    switch (b) {
        case COS_V276_BACK_MAMBA:
            return sm;
        case COS_V276_BACK_DELTANET:
            return sd;
        case COS_V276_BACK_TTT:
            return st;
        default:
            return 1.0f;
    }
}

static float rival_sigma(cos_v276_back_t ch, float sm, float sd, float st) {
    float r = 1.0f;
    if (ch != COS_V276_BACK_MAMBA) {
        r = fminf(r, sm);
    }
    if (ch != COS_V276_BACK_DELTANET) {
        r = fminf(r, sd);
    }
    if (ch != COS_V276_BACK_TTT) {
        r = fminf(r, st);
    }
    return r;
}

void cos_v276_init(cos_v276_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed     = seed;
    s->tau_gate = 0.35f;

    zcpy(s->backends[0].name, sizeof(s->backends[0].name), "deltanet");
    s->backends[0].exponent      = 1;
    s->backends[0].gate_mechanism = true;
    s->backends[0].throughput_rel = 2.0f;

    zcpy(s->backends[1].name, sizeof(s->backends[1].name), "transformer");
    s->backends[1].exponent       = 2;
    s->backends[1].gate_mechanism = false;
    s->backends[1].throughput_rel = 1.0f;

    s->gate[0].token_id    = 1;
    s->gate[0].sigma_gate  = 0.20f;
    s->gate[0].decision    = COS_V276_DEC_LINEAR;
    s->gate[1].token_id    = 2;
    s->gate[1].sigma_gate  = 0.50f;
    s->gate[1].decision    = COS_V276_DEC_FALLBACK_FULL;
    s->gate[2].token_id    = 3;
    s->gate[2].sigma_gate  = 0.35f;
    s->gate[2].decision    = COS_V276_DEC_LINEAR;
    s->gate[3].token_id    = 4;
    s->gate[3].sigma_gate  = 0.36f;
    s->gate[3].decision    = COS_V276_DEC_FALLBACK_FULL;

    zcpy(s->combo[0].component, sizeof(s->combo[0].component), "deltanet");
    s->combo[0].enabled    = true;
    s->combo[0].layer_slot = 1;
    zcpy(s->combo[1].component, sizeof(s->combo[1].component), "ttt");
    s->combo[1].enabled    = true;
    s->combo[1].layer_slot = 2;
    zcpy(s->combo[2].component, sizeof(s->combo[2].component), "sigma_gate");
    s->combo[2].enabled    = true;
    s->combo[2].layer_slot = 3;

    zcpy(s->tasks[0].label, sizeof(s->tasks[0].label), "long_ctx_routing");
    s->tasks[0].sigma_mamba    = 0.50f;
    s->tasks[0].sigma_deltanet = 0.20f;
    s->tasks[0].sigma_ttt      = 0.40f;
    s->tasks[0].chosen         = COS_V276_BACK_DELTANET;
    s->tasks[0].sigma_chosen   = 0.20f;
    s->tasks[0].sigma_rival    = 0.40f;

    zcpy(s->tasks[1].label, sizeof(s->tasks[1].label), "chat_short");
    s->tasks[1].sigma_mamba    = 0.10f;
    s->tasks[1].sigma_deltanet = 0.30f;
    s->tasks[1].sigma_ttt      = 0.40f;
    s->tasks[1].chosen         = COS_V276_BACK_MAMBA;
    s->tasks[1].sigma_chosen   = 0.10f;
    s->tasks[1].sigma_rival    = 0.30f;

    zcpy(s->tasks[2].label, sizeof(s->tasks[2].label), "code_edit");
    s->tasks[2].sigma_mamba    = 0.40f;
    s->tasks[2].sigma_deltanet = 0.50f;
    s->tasks[2].sigma_ttt      = 0.15f;
    s->tasks[2].chosen         = COS_V276_BACK_TTT;
    s->tasks[2].sigma_chosen   = 0.15f;
    s->tasks[2].sigma_rival    = 0.40f;
}

void cos_v276_run(cos_v276_state_t *s) {
    uint64_t h = 0x27676134ULL ^ s->seed;

    /* --- backends (weight 2 + 1 + 1) --- */
    s->n_backend_rows_ok = 0;
    if (s->backends[0].exponent == 1 && s->backends[0].gate_mechanism) {
        s->n_backend_rows_ok++;
    }
    if (s->backends[1].exponent == 2 && !s->backends[1].gate_mechanism) {
        s->n_backend_rows_ok++;
    }
    s->backend_exponents_ok =
        (s->backends[0].exponent == 1 && s->backends[1].exponent == 2);
    s->backend_throughput_ok =
        (s->backends[0].throughput_rel > s->backends[1].throughput_rel);

    int pass_back = s->n_backend_rows_ok + (s->backend_exponents_ok ? 1 : 0) +
                    (s->backend_throughput_ok ? 1 : 0);
    h = fnv1a_bytes(s->backends[0].name, strlen(s->backends[0].name), h);
    h = fnv1a_bytes(s->backends[1].name, strlen(s->backends[1].name), h);

    /* --- σ-gate fixtures (weight 4 + 1) --- */
    s->n_gate_rows_ok  = 0;
    s->n_gate_linear   = 0;
    s->n_gate_fallback = 0;
    for (int i = 0; i < COS_V276_N_GATE; ++i) {
        cos_v276_dec_t want = gate_decide(s->gate[i].sigma_gate, s->tau_gate);
        bool           ok   = (s->gate[i].decision == want);
        if (ok) {
            s->n_gate_rows_ok++;
        }
        if (want == COS_V276_DEC_LINEAR) {
            s->n_gate_linear++;
        }
        if (want == COS_V276_DEC_FALLBACK_FULL) {
            s->n_gate_fallback++;
        }
        h = fnv1a_bytes(&s->gate[i].token_id, sizeof(s->gate[i].token_id), h);
        h = fnv1a_bytes(&s->gate[i].sigma_gate, sizeof(s->gate[i].sigma_gate), h);
        h = fnv1a_bytes(&s->gate[i].decision, sizeof(s->gate[i].decision), h);
    }
    bool gate_both_ok = (s->n_gate_linear >= 1) && (s->n_gate_fallback >= 1);
    int  pass_gate    = s->n_gate_rows_ok + (gate_both_ok ? 1 : 0);

    /* --- combo (weight 3 + 1) --- */
    s->n_combo_rows_ok = 0;
    for (int i = 0; i < COS_V276_N_COMBO; ++i) {
        if (s->combo[i].enabled && s->combo[i].layer_slot == i + 1) {
            s->n_combo_rows_ok++;
        }
        h = fnv1a_bytes(s->combo[i].component, strlen(s->combo[i].component), h);
        h = fnv1a_bytes(&s->combo[i].layer_slot, sizeof(s->combo[i].layer_slot), h);
    }
    s->combo_order_ok =
        (strncmp(s->combo[0].component, "deltanet", 14) == 0 && s->combo[0].layer_slot == 1 &&
         strncmp(s->combo[1].component, "ttt", 14) == 0 && s->combo[1].layer_slot == 2 &&
         strncmp(s->combo[2].component, "sigma_gate", 14) == 0 &&
         s->combo[2].layer_slot == 3);
    int pass_combo = s->n_combo_rows_ok + (s->combo_order_ok ? 1 : 0);

    /* --- tri-backend tasks (weight 3 + 1 + 1) --- */
    s->n_task_rows_ok    = 0;
    s->n_task_chosen_ok  = 0;
    s->n_distinct_chosen = 0;
    bool seen[4] = {false, false, false, false};

    for (int i = 0; i < COS_V276_N_TASKS; ++i) {
        cos_v276_task_t *t = &s->tasks[i];
        cos_v276_back_t  want_ch =
            argmin_backend(t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt);
        float want_sigma =
            sigma_for_back(want_ch, t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt);
        float want_rival = rival_sigma(want_ch, t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt);

        bool row_ok = (t->chosen == want_ch) && (fabsf(t->sigma_chosen - want_sigma) < 1e-4f) &&
                      (fabsf(t->sigma_rival - want_rival) < 1e-4f);
        if (row_ok) {
            s->n_task_rows_ok++;
        }
        if (t->sigma_chosen <= t->sigma_rival + 1e-4f) {
            s->n_task_chosen_ok++;
        }
        if ((int)t->chosen >= 1 && (int)t->chosen <= 3) {
            seen[(int)t->chosen] = true;
        }

        h = fnv1a_bytes(t->label, strlen(t->label), h);
        h = fnv1a_bytes(&t->sigma_mamba, sizeof(t->sigma_mamba), h);
        h = fnv1a_bytes(&t->sigma_deltanet, sizeof(t->sigma_deltanet), h);
        h = fnv1a_bytes(&t->sigma_ttt, sizeof(t->sigma_ttt), h);
        h = fnv1a_bytes(&t->chosen, sizeof(t->chosen), h);
    }

    int distinct = 0;
    for (int b = 1; b <= 3; ++b) {
        if (seen[b]) {
            distinct++;
        }
    }
    s->n_distinct_chosen   = distinct;
    bool task_diversity_ok = distinct >= 2;
    bool task_rule_closed  = (s->n_task_chosen_ok == COS_V276_N_TASKS);

    int pass_task =
        s->n_task_rows_ok + (task_rule_closed ? 1 : 0) + (task_diversity_ok ? 1 : 0);

    s->passing = pass_back + pass_gate + pass_combo + pass_task;
    bool closed = (s->passing == COS_V276_DENOMINATOR);
    s->sigma_deltanet =
        closed ? 0.0f : (1.0f - (float)s->passing / (float)COS_V276_DENOMINATOR);
    if (s->sigma_deltanet < 0.0f) {
        s->sigma_deltanet = 0.0f;
    }
    if (s->sigma_deltanet > 1.0f) {
        s->sigma_deltanet = 1.0f;
    }

    h = fnv1a_bytes(&s->tau_gate, sizeof(s->tau_gate), h);
    h = fnv1a_bytes(&s->passing, sizeof(s->passing), h);
    h = fnv1a_bytes(&s->sigma_deltanet, sizeof(s->sigma_deltanet), h);
    s->terminal_hash = h;
    s->chain_valid   = closed;
}

static const char *dec_str(cos_v276_dec_t d) {
    return (d == COS_V276_DEC_LINEAR) ? "LINEAR" : "FALLBACK_FULL";
}

static const char *back_str(cos_v276_back_t b) {
    switch (b) {
        case COS_V276_BACK_MAMBA:
            return "mamba";
        case COS_V276_BACK_DELTANET:
            return "deltanet";
        case COS_V276_BACK_TTT:
            return "ttt";
        default:
            return "unknown";
    }
}

size_t cos_v276_to_json(const cos_v276_state_t *s, char *buf, size_t cap) {
    if (!buf || cap < 256) {
        return 0;
    }
    char *w   = buf;
    char *end = buf + cap;
    int   n   = snprintf(
        w, (size_t)(end - w),
        "{"
        "\"kernel\":\"v276\","
        "\"tau_gate\":%.6g,"
        "\"denominator\":%d,"
        "\"passing\":%d,"
        "\"sigma_deltanet\":%.6g,"
        "\"chain_hash\":\"0x%016llx\","
        "\"manifest_closed\":%s,"
        "\"backends\":[",
        s->tau_gate, COS_V276_DENOMINATOR, s->passing, s->sigma_deltanet,
        (unsigned long long)s->terminal_hash, s->chain_valid ? "true" : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V276_N_BACKENDS; ++i) {
        const cos_v276_backend_t *b = &s->backends[i];
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"name\":\"%s\",\"exponent\":%d,\"gate_mechanism\":%s,\"throughput_rel\":%.g}",
            i ? "," : "", b->name, b->exponent, b->gate_mechanism ? "true" : "false",
            (double)b->throughput_rel);
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"gates\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V276_N_GATE; ++i) {
        cos_v276_dec_t want = gate_decide(s->gate[i].sigma_gate, s->tau_gate);
        bool           ok   = (s->gate[i].decision == want);
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"token_id\":%d,\"sigma_gate\":%.6g,\"decision\":\"%s\",\"expect_ok\":%s}",
            i ? "," : "", s->gate[i].token_id, s->gate[i].sigma_gate,
            dec_str(s->gate[i].decision), ok ? "true" : "false");
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"combo\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V276_N_COMBO; ++i) {
        const cos_v276_combo_t *c = &s->combo[i];
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"component\":\"%s\",\"enabled\":%s,\"layer_slot\":%d}", i ? "," : "",
            c->component, c->enabled ? "true" : "false", c->layer_slot);
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"tasks\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V276_N_TASKS; ++i) {
        const cos_v276_task_t *t = &s->tasks[i];
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"label\":\"%s\",\"sigma_mamba\":%.6g,\"sigma_deltanet\":%.6g,"
            "\"sigma_ttt\":%.6g,\"chosen\":\"%s\",\"sigma_chosen\":%.6g,\"sigma_rival\":%.6g}",
            i ? "," : "", t->label, t->sigma_mamba, t->sigma_deltanet, t->sigma_ttt,
            back_str(t->chosen), t->sigma_chosen, t->sigma_rival);
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(
        w, (size_t)(end - w),
        "],"
        "\"n_backend_rows_ok\":%d,"
        "\"n_gate_rows_ok\":%d,"
        "\"n_combo_rows_ok\":%d,"
        "\"n_task_rows_ok\":%d,"
        "\"n_distinct_chosen\":%d,"
        "\"task_chosen_ok\":%s"
        "}",
        s->n_backend_rows_ok, s->n_gate_rows_ok, s->n_combo_rows_ok, s->n_task_rows_ok,
        s->n_distinct_chosen, (s->n_task_chosen_ok == COS_V276_N_TASKS) ? "true" : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    return (size_t)(w - buf);
}

int cos_v276_self_test(void) {
    cos_v276_state_t s;
    cos_v276_init(&s, 0x276u);
    cos_v276_run(&s);
    if (!s.chain_valid || fabsf(s.sigma_deltanet) > 1e-5f) {
        return 1;
    }
    char js[8192];
    size_t k = cos_v276_to_json(&s, js, sizeof(js));
    if (k == 0 || k >= sizeof(js)) {
        return 2;
    }
    return 0;
}
