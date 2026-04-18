/*
 * v196 σ-Habit — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "habit.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

void cos_v196_init(cos_v196_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x196A812ULL;
    s->tau_repeat  = 5.0f;
    s->tau_break   = 0.40f;
    s->min_speedup = 10.0f;
    s->n_ticks     = COS_V196_N_TRACE;
}

void cos_v196_build(cos_v196_state_t *s) {
    static const struct { const char *name; int occ; int full; int comp; float sig; }
    CAT[COS_V196_N_PATTERNS] = {
        { "weather_check",         12, 2400,  48, 0.08f },  /* habit */
        { "code_review",            9, 3600, 120, 0.10f },  /* habit */
        { "daily_summary",          7, 3000, 150, 0.12f },  /* habit */
        { "merge_gate_run",         6, 2800,  70, 0.09f },  /* habit */
        { "novel_proof_attempt",    2, 8000, 800, 0.55f },  /* NOT habit */
        { "ad_hoc_exploration",     3, 5000, 400, 0.42f },  /* NOT habit */
        { "debug_unique_crash",     1, 9000, 900, 0.60f },  /* NOT habit */
        { "rare_schema_migrate",    4, 6000, 500, 0.35f }   /* NOT habit */
    };

    s->n_patterns = COS_V196_N_PATTERNS;
    s->n_habits   = 0;
    uint64_t prev = 0x196CE23ULL;
    for (int i = 0; i < COS_V196_N_PATTERNS; ++i) {
        cos_v196_pattern_t *p = &s->patterns[i];
        memset(p, 0, sizeof(*p));
        strncpy(p->name, CAT[i].name, COS_V196_STR_MAX - 1);
        p->occurrences      = CAT[i].occ;
        p->cycles_reasoning = CAT[i].full;
        p->cycles_compiled  = CAT[i].comp;
        p->speedup          = (float)CAT[i].full / (float)CAT[i].comp;
        p->sigma_steady     = CAT[i].sig;
        p->is_habit         = ((float)p->occurrences >= s->tau_repeat) &&
                              (p->speedup >= s->min_speedup) &&
                              (p->sigma_steady < s->tau_break);
        if (p->is_habit) s->n_habits++;

        struct { int i, occ, full, comp; float sig; uint64_t prev; }
            rec = { i, p->occurrences, p->cycles_reasoning,
                     p->cycles_compiled, p->sigma_steady, prev };
        p->habit_hash = fnv1a(&rec, sizeof(rec), prev);
        prev          = p->habit_hash;
    }
    s->terminal_hash = prev;
}

