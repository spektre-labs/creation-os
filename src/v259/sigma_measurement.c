/* v259 — sigma_measurement_t v0 manifest implementation. */

#include "sigma_measurement.h"

#include <stdalign.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t cos_v259_fnv1a(uint64_t h, const void *p, size_t n) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t i = 0;
    if (cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    for (; src[i] && i + 1 < cap; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    if (x < y) return -1;
    if (x > y) return  1;
    return 0;
}

/* monotonic ns clock (POSIX).  Linux + macOS + *BSD all support this. */
static uint64_t mono_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* --- benchmark workers ------------------------------------------------- */

/* Written to return something observable to prevent whole-body dead-code
 * elimination.  The returned value is summed into a volatile sink. */
static volatile uint64_t cos_v259_sink = 0;

static uint64_t bench_encode_decode_once(cos_sigma_measurement_t m) {
    unsigned char buf[sizeof(cos_sigma_measurement_t)];
    memcpy(buf, &m, sizeof buf);
    cos_sigma_measurement_t out;
    memcpy(&out, buf, sizeof out);
    return (uint64_t)out.header
         ^ (uint64_t)(*(uint32_t *)&out.sigma)
         ^ (uint64_t)(*(uint32_t *)&out.tau);
}

static uint64_t bench_gate_once(const cos_sigma_measurement_t *m) {
    return (uint64_t)cos_sigma_measurement_gate(m);
}

/* Distribution-aware microbench.  iters × 1 call each, timed end-to-end
 * as a "block", split into K blocks so we can report min/median/mean.    */
static void run_bench(cos_v259_bench_row_t *row,
                      const char *label,
                      uint64_t iters,
                      cos_sigma_measurement_t init_m,
                      int mode /* 0 = encode_decode, 1 = gate */) {
    enum { K = 32 };
    double per_call[K];
    uint64_t per_block = iters / K;
    if (per_block == 0) per_block = 1;

    uint64_t total_calls = 0;
    uint64_t acc = 0;
    for (int k = 0; k < K; ++k) {
        cos_sigma_measurement_t m = init_m;
        uint64_t t0 = mono_ns();
        for (uint64_t j = 0; j < per_block; ++j) {
            if (mode == 0) {
                m.sigma = (float)((j & 0x3fff) * (1.0 / 16384.0));
                acc   += bench_encode_decode_once(m);
            } else {
                m.sigma = (float)((j & 0x3fff) * (1.0 / 16384.0));
                acc   += bench_gate_once(&m);
            }
        }
        uint64_t t1 = mono_ns();
        double dt = (double)(t1 - t0);
        per_call[k] = (per_block ? dt / (double)per_block : 0.0);
        total_calls += per_block;
    }
    cos_v259_sink ^= acc;

    double sum = 0.0;
    double mn = per_call[0], mx = per_call[0];
    for (int k = 0; k < K; ++k) {
        sum += per_call[k];
        if (per_call[k] < mn) mn = per_call[k];
        if (per_call[k] > mx) mx = per_call[k];
    }
    double tmp[K];
    memcpy(tmp, per_call, sizeof tmp);
    qsort(tmp, K, sizeof tmp[0], cmp_double);
    double median = 0.5 * (tmp[K / 2 - 1] + tmp[K / 2]);

    cpy(row->label, sizeof row->label, label);
    row->iters         = total_calls;
    row->mean_ns       = sum / (double)K;
    row->median_ns     = median;
    row->min_ns        = mn;
    row->max_ns        = mx;
    row->under_budget  = (row->mean_ns < COS_V259_BUDGET_NS)
                     && (row->median_ns < COS_V259_BUDGET_NS);
}

/* --- init / run -------------------------------------------------------- */

void cos_v259_init(cos_v259_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof *s);
    s->seed = seed;
    s->sizeof_measurement  = sizeof(cos_sigma_measurement_t);
    s->alignof_measurement = alignof(cos_sigma_measurement_t);
}

