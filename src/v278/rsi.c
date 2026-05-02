/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* v278 reference implementation. */

#include "rsi.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

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

static cos_v278_godel_action_t godel_decide(float sg, float tau) {
    return (sg <= tau) ? COS_V278_SELF_CONFIDENT : COS_V278_CALL_PROCONDUCTOR;
}

void cos_v278_init(cos_v278_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed;
    s->tau_goedel = 0.40f;

    s->calibration_err[0] = 0.42f;
    s->calibration_err[1] = 0.28f;
    s->calibration_err[2] = 0.11f;
    s->calibration_err[3] = 0.04f;

    s->arch[0].n_channels = 6;
    s->arch[0].aurcc     = 0.72f;
    s->arch[0].chosen    = false;
    s->arch[1].n_channels = 8;
    s->arch[1].aurcc     = 0.88f;
    s->arch[1].chosen    = true;
    s->arch[2].n_channels = 12;
    s->arch[2].aurcc     = 0.71f;
    s->arch[2].chosen    = false;

    zcpy(s->tau[0].domain, sizeof(s->tau[0].domain), "code");
    s->tau[0].tau = 0.20f;
    zcpy(s->tau[1].domain, sizeof(s->tau[1].domain), "creative");
    s->tau[1].tau = 0.50f;
    zcpy(s->tau[2].domain, sizeof(s->tau[2].domain), "medical");
    s->tau[2].tau = 0.15f;

    zcpy(s->goedel[0].tag, sizeof(s->goedel[0].tag), "g0");
    s->goedel[0].sigma_goedel = 0.25f;
    s->goedel[0].action       = COS_V278_SELF_CONFIDENT;
    zcpy(s->goedel[1].tag, sizeof(s->goedel[1].tag), "g1");
    s->goedel[1].sigma_goedel = 0.40f;
    s->goedel[1].action       = COS_V278_SELF_CONFIDENT;
    zcpy(s->goedel[2].tag, sizeof(s->goedel[2].tag), "g2");
    s->goedel[2].sigma_goedel = 0.75f;
    s->goedel[2].action       = COS_V278_CALL_PROCONDUCTOR;
}