void cos_v196_run(cos_v196_state_t *s) {
    /* Deterministic 32-tick trace:
     *    t  0..23 : drive habit patterns 0..3 round-robin with
     *               σ = p->sigma_steady (all < τ_break) → cerebellum
     *    t 24..27 : inject σ spike on pattern 0 (σ = 0.70) →
     *               break_out, cortex mode, cycles_reasoning used
     *    t 28..31 : return to steady state on remaining habits
     */
    uint64_t prev = s->terminal_hash;
    s->n_break_outs = 0;

    for (int t = 0; t < s->n_ticks; ++t) {
        cos_v196_tick_t *tk = &s->trace[t];
        memset(tk, 0, sizeof(*tk));
        tk->t = t;

        if (t < 24) {
            int pid = t % 4;           /* habits 0..3 */
            cos_v196_pattern_t *p = &s->patterns[pid];
            tk->pattern_id = pid;
            tk->sigma      = p->sigma_steady;
            tk->mode       = COS_V196_MODE_CEREBELLUM;
            tk->cycles     = p->cycles_compiled;
        } else if (t < 28) {
            /* σ spike → break out of habit 0. */
            int pid = 0;
            cos_v196_pattern_t *p = &s->patterns[pid];
            tk->pattern_id = pid;
            tk->sigma      = 0.70f;
            tk->mode       = COS_V196_MODE_CORTEX;
            tk->cycles     = p->cycles_reasoning;
            tk->break_out  = true;
            s->n_break_outs++;
        } else {
            int pid = 1 + ((t - 28) % 3);
            cos_v196_pattern_t *p = &s->patterns[pid];
            tk->pattern_id = pid;
            tk->sigma      = p->sigma_steady;
            tk->mode       = COS_V196_MODE_CEREBELLUM;
            tk->cycles     = p->cycles_compiled;
        }
    }

    /* Chain verify (run rebuilds prev). */
    uint64_t v = 0x196CE23ULL;
    s->chain_valid = true;
    for (int i = 0; i < s->n_patterns; ++i) {
        const cos_v196_pattern_t *p = &s->patterns[i];
        struct { int i, occ, full, comp; float sig; uint64_t prev; }
            rec = { i, p->occurrences, p->cycles_reasoning,
                     p->cycles_compiled, p->sigma_steady, v };
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != p->habit_hash) { s->chain_valid = false; break; }
        v = h;
    }
    if (v != s->terminal_hash) s->chain_valid = false;
    (void)prev;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v196_to_json(const cos_v196_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v196\","
        "\"n_patterns\":%d,\"n_habits\":%d,"
        "\"tau_repeat\":%.2f,\"tau_break\":%.2f,"
        "\"min_speedup\":%.2f,"
        "\"n_ticks\":%d,\"n_break_outs\":%d,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"patterns\":[",
        s->n_patterns, s->n_habits, s->tau_repeat, s->tau_break,
        s->min_speedup, s->n_ticks, s->n_break_outs,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < s->n_patterns; ++i) {
        const cos_v196_pattern_t *p = &s->patterns[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"occ\":%d,\"is_habit\":%s,"
            "\"cycles_reasoning\":%d,\"cycles_compiled\":%d,"
            "\"speedup\":%.2f,\"sigma_steady\":%.3f}",
            i == 0 ? "" : ",", p->name, p->occurrences,
            p->is_habit ? "true" : "false",
            p->cycles_reasoning, p->cycles_compiled,
            p->speedup, p->sigma_steady);
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int t2 = snprintf(buf + off, cap - off, "],\"trace\":[");
    if (t2 < 0 || off + (size_t)t2 >= cap) return 0;
    off += (size_t)t2;
    for (int t = 0; t < s->n_ticks; ++t) {
        const cos_v196_tick_t *tk = &s->trace[t];
        int k = snprintf(buf + off, cap - off,
            "%s{\"t\":%d,\"pid\":%d,\"sigma\":%.3f,"
            "\"mode\":%d,\"cycles\":%d,\"break_out\":%s}",
            t == 0 ? "" : ",", tk->t, tk->pattern_id, tk->sigma,
            tk->mode, tk->cycles, tk->break_out ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v196_self_test(void) {
    cos_v196_state_t s;
    cos_v196_init(&s, 0x196A812ULL);
    cos_v196_build(&s);
    cos_v196_run(&s);

    if (s.n_habits < 3)             return 1;
    /* Every habit ≥ 10× speedup. */
    for (int i = 0; i < s.n_patterns; ++i) {
        const cos_v196_pattern_t *p = &s.patterns[i];
        if (!p->is_habit) continue;
        if (!(p->speedup >= s.min_speedup)) return 2;
        if (!(p->sigma_steady < s.tau_break)) return 3;
    }
    if (s.n_break_outs < 1)         return 4;
    if (!s.chain_valid)             return 5;

    /* Steady-state ticks (not break-out) run in cerebellum. */
    for (int t = 0; t < s.n_ticks; ++t) {
        const cos_v196_tick_t *tk = &s.trace[t];
        if (tk->break_out) {
            if (tk->mode != COS_V196_MODE_CORTEX) return 6;
            if (tk->sigma < s.tau_break)          return 7;
        } else {
            if (tk->mode != COS_V196_MODE_CEREBELLUM) return 8;
            if (!(tk->sigma < s.tau_break))            return 9;
        }
    }
    return 0;
}
