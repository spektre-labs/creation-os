/*
 * v186 σ-Continual-Architecture — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "grow.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* ---- hash chain (FNV-1a over a canonical string) ---------------- */

static uint64_t fnv1a(const char *s) {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        h ^= *p;
        h *= 0x100000001B3ULL;
    }
    return h;
}

/* ---- init ------------------------------------------------------ */

void cos_v186_init(cos_v186_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed              = seed ? seed : 0x186DA710ULL;
    s->tau_growth        = 0.03f;  /* σ slope per epoch that flags starved */
    s->tau_prune_utility = 0.15f;  /* utility below this ⇒ pruned */
}

static void add_kernel(cos_v186_state_t *s, const char *name,
                       int domain, int epoch) {
    if (s->n_kernels >= COS_V186_MAX_KERNELS) return;
    cos_v186_kernel_t *k = &s->kernels[s->n_kernels];
    k->id              = s->n_kernels;
    snprintf(k->name, COS_V186_STR_MAX, "%s", name);
    k->domain          = domain;
    k->created_epoch   = epoch;
    k->active          = true;
    k->sigma_mean      = 0.30f;
    k->utility         = 0.50f;
    s->n_kernels++;
}

static void log_change(cos_v186_state_t *s, int epoch,
                       cos_v186_change_kind_t kind,
                       int kernel_id, int domain,
                       float s_before, float s_after,
                       const char *reason) {
    if (s->n_changes >= COS_V186_MAX_CHANGES) return;
    cos_v186_change_t *c = &s->changes[s->n_changes];
    c->epoch         = epoch;
    c->kind          = (int)kind;
    c->kernel_id     = kernel_id;
    c->domain        = domain;
    c->sigma_before  = s_before;
    c->sigma_after   = s_after;
    snprintf(c->reason, COS_V186_STR_MAX, "%s", reason);

    c->prev_hash = (s->n_changes == 0) ? 0ULL :
                    s->changes[s->n_changes - 1].hash;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "%d|%d|%d|%d|%.4f|%.4f|%s|%016llx",
             epoch, (int)kind, kernel_id, domain,
             s_before, s_after, reason,
             (unsigned long long)c->prev_hash);
    c->hash = fnv1a(buf);
    s->n_changes++;
}

uint64_t cos_v186_replay_chain(const cos_v186_state_t *s) {
    uint64_t last = 0ULL;
    for (int i = 0; i < s->n_changes; ++i) {
        const cos_v186_change_t *c = &s->changes[i];
        if (c->prev_hash != last) return 0ULL;    /* tamper */
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "%d|%d|%d|%d|%.4f|%.4f|%s|%016llx",
                 c->epoch, c->kind, c->kernel_id, c->domain,
                 c->sigma_before, c->sigma_after, c->reason,
                 (unsigned long long)c->prev_hash);
        if (fnv1a(buf) != c->hash) return 0ULL;   /* tamper */
        last = c->hash;
    }
    return last;
}

/* ---- per-domain σ trend ---------------------------------------- */

static float slope(const float *arr, int n) {
    /* simple least-squares slope over [0..n-1] */
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; ++i) {
        sx  += i;
        sy  += arr[i];
        sxx += (double)i * i;
        sxy += (double)i * arr[i];
    }
    double denom = n * sxx - sx * sx;
    if (fabs(denom) < 1e-12) return 0.0f;
    return (float)((n * sxy - sx * sy) / denom);
}

/* ---- main run -------------------------------------------------- */

static float domain_sigma(const cos_v186_state_t *s, int domain) {
    double sum = 0;
    int    n   = 0;
    for (int i = 0; i < s->n_kernels; ++i) {
        if (s->kernels[i].active && s->kernels[i].domain == domain) {
            sum += s->kernels[i].sigma_mean;
            n++;
        }
    }
    return n ? (float)(sum / n) : 0.50f;
}

