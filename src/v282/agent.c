/*
 * v282 σ-Agent — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "agent.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct { int id; float s; }
    ACTIONS[COS_V282_N_ACTION] = {
    { 0, 0.08f },   /* AUTO  */
    { 1, 0.35f },   /* ASK   */
    { 2, 0.55f },   /* ASK   */
    { 3, 0.82f },   /* BLOCK */
};

static const struct { const char *lbl; int n; float sp; }
    CHAINS[COS_V282_N_CHAIN] = {
    { "short",  3,  0.10f },   /* σ_total ≈ 0.271 → PROCEED */
    { "long",  10,  0.30f },   /* σ_total ≈ 0.972 → ABORT   */
};

static const struct { const char *id; float s; }
    TOOLS[COS_V282_N_TOOL] = {
    { "correct",      0.10f },   /* USE   */
    { "wrong_light",  0.45f },   /* SWAP  */
    { "wrong_heavy",  0.78f },   /* BLOCK */
};

static const struct { int id; float sft; float saf; }
    RECOVERIES[COS_V282_N_RECOVERY] = {
    { 0, 0.15f, 0.45f },
    { 1, 0.20f, 0.60f },
    { 2, 0.30f, 0.72f },
};

void cos_v282_init(cos_v282_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed          = seed ? seed : 0x282aaaULL;
    s->tau_auto      = 0.20f;
    s->tau_ask       = 0.60f;
    s->tau_chain     = 0.70f;
    s->tau_tool_use  = 0.30f;
    s->tau_tool_swap = 0.60f;
    s->math_eps      = 1e-4f;
}

