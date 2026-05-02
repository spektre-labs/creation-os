/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/* v277 σ-Distill-Runtime — reference implementation. */

#include "distill_runtime.h"

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

static cos_v277_learn_dec_t learn_decide(float sg, float tau) {
    return (sg <= tau) ? COS_V277_LEARN : COS_V277_SKIP;
}

static cos_v277_route_t domain_route(float sd, float tau) {
    return (sd <= tau) ? COS_V277_ROUTE_LOCAL : COS_V277_ROUTE_API;
}

void cos_v277_init(cos_v277_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed;
    s->tau_learn   = 0.25f;
    s->tau_domain  = 0.30f;

    zcpy(s->teacher, sizeof(s->teacher), "api-claude");
    zcpy(s->student, sizeof(s->student), "bitnet-3B-local");

    zcpy(s->learn[0].tag, sizeof(s->learn[0].tag), "learn_lo");
    s->learn[0].sigma_teacher = 0.15f;
    s->learn[0].decision      = COS_V277_LEARN;
    zcpy(s->learn[1].tag, sizeof(s->learn[1].tag), "learn_edge");
    s->learn[1].sigma_teacher = 0.25f;
    s->learn[1].decision      = COS_V277_LEARN;
    zcpy(s->learn[2].tag, sizeof(s->learn[2].tag), "skip_hi");
    s->learn[2].sigma_teacher = 0.26f;
    s->learn[2].decision      = COS_V277_SKIP;
    zcpy(s->learn[3].tag, sizeof(s->learn[3].tag), "skip_far");
    s->learn[3].sigma_teacher = 0.90f;
    s->learn[3].decision      = COS_V277_SKIP;

    zcpy(s->domain[0].domain, sizeof(s->domain[0].domain), "law");
    s->domain[0].sigma_domain = 0.20f;
    s->domain[0].route        = COS_V277_ROUTE_LOCAL;
    zcpy(s->domain[1].domain, sizeof(s->domain[1].domain), "code");
    s->domain[1].sigma_domain = 0.35f;
    s->domain[1].route        = COS_V277_ROUTE_API;
    zcpy(s->domain[2].domain, sizeof(s->domain[2].domain), "medical");
    s->domain[2].sigma_domain = 0.30f;
    s->domain[2].route        = COS_V277_ROUTE_LOCAL;

    zcpy(s->traj[0].month, sizeof(s->traj[0].month), "month_0");
    s->traj[0].api_share   = 0.80f;
    s->traj[0].local_share = 0.20f;
    s->traj[0].cost        = 100.0f;
    zcpy(s->traj[1].month, sizeof(s->traj[1].month), "month_1");
    s->traj[1].api_share   = 0.55f;
    s->traj[1].local_share = 0.45f;
    s->traj[1].cost        = 70.0f;
    zcpy(s->traj[2].month, sizeof(s->traj[2].month), "month_3");
    s->traj[2].api_share   = 0.25f;
    s->traj[2].local_share = 0.75f;
    s->traj[2].cost        = 40.0f;
    zcpy(s->traj[3].month, sizeof(s->traj[3].month), "month_12");
    s->traj[3].api_share   = 0.08f;
    s->traj[3].local_share = 0.92f;
    s->traj[3].cost        = 15.0f;
}