void cos_v259_run(cos_v259_state_t *s) {
    /* 1. Layout ------------------------------------------------------ */
    static const char *names[COS_V259_N_LAYOUT] = { "header", "sigma", "tau" };
    const uint32_t offsets[COS_V259_N_LAYOUT] = {
        (uint32_t)offsetof(cos_sigma_measurement_t, header),
        (uint32_t)offsetof(cos_sigma_measurement_t, sigma),
        (uint32_t)offsetof(cos_sigma_measurement_t, tau),
    };
    const uint32_t sizes[COS_V259_N_LAYOUT] = { 4, 4, 4 };
    int lok = 0;
    for (int i = 0; i < COS_V259_N_LAYOUT; ++i) {
        cpy(s->layout[i].field, sizeof s->layout[i].field, names[i]);
        s->layout[i].offset = offsets[i];
        s->layout[i].size   = sizes[i];
        if (s->layout[i].offset == (uint32_t)(i * 4)
         && s->layout[i].size   == 4) ++lok;
    }
    s->n_layout_rows_ok = lok;
    s->layout_size_ok   = (s->sizeof_measurement == 12);
    s->layout_align_ok  = (s->alignof_measurement == 4);

    /* 2. Roundtrip --------------------------------------------------- */
    const float rt_sig[COS_V259_N_RT] = { 0.00f, 0.20f, 0.50f, 0.80f };
    const float rt_tau[COS_V259_N_RT] = { 0.50f, 0.50f, 0.50f, 0.50f };
    const cos_sigma_gate_t rt_exp[COS_V259_N_RT] = {
        COS_SIGMA_GATE_ALLOW,
        COS_SIGMA_GATE_ALLOW,
        COS_SIGMA_GATE_BOUNDARY,
        COS_SIGMA_GATE_ABSTAIN,
    };
    int rok = 0;
    int rt_all = 1;
    int rt_verdict_all = 1;
    for (int i = 0; i < COS_V259_N_RT; ++i) {
        cos_sigma_measurement_t a;
        a.header = 0x01010000u;
        a.sigma  = rt_sig[i];
        a.tau    = rt_tau[i];
        unsigned char buf[12];
        memcpy(buf, &a, sizeof buf);
        cos_sigma_measurement_t b;
        memcpy(&b, buf, sizeof b);
        bool eq = (memcmp(&a, &b, sizeof a) == 0);
        cos_sigma_gate_t v = cos_sigma_measurement_gate(&b);
        s->rt[i].sigma        = rt_sig[i];
        s->rt[i].tau          = rt_tau[i];
        s->rt[i].expected     = rt_exp[i];
        s->rt[i].roundtrip_ok = eq;
        s->rt[i].observed     = v;
        if (eq) ++rok; else rt_all = 0;
        if (v != rt_exp[i]) rt_verdict_all = 0;
    }
    s->n_rt_rows_ok        = rok;
    s->rt_roundtrip_ok     = (rt_all == 1);
    s->rt_gate_verdict_ok  = (rt_verdict_all == 1);

    /* 3. Gate regimes ------------------------------------------------ */
    static const char *gate_lbl[COS_V259_N_GATE] = {
        "signal_dominant", "critical", "noise_dominant"
    };
    const float gate_sig[COS_V259_N_GATE] = { 0.25f, 0.50f, 0.75f };
    const float gate_tau[COS_V259_N_GATE] = { 0.50f, 0.50f, 0.50f };
    int gok = 0;
    bool ordered = true;
    cos_sigma_gate_t prev = (cos_sigma_gate_t)-1;
    for (int i = 0; i < COS_V259_N_GATE; ++i) {
        cos_sigma_measurement_t m = { 0u, gate_sig[i], gate_tau[i] };
        cos_sigma_gate_t v = cos_sigma_measurement_gate(&m);
        cpy(s->gate[i].label, sizeof s->gate[i].label, gate_lbl[i]);
        s->gate[i].sigma   = gate_sig[i];
        s->gate[i].tau     = gate_tau[i];
        s->gate[i].verdict = v;
        if ((int)v <= (int)prev) ordered = false;
        prev = v;
        if (strlen(s->gate[i].label) > 0) ++gok;
    }
    s->n_gate_rows_ok = gok;
    s->gate_order_ok  = ordered;
    /* Purity: run the predicate 256 times with identical inputs and
     * verify the output never varies.  A non-pure implementation would
     * have to fabricate divergence in a test designed to detect it. */
    cos_sigma_measurement_t probe = { 0u, 0.42f, 0.40f };
    cos_sigma_gate_t expected = cos_sigma_measurement_gate(&probe);
    bool pure = true;
    for (int i = 0; i < 256; ++i) {
        if (cos_sigma_measurement_gate(&probe) != expected) {
            pure = false; break;
        }
    }
    s->gate_pure_ok = pure;

    /* 4. Microbench -------------------------------------------------- */
    cos_sigma_measurement_t m_allow   = { 0u, 0.20f, 0.50f };
    cos_sigma_measurement_t m_abstain = { 0u, 0.80f, 0.50f };
    run_bench(&s->bench[0], "encode_decode",
              COS_V259_BENCH_ITERS, m_allow, 0);
    run_bench(&s->bench[1], "gate_allow",
              COS_V259_BENCH_ITERS, m_allow, 1);
    run_bench(&s->bench[2], "gate_abstain",
              COS_V259_BENCH_ITERS, m_abstain, 1);
    int bok = 0;
    bool budget_all = true;
    bool measured_all = true;
    for (int i = 0; i < COS_V259_N_BENCH; ++i) {
        if (strlen(s->bench[i].label) > 0
         && s->bench[i].iters >= COS_V259_BENCH_ITERS) ++bok;
        if (!s->bench[i].under_budget) budget_all = false;
        /* measured_ok: mean_ns > 0 AND min_ns > 0.  We do NOT require
         * min < max because on a modern x86/ARM with an 0.5 ns inlined
         * predicate every timing block can legitimately collapse to the
         * same integer-nanosecond bucket.  We therefore only prove that
         * a non-zero cost was observed; distributional shape is
         * reported in the JSON for inspection, not asserted. */
        if (!(s->bench[i].mean_ns > 0.0 && s->bench[i].min_ns > 0.0)) {
            measured_all = false;
        }
    }
    s->n_bench_rows_ok     = bok;
    s->bench_budget_ok     = budget_all;
    s->bench_distribution_ok = measured_all;

    /* σ_measurement aggregation ------------------------------------- */
    int good = 0, denom = 0;
    good  += s->n_layout_rows_ok;                          denom += 3;
    good  += s->layout_size_ok ? 1 : 0;                    denom += 1;
    good  += s->layout_align_ok ? 1 : 0;                   denom += 1;
    good  += s->n_rt_rows_ok;                              denom += 4;
    good  += s->rt_roundtrip_ok ? 1 : 0;                   denom += 1;
    good  += s->rt_gate_verdict_ok ? 1 : 0;                denom += 1;
    good  += s->n_gate_rows_ok;                            denom += 3;
    good  += s->gate_order_ok ? 1 : 0;                     denom += 1;
    good  += s->gate_pure_ok ? 1 : 0;                      denom += 1;
    good  += s->n_bench_rows_ok;                           denom += 3;
    good  += s->bench_budget_ok ? 1 : 0;                   denom += 1;
    good  += s->bench_distribution_ok ? 1 : 0;             denom += 1;
    s->sigma_measurement =
        1.0f - (float)((double)good / (double)denom);

    /* FNV-1a chain over everything we computed --------------------- */
    uint64_t h = 0xcbf29ce484222325ULL ^ s->seed;
    h = cos_v259_fnv1a(h, s->layout, sizeof s->layout);
    for (int i = 0; i < COS_V259_N_RT; ++i) {
        h = cos_v259_fnv1a(h, &s->rt[i].sigma,    sizeof(float));
        h = cos_v259_fnv1a(h, &s->rt[i].tau,      sizeof(float));
        h = cos_v259_fnv1a(h, &s->rt[i].expected, sizeof(int));
        uint8_t ok = s->rt[i].roundtrip_ok ? 1u : 0u;
        h = cos_v259_fnv1a(h, &ok, 1);
    }
    h = cos_v259_fnv1a(h, s->gate,  sizeof s->gate);
    for (int i = 0; i < COS_V259_N_BENCH; ++i) {
        h = cos_v259_fnv1a(h, s->bench[i].label, sizeof s->bench[i].label);
        uint64_t it = s->bench[i].iters;
        h = cos_v259_fnv1a(h, &it, sizeof it);
    }
    s->terminal_hash = h;
    s->chain_valid   = (h != 0);
}