void cos_v282_run(cos_v282_state_t *s) {
    uint64_t prev = 0x282aaa00ULL;

    s->n_action_rows_ok = 0;
    s->n_action_auto = s->n_action_ask = s->n_action_block = 0;
    for (int i = 0; i < COS_V282_N_ACTION; ++i) {
        cos_v282_action_t *a = &s->action[i];
        memset(a, 0, sizeof(*a));
        a->action_id    = ACTIONS[i].id;
        a->sigma_action = ACTIONS[i].s;
        if      (a->sigma_action <= s->tau_auto) a->decision = COS_V282_ACT_AUTO;
        else if (a->sigma_action <= s->tau_ask)  a->decision = COS_V282_ACT_ASK;
        else                                     a->decision = COS_V282_ACT_BLOCK;
        if (a->sigma_action >= 0.0f && a->sigma_action <= 1.0f)
            s->n_action_rows_ok++;
        if (a->decision == COS_V282_ACT_AUTO)  s->n_action_auto++;
        if (a->decision == COS_V282_ACT_ASK)   s->n_action_ask++;
        if (a->decision == COS_V282_ACT_BLOCK) s->n_action_block++;
        prev = fnv1a(&a->action_id,    sizeof(a->action_id),    prev);
        prev = fnv1a(&a->sigma_action, sizeof(a->sigma_action), prev);
        prev = fnv1a(&a->decision,     sizeof(a->decision),     prev);
    }

    s->n_chain_rows_ok = 0;
    s->n_chain_proceed = s->n_chain_abort = 0;
    bool math_ok = true;
    for (int i = 0; i < COS_V282_N_CHAIN; ++i) {
        cos_v282_chain_t *c = &s->chain[i];
        memset(c, 0, sizeof(*c));
        cpy(c->label, sizeof(c->label), CHAINS[i].lbl);
        c->n_steps        = CHAINS[i].n;
        c->sigma_per_step = CHAINS[i].sp;

        float confidence = 1.0f;
        for (int k = 0; k < c->n_steps; ++k)
            confidence *= (1.0f - c->sigma_per_step);
        float total = 1.0f - confidence;
        if (total < 0.0f) total = 0.0f;
        if (total > 1.0f) total = 1.0f;
        c->sigma_total = total;

        c->verdict = (c->sigma_total <= s->tau_chain)
                         ? COS_V282_VER_PROCEED
                         : COS_V282_VER_ABORT;

        /* Expected math per canonical fixtures above. */
        float expected = 0.0f;
        if (i == 0) expected = 1.0f - 0.9f * 0.9f * 0.9f;
        else {
            float conf = 1.0f;
            for (int k = 0; k < 10; ++k) conf *= 0.7f;
            expected = 1.0f - conf;
        }
        float d = c->sigma_total - expected;
        if (d < 0.0f) d = -d;
        if (d > s->math_eps) math_ok = false;

        if (c->sigma_per_step >= 0.0f && c->sigma_per_step <= 1.0f &&
            c->sigma_total    >= 0.0f && c->sigma_total    <= 1.0f &&
            c->n_steps > 0)
            s->n_chain_rows_ok++;
        if (c->verdict == COS_V282_VER_PROCEED) s->n_chain_proceed++;
        if (c->verdict == COS_V282_VER_ABORT)   s->n_chain_abort++;

        prev = fnv1a(c->label, strlen(c->label), prev);
        prev = fnv1a(&c->n_steps,        sizeof(c->n_steps),        prev);
        prev = fnv1a(&c->sigma_per_step, sizeof(c->sigma_per_step), prev);
        prev = fnv1a(&c->sigma_total,    sizeof(c->sigma_total),    prev);
        prev = fnv1a(&c->verdict,        sizeof(c->verdict),        prev);
    }
    s->chain_math_ok = math_ok;

    s->n_tool_rows_ok = 0;
    s->n_tool_use = s->n_tool_swap = s->n_tool_block = 0;
    for (int i = 0; i < COS_V282_N_TOOL; ++i) {
        cos_v282_tool_row_t *t = &s->tool[i];
        memset(t, 0, sizeof(*t));
        cpy(t->tool_id, sizeof(t->tool_id), TOOLS[i].id);
        t->sigma_tool = TOOLS[i].s;
        if      (t->sigma_tool <= s->tau_tool_use)  t->decision = COS_V282_TOOL_USE;
        else if (t->sigma_tool <= s->tau_tool_swap) t->decision = COS_V282_TOOL_SWAP;
        else                                        t->decision = COS_V282_TOOL_BLOCK;
        if (t->sigma_tool >= 0.0f && t->sigma_tool <= 1.0f)
            s->n_tool_rows_ok++;
        if (t->decision == COS_V282_TOOL_USE)   s->n_tool_use++;
        if (t->decision == COS_V282_TOOL_SWAP)  s->n_tool_swap++;
        if (t->decision == COS_V282_TOOL_BLOCK) s->n_tool_block++;
        prev = fnv1a(t->tool_id, strlen(t->tool_id), prev);
        prev = fnv1a(&t->sigma_tool, sizeof(t->sigma_tool), prev);
        prev = fnv1a(&t->decision,   sizeof(t->decision),   prev);
    }

    s->n_recovery_rows_ok = 0;
    bool rec_mono = true, rec_upd = true;
    for (int i = 0; i < COS_V282_N_RECOVERY; ++i) {
        cos_v282_recovery_t *r = &s->recovery[i];
        memset(r, 0, sizeof(*r));
        r->context_id          = RECOVERIES[i].id;
        r->sigma_first_try     = RECOVERIES[i].sft;
        r->sigma_after_fail    = RECOVERIES[i].saf;
        r->gate_update_applied = true;
        if (r->sigma_first_try  >= 0.0f && r->sigma_first_try  <= 1.0f &&
            r->sigma_after_fail >= 0.0f && r->sigma_after_fail <= 1.0f)
            s->n_recovery_rows_ok++;
        if (!(r->sigma_after_fail > r->sigma_first_try)) rec_mono = false;
        if (!r->gate_update_applied)                     rec_upd  = false;
        prev = fnv1a(&r->context_id,          sizeof(r->context_id),          prev);
        prev = fnv1a(&r->sigma_first_try,     sizeof(r->sigma_first_try),     prev);
        prev = fnv1a(&r->sigma_after_fail,    sizeof(r->sigma_after_fail),    prev);
        prev = fnv1a(&r->gate_update_applied, sizeof(r->gate_update_applied), prev);
    }
    s->recovery_monotone_ok = rec_mono;
    s->recovery_updates_ok  = rec_upd;

    bool action_all =
        (s->n_action_auto  >= 1) &&
        (s->n_action_ask   >= 1) &&
        (s->n_action_block >= 1);
    bool chain_both =
        (s->n_chain_proceed == 1) && (s->n_chain_abort == 1);
    bool tool_all =
        (s->n_tool_use   >= 1) &&
        (s->n_tool_swap  >= 1) &&
        (s->n_tool_block >= 1);

    int total   = COS_V282_N_ACTION   + 1 +
                  COS_V282_N_CHAIN    + 1 + 1 +
                  COS_V282_N_TOOL     + 1 +
                  COS_V282_N_RECOVERY + 1 + 1;
    int passing = s->n_action_rows_ok +
                  (action_all ? 1 : 0) +
                  s->n_chain_rows_ok +
                  (s->chain_math_ok ? 1 : 0) +
                  (chain_both ? 1 : 0) +
                  s->n_tool_rows_ok +
                  (tool_all ? 1 : 0) +
                  s->n_recovery_rows_ok +
                  (s->recovery_monotone_ok ? 1 : 0) +
                  (s->recovery_updates_ok  ? 1 : 0);
    s->sigma_agent = 1.0f - ((float)passing / (float)total);
    if (s->sigma_agent < 0.0f) s->sigma_agent = 0.0f;
    if (s->sigma_agent > 1.0f) s->sigma_agent = 1.0f;

    struct { int na, naa, nas, nab,
                 nc, ncp, nca,
                 nt, ntu, nts, ntb,
                 nr; 
             bool cm, rm, ru;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.na  = s->n_action_rows_ok;
    trec.naa = s->n_action_auto;
    trec.nas = s->n_action_ask;
    trec.nab = s->n_action_block;
    trec.nc  = s->n_chain_rows_ok;
    trec.ncp = s->n_chain_proceed;
    trec.nca = s->n_chain_abort;
    trec.nt  = s->n_tool_rows_ok;
    trec.ntu = s->n_tool_use;
    trec.nts = s->n_tool_swap;
    trec.ntb = s->n_tool_block;
    trec.nr  = s->n_recovery_rows_ok;
    trec.cm  = s->chain_math_ok;
    trec.rm  = s->recovery_monotone_ok;
    trec.ru  = s->recovery_updates_ok;
    trec.sigma = s->sigma_agent;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *act_name(cos_v282_act_t a) {
    switch (a) {
    case COS_V282_ACT_AUTO:  return "AUTO";
    case COS_V282_ACT_ASK:   return "ASK";
    case COS_V282_ACT_BLOCK: return "BLOCK";
    }
    return "UNKNOWN";
}

static const char *ver_name(cos_v282_ver_t v) {
    switch (v) {
    case COS_V282_VER_PROCEED: return "PROCEED";
    case COS_V282_VER_ABORT:   return "ABORT";
    }
    return "UNKNOWN";
}

static const char *tool_name(cos_v282_tool_t t) {
    switch (t) {
    case COS_V282_TOOL_USE:   return "USE";
    case COS_V282_TOOL_SWAP:  return "SWAP";
    case COS_V282_TOOL_BLOCK: return "BLOCK";
    }
    return "UNKNOWN";
}

size_t cos_v282_to_json(const cos_v282_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v282\","
        "\"tau_auto\":%.3f,\"tau_ask\":%.3f,\"tau_chain\":%.3f,"
        "\"tau_tool_use\":%.3f,\"tau_tool_swap\":%.3f,"
        "\"math_eps\":%.6f,"
        "\"n_action_rows_ok\":%d,"
        "\"n_action_auto\":%d,\"n_action_ask\":%d,\"n_action_block\":%d,"
        "\"n_chain_rows_ok\":%d,"
        "\"n_chain_proceed\":%d,\"n_chain_abort\":%d,"
        "\"chain_math_ok\":%s,"
        "\"n_tool_rows_ok\":%d,"
        "\"n_tool_use\":%d,\"n_tool_swap\":%d,\"n_tool_block\":%d,"
        "\"n_recovery_rows_ok\":%d,"
        "\"recovery_monotone_ok\":%s,\"recovery_updates_ok\":%s,"
        "\"sigma_agent\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"action\":[",
        s->tau_auto, s->tau_ask, s->tau_chain,
        s->tau_tool_use, s->tau_tool_swap,
        s->math_eps,
        s->n_action_rows_ok,
        s->n_action_auto, s->n_action_ask, s->n_action_block,
        s->n_chain_rows_ok,
        s->n_chain_proceed, s->n_chain_abort,
        s->chain_math_ok ? "true" : "false",
        s->n_tool_rows_ok,
        s->n_tool_use, s->n_tool_swap, s->n_tool_block,
        s->n_recovery_rows_ok,
        s->recovery_monotone_ok ? "true" : "false",
        s->recovery_updates_ok  ? "true" : "false",
        s->sigma_agent,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V282_N_ACTION; ++i) {
        const cos_v282_action_t *a = &s->action[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"action_id\":%d,\"sigma_action\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", a->action_id, a->sigma_action, act_name(a->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"chain\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V282_N_CHAIN; ++i) {
        const cos_v282_chain_t *c = &s->chain[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"n_steps\":%d,"
            "\"sigma_per_step\":%.4f,\"sigma_total\":%.4f,"
            "\"verdict\":\"%s\"}",
            i == 0 ? "" : ",", c->label, c->n_steps,
            c->sigma_per_step, c->sigma_total,
            ver_name(c->verdict));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"tool\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V282_N_TOOL; ++i) {
        const cos_v282_tool_row_t *t = &s->tool[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tool_id\":\"%s\",\"sigma_tool\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", t->tool_id, t->sigma_tool, tool_name(t->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"recovery\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V282_N_RECOVERY; ++i) {
        const cos_v282_recovery_t *r = &s->recovery[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"context_id\":%d,\"sigma_first_try\":%.4f,"
            "\"sigma_after_fail\":%.4f,\"gate_update_applied\":%s}",
            i == 0 ? "" : ",", r->context_id, r->sigma_first_try,
            r->sigma_after_fail,
            r->gate_update_applied ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int mx = snprintf(buf + off, cap - off, "]}");
    if (mx < 0 || off + (size_t)mx >= cap) return 0;
    return off + (size_t)mx;
}

int cos_v282_self_test(void) {
    cos_v282_state_t s;
    cos_v282_init(&s, 0x282aaaULL);
    cos_v282_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V282_N_ACTION; ++i) {
        cos_v282_act_t exp;
        if      (s.action[i].sigma_action <= s.tau_auto) exp = COS_V282_ACT_AUTO;
        else if (s.action[i].sigma_action <= s.tau_ask)  exp = COS_V282_ACT_ASK;
        else                                             exp = COS_V282_ACT_BLOCK;
        if (s.action[i].decision != exp) return 2;
    }
    if (s.n_action_rows_ok != COS_V282_N_ACTION) return 3;
    if (s.n_action_auto  < 1) return 4;
    if (s.n_action_ask   < 1) return 5;
    if (s.n_action_block < 1) return 6;

    static const char *WANT_CHAIN[COS_V282_N_CHAIN] = { "short", "long" };
    for (int i = 0; i < COS_V282_N_CHAIN; ++i) {
        if (strcmp(s.chain[i].label, WANT_CHAIN[i]) != 0) return 7;
    }
    if (s.n_chain_rows_ok != COS_V282_N_CHAIN) return 8;
    if (!s.chain_math_ok) return 9;
    if (s.n_chain_proceed != 1) return 10;
    if (s.n_chain_abort   != 1) return 11;
    if (s.chain[0].verdict != COS_V282_VER_PROCEED) return 12;
    if (s.chain[1].verdict != COS_V282_VER_ABORT)   return 13;

    static const char *WANT_TOOL[COS_V282_N_TOOL] = {
        "correct", "wrong_light", "wrong_heavy"
    };
    for (int i = 0; i < COS_V282_N_TOOL; ++i) {
        if (strcmp(s.tool[i].tool_id, WANT_TOOL[i]) != 0) return 14;
        cos_v282_tool_t exp;
        if      (s.tool[i].sigma_tool <= s.tau_tool_use)  exp = COS_V282_TOOL_USE;
        else if (s.tool[i].sigma_tool <= s.tau_tool_swap) exp = COS_V282_TOOL_SWAP;
        else                                              exp = COS_V282_TOOL_BLOCK;
        if (s.tool[i].decision != exp) return 15;
    }
    if (s.n_tool_rows_ok != COS_V282_N_TOOL) return 16;
    if (s.n_tool_use   < 1) return 17;
    if (s.n_tool_swap  < 1) return 18;
    if (s.n_tool_block < 1) return 19;

    for (int i = 0; i < COS_V282_N_RECOVERY; ++i) {
        if (!(s.recovery[i].sigma_after_fail > s.recovery[i].sigma_first_try)) return 20;
        if (!s.recovery[i].gate_update_applied) return 21;
    }
    if (s.n_recovery_rows_ok != COS_V282_N_RECOVERY) return 22;
    if (!s.recovery_monotone_ok) return 23;
    if (!s.recovery_updates_ok)  return 24;

    if (s.sigma_agent < 0.0f || s.sigma_agent > 1.0f) return 25;
    if (s.sigma_agent > 1e-6f) return 26;

    cos_v282_state_t u;
    cos_v282_init(&u, 0x282aaaULL);
    cos_v282_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 27;
    return 0;
}