void cos_v277_run(cos_v277_state_t *s) {
    uint64_t h = 0x2774414eULL ^ s->seed;

    s->pair_ok = (strcmp(s->teacher, s->student) != 0 && s->teacher[0] && s->student[0]);
    int pass_pair = s->pair_ok ? 1 : 0;
    h             = fnv1a_bytes(s->teacher, strlen(s->teacher), h);
    h             = fnv1a_bytes(s->student, strlen(s->student), h);

    s->n_learn_row_ok = 0;
    int n_learn = 0;
    int n_skip  = 0;
    for (int i = 0; i < COS_V277_N_LEARN; ++i) {
        cos_v277_learn_row_t *r = &s->learn[i];
        cos_v277_learn_dec_t  w = learn_decide(r->sigma_teacher, s->tau_learn);
        bool                  ok = (w == r->decision);
        if (ok) {
            s->n_learn_row_ok++;
        }
        if (w == COS_V277_LEARN) {
            n_learn++;
        }
        if (w == COS_V277_SKIP) {
            n_skip++;
        }
        h = fnv1a_bytes(r->tag, strlen(r->tag), h);
        h = fnv1a_bytes(&r->sigma_teacher, sizeof(r->sigma_teacher), h);
        h = fnv1a_bytes(&r->decision, sizeof(r->decision), h);
    }
    s->learn_both_ok = (n_learn >= 1) && (n_skip >= 1);
    int pass_learn   = s->n_learn_row_ok + (s->learn_both_ok ? 1 : 0);

    s->n_domain_row_ok = 0;
    int n_loc          = 0;
    int n_api          = 0;
    for (int i = 0; i < COS_V277_N_DOMAIN; ++i) {
        cos_v277_domain_row_t *r = &s->domain[i];
        cos_v277_route_t       w = domain_route(r->sigma_domain, s->tau_domain);
        bool                   ok = (w == r->route);
        if (ok) {
            s->n_domain_row_ok++;
        }
        if (w == COS_V277_ROUTE_LOCAL) {
            n_loc++;
        }
        if (w == COS_V277_ROUTE_API) {
            n_api++;
        }
        h = fnv1a_bytes(r->domain, strlen(r->domain), h);
        h = fnv1a_bytes(&r->sigma_domain, sizeof(r->sigma_domain), h);
        h = fnv1a_bytes(&r->route, sizeof(r->route), h);
    }
    s->domain_both_ok = (n_loc >= 1) && (n_api >= 1);
    int pass_domain   = s->n_domain_row_ok + (s->domain_both_ok ? 1 : 0);

    s->n_traj_row_ok = 0;
    for (int i = 0; i < COS_V277_N_TRAJ; ++i) {
        cos_v277_traj_row_t *t = &s->traj[i];
        float                sum = t->api_share + t->local_share;
        if (fabsf(sum - 1.0f) <= 1e-3f) {
            s->n_traj_row_ok++;
        }
        h = fnv1a_bytes(t->month, strlen(t->month), h);
        h = fnv1a_bytes(&t->api_share, sizeof(t->api_share), h);
        h = fnv1a_bytes(&t->local_share, sizeof(t->local_share), h);
        h = fnv1a_bytes(&t->cost, sizeof(t->cost), h);
    }

    s->traj_api_dec = true;
    s->traj_local_inc = true;
    s->traj_cost_dec = true;
    for (int i = 0; i < COS_V277_N_TRAJ - 1; ++i) {
        if (!(s->traj[i].api_share > s->traj[i + 1].api_share + 1e-5f)) {
            s->traj_api_dec = false;
        }
        if (!(s->traj[i].local_share < s->traj[i + 1].local_share - 1e-5f)) {
            s->traj_local_inc = false;
        }
        if (!(s->traj[i].cost > s->traj[i + 1].cost + 1e-5f)) {
            s->traj_cost_dec = false;
        }
    }
    s->traj_anchor_api_high = (s->traj[0].api_share >= 0.75f - 1e-5f);
    s->traj_anchor_api_low  = (s->traj[COS_V277_N_TRAJ - 1].api_share <= 0.10f + 1e-5f);

    int pass_traj = s->n_traj_row_ok + (s->traj_api_dec ? 1 : 0) + (s->traj_local_inc ? 1 : 0) +
                    (s->traj_cost_dec ? 1 : 0) + (s->traj_anchor_api_high ? 1 : 0) +
                    (s->traj_anchor_api_low ? 1 : 0);

    s->passing = pass_pair + pass_learn + pass_domain + pass_traj;
    bool closed = (s->passing == COS_V277_DENOMINATOR);
    s->sigma_distill =
        closed ? 0.0f : (1.0f - (float)s->passing / (float)COS_V277_DENOMINATOR);
    if (s->sigma_distill < 0.0f) {
        s->sigma_distill = 0.0f;
    }
    if (s->sigma_distill > 1.0f) {
        s->sigma_distill = 1.0f;
    }

    h = fnv1a_bytes(&s->tau_learn, sizeof(s->tau_learn), h);
    h = fnv1a_bytes(&s->tau_domain, sizeof(s->tau_domain), h);
    h = fnv1a_bytes(&s->passing, sizeof(s->passing), h);
    h = fnv1a_bytes(&s->sigma_distill, sizeof(s->sigma_distill), h);
    s->terminal_hash = h;
    s->chain_valid   = closed;
}