void cos_v186_run(cos_v186_state_t *s) {
    /* Bootstrap: 6 initial kernels across 4 domains. */
    add_kernel(s, "code_gen",        0, 0);
    add_kernel(s, "math_solver",     1, 0);
    add_kernel(s, "history_recall",  2, 0);
    add_kernel(s, "plan_eval",       3, 0);
    add_kernel(s, "retrieval",       0, 0);
    add_kernel(s, "long_context",    2, 0);
    s->n_initial_kernels = s->n_kernels;

    /* Every initial kernel is logged as INIT. */
    for (int i = 0; i < s->n_kernels; ++i) {
        log_change(s, 0, COS_V186_CHANGE_INIT, i,
                   s->kernels[i].domain,
                   s->kernels[i].sigma_mean, s->kernels[i].sigma_mean,
                   s->kernels[i].name);
    }

    /* Deterministic domain σ-evolution:
     *   domain 0 (code):    drifts upward  (starved)
     *   domain 1 (math):    flat           (stable)
     *   domain 2 (history): drifts upward  (starved)
     *   domain 3 (plan):    drifts downward (over-capacity)
     */
    for (int e = 0; e < COS_V186_N_EPOCHS; ++e) {
        for (int d = 0; d < COS_V186_N_DOMAINS; ++d) {
            float base = domain_sigma(s, d);
            float drift = 0.0f;
            if (d == 0) drift = 0.05f * e;
            if (d == 2) drift = 0.04f * e;
            if (d == 3) drift = -0.03f * e;
            float v = base + drift - 0.15f;  /* start a bit lower */
            if (v < 0.02f) v = 0.02f;
            if (v > 0.95f) v = 0.95f;
            s->sigma_history[d][e] = v;
        }
    }

    /* Capacity monitor: slope > tau_growth ⇒ starved. */
    for (int d = 0; d < COS_V186_N_DOMAINS; ++d) {
        float sl = slope(s->sigma_history[d], COS_V186_N_EPOCHS);
        s->starved[d] = (sl > s->tau_growth);
    }

    /* Growth proposals for starved domains. Accept iff a newly
     * added kernel's synthetic σ is strictly below the current
     * domain σ_mean (= Δσ improvement). */
    for (int d = 0; d < COS_V186_N_DOMAINS; ++d) {
        if (!s->starved[d]) continue;
        float before = s->sigma_history[d][COS_V186_N_EPOCHS - 1];

        char name[COS_V186_STR_MAX];
        snprintf(name, COS_V186_STR_MAX, "grown_d%d", d);
        int prev_n = s->n_kernels;
        add_kernel(s, name, d, COS_V186_N_EPOCHS);
        s->n_grown++;
        cos_v186_kernel_t *k = &s->kernels[prev_n];
        /* The newly grown kernel has a lower σ_mean than the current
         * domain average (synthetic win). */
        k->sigma_mean = before * 0.60f;
        k->utility    = 1.0f - k->sigma_mean;

        float new_avg = domain_sigma(s, d);
        bool accept   = (new_avg < before);

        log_change(s, COS_V186_N_EPOCHS, COS_V186_CHANGE_GROW,
                   k->id, d, before, new_avg, name);

        if (accept) {
            s->n_accepted++;
            log_change(s, COS_V186_N_EPOCHS, COS_V186_CHANGE_ACCEPT,
                       k->id, d, before, new_avg, "delta_sigma_down");
        } else {
            /* Reject: revert activation. */
            k->active = false;
            s->n_rejected++;
            log_change(s, COS_V186_N_EPOCHS, COS_V186_CHANGE_REJECT,
                       k->id, d, before, new_avg, "no_improvement");
        }
    }

    /* Prune the weakest kernel in a non-starved domain (over-capacity). */
    for (int d = 0; d < COS_V186_N_DOMAINS; ++d) {
        if (s->starved[d]) continue;
        /* Find kernel with lowest utility in domain. */
        int   pick    = -1;
        float minu    = 1e9f;
        for (int i = 0; i < s->n_initial_kernels; ++i) {
            if (!s->kernels[i].active) continue;
            if (s->kernels[i].domain != d) continue;
            if (s->kernels[i].utility < minu) {
                minu = s->kernels[i].utility;
                pick = i;
            }
        }
        if (pick < 0) continue;
        cos_v186_kernel_t *k = &s->kernels[pick];
        /* Prune if domain was over-capacity (slope negative) or
         * utility below threshold. */
        float sl = slope(s->sigma_history[d], COS_V186_N_EPOCHS);
        if (sl < 0.0f || k->utility <= s->tau_prune_utility) {
            k->active = false;
            s->n_pruned++;
            log_change(s, COS_V186_N_EPOCHS, COS_V186_CHANGE_PRUNE,
                       k->id, d, k->sigma_mean, 1.0f,
                       "over_capacity");
            break;   /* prune once per run */
        }
    }

    /* Count final active kernels. */
    s->n_final_kernels = 0;
    for (int i = 0; i < s->n_kernels; ++i)
        if (s->kernels[i].active) s->n_final_kernels++;
}