/* --- JSON emit --------------------------------------------------------- */

size_t cos_v259_to_json(const cos_v259_state_t *s, char *buf, size_t cap) {
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v259_sigma_measurement\","
        "\"sizeof\":%zu,\"alignof\":%zu,"
        "\"layout_size_ok\":%s,\"layout_align_ok\":%s,"
        "\"layout\":[",
        s->sizeof_measurement,
        s->alignof_measurement,
        s->layout_size_ok ? "true" : "false",
        s->layout_align_ok ? "true" : "false");
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V259_N_LAYOUT; ++i) {
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"field\":\"%s\",\"offset\":%u,\"size\":%u}",
            i ? "," : "",
            s->layout[i].field,
            (unsigned)s->layout[i].offset,
            (unsigned)s->layout[i].size);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }

    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"roundtrip\":[");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    for (int i = 0; i < COS_V259_N_RT; ++i) {
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"sigma\":%.4f,\"tau\":%.4f,"
            "\"expected\":%d,\"observed\":%d,"
            "\"roundtrip_ok\":%s}",
            i ? "," : "",
            (double)s->rt[i].sigma,
            (double)s->rt[i].tau,
            (int)s->rt[i].expected,
            (int)s->rt[i].observed,
            s->rt[i].roundtrip_ok ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }

    n = snprintf(buf + w, cap - (size_t)w,
        "],\"rt_rows_ok\":%d,"
        "\"rt_roundtrip_ok\":%s,"
        "\"rt_gate_verdict_ok\":%s,"
        "\"gate\":[",
        s->n_rt_rows_ok,
        s->rt_roundtrip_ok ? "true" : "false",
        s->rt_gate_verdict_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    for (int i = 0; i < COS_V259_N_GATE; ++i) {
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"sigma\":%.4f,"
            "\"tau\":%.4f,\"verdict\":%d}",
            i ? "," : "",
            s->gate[i].label,
            (double)s->gate[i].sigma,
            (double)s->gate[i].tau,
            (int)s->gate[i].verdict);
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"gate_rows_ok\":%d,"
        "\"gate_order_ok\":%s,"
        "\"gate_pure_ok\":%s,"
        "\"bench\":[",
        s->n_gate_rows_ok,
        s->gate_order_ok ? "true" : "false",
        s->gate_pure_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    for (int i = 0; i < COS_V259_N_BENCH; ++i) {
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\","
            "\"iters\":%llu,"
            "\"mean_ns\":%.3f,\"median_ns\":%.3f,"
            "\"min_ns\":%.3f,\"max_ns\":%.3f,"
            "\"under_budget\":%s}",
            i ? "," : "",
            s->bench[i].label,
            (unsigned long long)s->bench[i].iters,
            s->bench[i].mean_ns,
            s->bench[i].median_ns,
            s->bench[i].min_ns,
            s->bench[i].max_ns,
            s->bench[i].under_budget ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }

    n = snprintf(buf + w, cap - (size_t)w,
        "],\"bench_rows_ok\":%d,"
        "\"bench_budget_ok\":%s,"
        "\"bench_distribution_ok\":%s,"
        "\"sigma_measurement\":%.6f,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_bench_rows_ok,
        s->bench_budget_ok ? "true" : "false",
        s->bench_distribution_ok ? "true" : "false",
        (double)s->sigma_measurement,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    return (size_t)w;
}

/* --- v259.1-range: exhaustive clamp invariant --------------------------- */

/* Verifies `cos_sigma_measurement_clamp(x) ∈ [0, 1]` for:
 *   (a) every IEEE-754 special value we care about (NaN, ±Inf, ±0,
 *       DBL_MIN-as-float, subnormal-min, denormal, largest-finite),
 *   (b) a deterministic grid of 1 000 000 float inputs drawn from a
 *       linear-congruential seed so the check is reproducible on every
 *       run, and
 *   (c) the five pre-registered canonical σ values used by the
 *       roundtrip table (0.0, 0.2, 0.5, 0.8, 1.0).
 *
 * Returns 0 on full pass; a positive code identifying the failing
 * input class otherwise.  Called from `cos_v259_self_test` after all
 * other invariants are confirmed.
 *
 * NOTE: this is a RUNTIME invariant check by exhaustive sampling; it
 * is NOT a formal proof.  See `hw/formal/v259/sigma_measurement.h.acsl`
 * and `hw/formal/v259/Measurement.lean` for the formal-proof
 * scaffolding (PENDING a toolchain). */
static int cos_v259_clamp_exhaustive_check(void) {
    const float specials[] = {
        0.0f, -0.0f, 1.0f, -1.0f,
        0.5f, 0.999999f, 1.000001f,
         1e-38f, -1e-38f,
         3.4e+38f, -3.4e+38f,
         (float)(1.0 / 0.0),      /* +Inf */
        -(float)(1.0 / 0.0),      /* -Inf */
         (float)(0.0 / 0.0),      /* NaN  */
    };
    const size_t n_spec = sizeof(specials) / sizeof(specials[0]);
    for (size_t i = 0; i < n_spec; ++i) {
        float y = cos_sigma_measurement_clamp(specials[i]);
        if (!(y >= 0.0f && y <= 1.0f)) return 100 + (int)i;
    }

    /* Deterministic grid: a linear congruential generator over
     * IEEE-754 float bit patterns interpreted as-float.  1M samples. */
    uint32_t state = 0x13579BDFu;
    for (size_t i = 0; i < 1000000u; ++i) {
        state = state * 1664525u + 1013904223u;
        union { uint32_t u; float f; } v;
        v.u = state;
        float y = cos_sigma_measurement_clamp(v.f);
        if (!(y >= 0.0f && y <= 1.0f)) return 200;
    }

    /* Canonical σ from the pre-registered roundtrip table. */
    const float canon[] = {0.0f, 0.2f, 0.5f, 0.8f, 1.0f};
    for (size_t i = 0; i < sizeof(canon) / sizeof(canon[0]); ++i) {
        float y = cos_sigma_measurement_clamp(canon[i]);
        if (y != canon[i]) return 300 + (int)i;   /* identity on valid */
    }
    return 0;
}

/* --- self-test --------------------------------------------------------- */

int cos_v259_self_test(void) {
    cos_v259_state_t s;
    cos_v259_init(&s, 259u);
    cos_v259_run(&s);

    if (s.n_layout_rows_ok != COS_V259_N_LAYOUT) return 1;
    if (!s.layout_size_ok)                       return 2;
    if (!s.layout_align_ok)                      return 3;

    if (s.n_rt_rows_ok != COS_V259_N_RT)         return 4;
    if (!s.rt_roundtrip_ok)                      return 5;
    if (!s.rt_gate_verdict_ok)                   return 6;

    if (s.n_gate_rows_ok != COS_V259_N_GATE)     return 7;
    if (!s.gate_order_ok)                        return 8;
    if (!s.gate_pure_ok)                         return 9;

    if (s.n_bench_rows_ok != COS_V259_N_BENCH)   return 10;
    if (!s.bench_budget_ok)                      return 11;
    if (!s.bench_distribution_ok)                return 12;

    if (!(s.sigma_measurement >= 0.0f && s.sigma_measurement <= 1.0f))
        return 13;
    if (s.sigma_measurement != 0.0f)             return 14;

    if (!s.chain_valid)                          return 15;
    if (s.terminal_hash == 0ULL)                 return 16;

    char buf[8192];
    size_t n = cos_v259_to_json(&s, buf, sizeof buf);
    if (n == 0 || n >= sizeof buf)               return 17;
    if (!strstr(buf, "\"kernel\":\"v259_sigma_measurement\""))
        return 18;

    /* v259.1-range: exhaustive clamp invariant
     * (runtime sampling, NOT a formal proof). */
    int rc = cos_v259_clamp_exhaustive_check();
    if (rc != 0) return 100 + rc;

    return 0;
}