void cos_v278_run(cos_v278_state_t *s) {
    uint64_t h = 0x2787273ULL ^ s->seed;

    float *e = s->calibration_err;
    s->cal_cmp_01   = (e[0] > e[1] + 1e-5f);
    s->cal_cmp_12   = (e[1] > e[2] + 1e-5f);
    s->cal_cmp_23   = (e[2] > e[3] + 1e-5f);
    s->cal_last_ok  = (e[3] <= 0.05f + 1e-5f);
    s->cal_span_ok  = ((e[0] - e[3]) > 0.10f);
    s->cal_range_ok = true;
    for (int i = 0; i < COS_V278_N_CALIB; ++i) {
        if (!(e[i] > 0.0f && e[i] < 1.0f)) {
            s->cal_range_ok = false;
        }
        h = fnv1a_bytes(&e[i], sizeof(e[i]), h);
    }
    int pass_cal = (s->cal_cmp_01 ? 1 : 0) + (s->cal_cmp_12 ? 1 : 0) + (s->cal_cmp_23 ? 1 : 0) +
                   (s->cal_last_ok ? 1 : 0) + (s->cal_span_ok ? 1 : 0) + (s->cal_range_ok ? 1 : 0);

    const int expect_n[COS_V278_N_ARCH] = {6, 8, 12};
    s->n_arch_row_ok = 0;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        if (s->arch[i].n_channels == expect_n[i]) {
            s->n_arch_row_ok++;
        }
        h = fnv1a_bytes(&s->arch[i].n_channels, sizeof(s->arch[i].n_channels), h);
        h = fnv1a_bytes(&s->arch[i].aurcc, sizeof(s->arch[i].aurcc), h);
        h = fnv1a_bytes(&s->arch[i].chosen, sizeof(s->arch[i].chosen), h);
    }
    int argmax_i = 0;
    for (int i = 1; i < COS_V278_N_ARCH; ++i) {
        if (s->arch[i].aurcc > s->arch[argmax_i].aurcc + 1e-5f) {
            argmax_i = i;
        }
    }
    int n_chosen = 0;
    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        if (s->arch[i].chosen) {
            n_chosen++;
        }
    }
    s->arch_argmax_ok = (n_chosen == 1) && s->arch[argmax_i].chosen;

    float a0 = s->arch[0].aurcc;
    float a1 = s->arch[1].aurcc;
    float a2 = s->arch[2].aurcc;
    int   distinct_a = 1;
    if (fabsf(a1 - a0) > 1e-4f) {
        distinct_a++;
    }
    if (fabsf(a2 - a0) > 1e-4f && fabsf(a2 - a1) > 1e-4f) {
        distinct_a++;
    }
    s->arch_aurcc_diverse = (distinct_a >= 2);

    int pass_arch = s->n_arch_row_ok + (s->arch_argmax_ok ? 1 : 0) + (s->arch_aurcc_diverse ? 1 : 0);

    struct {
        const char *dom;
        float       t;
    } expect_tau[COS_V278_N_TAU] = {{"code", 0.20f}, {"creative", 0.50f}, {"medical", 0.15f}};
    s->n_tau_row_ok = 0;
    s->tau_open_interval = true;
    for (int i = 0; i < COS_V278_N_TAU; ++i) {
        if (strcmp(s->tau[i].domain, expect_tau[i].dom) == 0 &&
            fabsf(s->tau[i].tau - expect_tau[i].t) < 1e-4f) {
            s->n_tau_row_ok++;
        }
        float t = s->tau[i].tau;
        if (!(t > 0.0f && t < 1.0f)) {
            s->tau_open_interval = false;
        }
        h = fnv1a_bytes(s->tau[i].domain, strlen(s->tau[i].domain), h);
        h = fnv1a_bytes(&s->tau[i].tau, sizeof(s->tau[i].tau), h);
    }
    float t0 = s->tau[0].tau;
    float t1 = s->tau[1].tau;
    float t2 = s->tau[2].tau;
    int   distinct_t = 1;
    if (fabsf(t1 - t0) > 1e-4f) {
        distinct_t++;
    }
    if (fabsf(t2 - t0) > 1e-4f && fabsf(t2 - t1) > 1e-4f) {
        distinct_t++;
    }
    s->tau_distinct_ok = distinct_t >= 2;
    int pass_tau       = s->n_tau_row_ok + (s->tau_open_interval ? 1 : 0) + (s->tau_distinct_ok ? 1 : 0);

    s->n_godel_row_ok = 0;
    int n_conf        = 0;
    int n_call        = 0;
    for (int i = 0; i < COS_V278_N_GOEDEL; ++i) {
        cos_v278_godel_row_t *r = &s->goedel[i];
        cos_v278_godel_action_t w = godel_decide(r->sigma_goedel, s->tau_goedel);
        if (w == r->action) {
            s->n_godel_row_ok++;
        }
        if (w == COS_V278_SELF_CONFIDENT) {
            n_conf++;
        }
        if (w == COS_V278_CALL_PROCONDUCTOR) {
            n_call++;
        }
        h = fnv1a_bytes(r->tag, strlen(r->tag), h);
        h = fnv1a_bytes(&r->sigma_goedel, sizeof(r->sigma_goedel), h);
        h = fnv1a_bytes(&r->action, sizeof(r->action), h);
    }
    s->godel_both_ok = (n_conf >= 1) && (n_call >= 1);
    s->godel_tau_ok  = fabsf(s->tau_goedel - 0.40f) < 1e-4f;
    int pass_godel =
        s->n_godel_row_ok + (s->godel_both_ok ? 1 : 0) + (s->godel_tau_ok ? 1 : 0);

    s->passing = pass_cal + pass_arch + pass_tau + pass_godel;
    bool closed = (s->passing == COS_V278_DENOMINATOR);
    s->sigma_rsi = closed ? 0.0f : (1.0f - (float)s->passing / (float)COS_V278_DENOMINATOR);
    if (s->sigma_rsi < 0.0f) {
        s->sigma_rsi = 0.0f;
    }
    if (s->sigma_rsi > 1.0f) {
        s->sigma_rsi = 1.0f;
    }

    h = fnv1a_bytes(&s->tau_goedel, sizeof(s->tau_goedel), h);
    h = fnv1a_bytes(&s->passing, sizeof(s->passing), h);
    h = fnv1a_bytes(&s->sigma_rsi, sizeof(s->sigma_rsi), h);
    s->terminal_hash = h;
    s->chain_valid   = closed;
}