/* ---- JSON ------------------------------------------------------ */

size_t cos_v186_to_json(const cos_v186_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v186\","
        "\"n_initial_kernels\":%d,\"n_final_kernels\":%d,"
        "\"n_grown\":%d,\"n_pruned\":%d,"
        "\"n_accepted\":%d,\"n_rejected\":%d,"
        "\"n_changes\":%d,"
        "\"tau_growth\":%.4f,\"tau_prune_utility\":%.4f,"
        "\"starved\":[%s,%s,%s,%s],"
        "\"chain_tip\":\"%016llx\","
        "\"history\":[",
        s->n_initial_kernels, s->n_final_kernels,
        s->n_grown, s->n_pruned,
        s->n_accepted, s->n_rejected,
        s->n_changes,
        s->tau_growth, s->tau_prune_utility,
        s->starved[0] ? "true" : "false",
        s->starved[1] ? "true" : "false",
        s->starved[2] ? "true" : "false",
        s->starved[3] ? "true" : "false",
        (unsigned long long)(s->n_changes ?
            s->changes[s->n_changes-1].hash : 0ULL));
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int d = 0; d < COS_V186_N_DOMAINS; ++d) {
        int k = snprintf(buf + off, cap - off,
            "%s{\"domain\":%d,\"sigma\":[%.4f,%.4f,%.4f,%.4f,%.4f,%.4f]}",
            d == 0 ? "" : ",", d,
            s->sigma_history[d][0], s->sigma_history[d][1],
            s->sigma_history[d][2], s->sigma_history[d][3],
            s->sigma_history[d][4], s->sigma_history[d][5]);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "],\"changes\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < s->n_changes; ++i) {
        const cos_v186_change_t *c = &s->changes[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"epoch\":%d,\"kind\":%d,\"kernel_id\":%d,"
            "\"domain\":%d,\"hash\":\"%016llx\"}",
            i == 0 ? "" : ",", c->epoch, c->kind, c->kernel_id,
            c->domain, (unsigned long long)c->hash);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m2 = snprintf(buf + off, cap - off, "]}");
    if (m2 < 0 || off + (size_t)m2 >= cap) return 0;
    return off + (size_t)m2;
}

/* ---- self-test ------------------------------------------------- */

int cos_v186_self_test(void) {
    cos_v186_state_t s;
    cos_v186_init(&s, 0x186DA710ULL);
    cos_v186_run(&s);

    if (s.n_initial_kernels != 6) return 1;
    if (s.n_grown < 1)             return 2;
    if (s.n_accepted < 1)          return 3;
    if (s.n_pruned < 1)            return 4;
    if (s.n_final_kernels == s.n_initial_kernels) return 5;

    /* At least one starved domain must have been found. */
    int starved = 0;
    for (int d = 0; d < COS_V186_N_DOMAINS; ++d)
        if (s.starved[d]) starved++;
    if (starved < 1) return 6;

    /* Replay chain must verify. */
    uint64_t tip = cos_v186_replay_chain(&s);
    if (tip == 0ULL) return 7;
    if (tip != s.changes[s.n_changes - 1].hash) return 8;

    /* Tamper detection: mutate a middle entry → replay must fail. */
    cos_v186_change_t save = s.changes[s.n_changes / 2];
    s.changes[s.n_changes / 2].sigma_after += 0.10f;
    uint64_t tamper = cos_v186_replay_chain(&s);
    s.changes[s.n_changes / 2] = save;
    if (tamper != 0ULL) return 9;

    return 0;
}