static const char *learn_str(cos_v277_learn_dec_t d) {
    return (d == COS_V277_LEARN) ? "LEARN" : "SKIP";
}

static const char *route_str(cos_v277_route_t r) {
    return (r == COS_V277_ROUTE_LOCAL) ? "LOCAL_ONLY" : "API";
}

size_t cos_v277_to_json(const cos_v277_state_t *s, char *buf, size_t cap) {
    if (!buf || cap < 256) {
        return 0;
    }
    char *w   = buf;
    char *end = buf + cap;

    int n = snprintf(
        w, (size_t)(end - w),
        "{"
        "\"kernel\":\"v277\","
        "\"teacher\":\"%s\","
        "\"student\":\"%s\","
        "\"tau_learn\":%.6g,"
        "\"tau_domain\":%.6g,"
        "\"denominator\":%d,"
        "\"passing\":%d,"
        "\"sigma_distill\":%.6g,"
        "\"chain_hash\":\"0x%016llx\","
        "\"manifest_closed\":%s,"
        "\"learn\":[",
        s->teacher, s->student, s->tau_learn, s->tau_domain, COS_V277_DENOMINATOR, s->passing,
        s->sigma_distill, (unsigned long long)s->terminal_hash,
        s->chain_valid ? "true" : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V277_N_LEARN; ++i) {
        const cos_v277_learn_row_t *r = &s->learn[i];
        cos_v277_learn_dec_t        wnt = learn_decide(r->sigma_teacher, s->tau_learn);
        bool expect_ok = (wnt == r->decision);
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"tag\":\"%s\",\"sigma_teacher\":%.6g,\"decision\":\"%s\",\"expect_ok\":%s}",
            i ? "," : "", r->tag, r->sigma_teacher, learn_str(r->decision),
            expect_ok ? "true" : "false");
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"domains\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V277_N_DOMAIN; ++i) {
        const cos_v277_domain_row_t *r = &s->domain[i];
        cos_v277_route_t               wnt = domain_route(r->sigma_domain, s->tau_domain);
        bool expect_ok = (wnt == r->route);
        n              = snprintf(
            w, (size_t)(end - w),
            "%s{\"domain\":\"%s\",\"sigma_domain\":%.6g,\"route\":\"%s\",\"expect_ok\":%s}",
            i ? "," : "", r->domain, r->sigma_domain, route_str(r->route),
            expect_ok ? "true" : "false");
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(w, (size_t)(end - w), "],\"trajectory\":[");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    for (int i = 0; i < COS_V277_N_TRAJ; ++i) {
        const cos_v277_traj_row_t *t = &s->traj[i];
        n = snprintf(
            w, (size_t)(end - w),
            "%s{\"month\":\"%s\",\"api_share\":%.6g,\"local_share\":%.6g,\"cost\":%.6g}",
            i ? "," : "", t->month, t->api_share, t->local_share, t->cost);
        if (n < 0 || (size_t)n >= (size_t)(end - w)) {
            return 0;
        }
        w += n;
    }

    n = snprintf(
        w, (size_t)(end - w),
        "],"
        "\"learn_both_ok\":%s,"
        "\"domain_both_ok\":%s,"
        "\"traj_monotone_ok\":%s"
        "}",
        s->learn_both_ok ? "true" : "false", s->domain_both_ok ? "true" : "false",
        (s->traj_api_dec && s->traj_local_inc && s->traj_cost_dec) ? "true" : "false");
    if (n < 0 || (size_t)n >= (size_t)(end - w)) {
        return 0;
    }
    w += n;

    return (size_t)(w - buf);
}

int cos_v277_self_test(void) {
    cos_v277_state_t s;
    cos_v277_init(&s, 0x277u);
    cos_v277_run(&s);
    if (!s.chain_valid || fabsf(s.sigma_distill) > 1e-5f) {
        return 1;
    }
    char   js[8192];
    size_t k = cos_v277_to_json(&s, js, sizeof(js));
    if (k == 0 || k >= sizeof(js)) {
        return 2;
    }
    return 0;
}