static const char *godel_act_str(cos_v278_godel_action_t a) {
    return (a == COS_V278_SELF_CONFIDENT) ? "SELF_CONFIDENT" : "CALL_PROCONDUCTOR";
}

size_t cos_v278_to_json(const cos_v278_state_t *s, char *buf, size_t cap) {
    if (!buf || cap < 256) {
        return 0;
    }
    char *w   = buf;
    char *end = buf + cap;
    int   n =
        snprintf(w, (size_t)(end - w),
                 "{"
                 "\"kernel\":\"v278\","
                 "\"tau_goedel\":%.6g,"
                 "\"denominator\":%d,"
                 "\"passing\":%d,"
                 "\"sigma_rsi\":%.6g,"
                 "\"chain_hash\":\"0x%016llx\","
                 "\"manifest_closed\":%s,"
                 "\"calibration_err\":[%.6g,%.6g,%.6g,%.6g],"
                 "\"calibration_ok\":%s,"
                 "\"arch\":[",
                 s->tau_goedel, COS_V278_DENOMINATOR, s->passing, s->sigma_rsi,
                 (unsigned long long)s->terminal_hash, s->chain_valid ? "true" : "false",
                 s->calibration_err[0], s->calibration_err[1], s->calibration_err[2],
                 s->calibration_err[3],
                 (s->cal_cmp_01 && s->cal_cmp_12 && s->cal_cmp_23 && s->cal_last_ok && s->cal_span_ok &&
                  s->cal_range_ok)
                     ? "true"
                     : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V278_N_ARCH; ++i) {
        const cos_v278_arch_row_t *a = &s->arch[i];
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"n_channels\":%d,\"aurcc\":%.6g,\"chosen\":%s}", i ? "," : "", a->n_channels,
            a->aurcc, a->chosen ? "true" : "false");
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"thresholds\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V278_N_TAU; ++i) {
        const cos_v278_tau_row_t *t = &s->tau[i];
        n = snprintf(w, (size_t)(end - w), "%s{\"domain\":\"%s\",\"tau\":%.6g}", i ? "," : "",
                     t->domain, t->tau);
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"goedel\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V278_N_GOEDEL; ++i) {
        const cos_v278_godel_row_t *g = &s->goedel[i];
        cos_v278_godel_action_t       wnt = godel_decide(g->sigma_goedel, s->tau_goedel);
        bool expect_ok = (wnt == g->action);
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"tag\":\"%s\",\"sigma_goedel\":%.6g,\"action\":\"%s\",\"expect_ok\":%s}",
            i ? "," : "", g->tag, g->sigma_goedel, godel_act_str(g->action),
            expect_ok ? "true" : "false");
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(
        w, (size_t)(end - w),
        "],"
        "\"arch_argmax_ok\":%s,"
        "\"tau_distinct_ok\":%s,"
        "\"godel_both_ok\":%s"
        "}",
        s->arch_argmax_ok ? "true" : "false", s->tau_distinct_ok ? "true" : "false",
        s->godel_both_ok ? "true" : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;
    return (size_t)(w - buf);
}

int cos_v278_self_test(void) {
    cos_v278_state_t s;
    cos_v278_init(&s, 0x278u);
    cos_v278_run(&s);
    if (!s.chain_valid || fabsf(s.sigma_rsi) > 1e-5f) {
        return 1;
    }
    char   js[8192];
    size_t k = cos_v278_to_json(&s, js, sizeof(js));
    if (k == 0 || k >= sizeof(js)) {
        return 2;
    }
    return 0;
}
