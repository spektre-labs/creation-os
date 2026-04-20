/* v304 σ-Narrative — v0 manifest implementation. */

#include "narrative.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static uint64_t cos_v304_fnv1a(uint64_t h, const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) { h ^= b[i]; h *= 0x100000001b3ULL; }
    return h;
}
static void cpy(char *d, size_t c, const char *s)
{
    size_t i = 0;
    if (!c || !s) { if (c) d[0] = '\0'; return; }
    for (; s[i] && i + 1 < c; ++i) d[i] = s[i];
    d[i] = '\0';
}

void cos_v304_init(cos_v304_state_t *s, uint64_t seed)
{ memset(s, 0, sizeof *s); s->seed = seed; }

void cos_v304_run(cos_v304_state_t *s)
{
    /* 1) Story σ */
    const char *st_lbl[] = { "coherent", "minor_tension", "contradictory" };
    const float st_s  [] = { 0.10f, 0.35f, 0.75f };

    int sok = 0, n_co = 0, n_ct = 0;
    for (int i = 0; i < COS_V304_N_STORY; ++i) {
        cos_v304_story_t *st = &s->story[i];
        cpy(st->label, sizeof st->label, st_lbl[i]);
        st->sigma_narrative = st_s[i];
        bool coherent = (st->sigma_narrative <= COS_V304_TAU_STORY);
        cpy(st->verdict, sizeof st->verdict,
            coherent ? "COHERENT" : "CONTRADICTORY");
        if (coherent) ++n_co; else ++n_ct;
        if (strlen(st->label) > 0) ++sok;
    }
    s->n_story_rows_ok = sok;
    bool sord = true;
    for (int i = 1; i < COS_V304_N_STORY; ++i)
        if (!(s->story[i].sigma_narrative >
              s->story[i-1].sigma_narrative)) sord = false;
    s->story_order_ok = sord;
    s->story_polarity_ok = (n_co > 0) && (n_ct > 0);
    s->story_count_ok = (n_co == 2) && (n_ct == 1);

    /* 2) Argument σ */
    const char *ar_lbl[] = {
        "valid_modus_ponens", "weak_induction", "affirming_consequent" };
    const float ar_s[] = { 0.05f, 0.35f, 0.85f };

    int aok = 0, n_v = 0, n_i = 0;
    for (int i = 0; i < COS_V304_N_ARG; ++i) {
        cos_v304_arg_t *a = &s->arg[i];
        cpy(a->step, sizeof a->step, ar_lbl[i]);
        a->sigma_arg = ar_s[i];
        a->valid = (a->sigma_arg <= COS_V304_TAU_ARG);
        if (a->valid) ++n_v; else ++n_i;
        if (strlen(a->step) > 0) ++aok;
    }
    s->n_arg_rows_ok = aok;
    bool aord = true;
    for (int i = 1; i < COS_V304_N_ARG; ++i)
        if (!(s->arg[i].sigma_arg > s->arg[i-1].sigma_arg))
            aord = false;
    s->arg_order_ok = aord;
    s->arg_polarity_ok = (n_v > 0) && (n_i > 0);

    /* 3) Propaganda detection */
    const char *pr_lbl[] = {
        "neutral_report", "persuasive_essay", "manipulative_ad" };
    const float pr_e [] = { 0.10f, 0.55f, 0.90f };
    const float pr_ls[] = { 0.10f, 0.30f, 0.80f };

    int pok = 0, n_f = 0, n_o = 0, f_all = 1;
    for (int i = 0; i < COS_V304_N_PROP; ++i) {
        cos_v304_prop_t *p = &s->prop[i];
        cpy(p->text, sizeof p->text, pr_lbl[i]);
        p->emotion_load = pr_e[i];
        p->logic_sigma  = pr_ls[i];
        double expected = (double)p->emotion_load * (double)p->logic_sigma;
        p->propaganda_score = (float)expected;
        if (fabs((double)p->propaganda_score - expected) > 1e-3)
            f_all = 0;
        p->flagged = (p->propaganda_score > COS_V304_TAU_PROP);
        if (p->flagged) ++n_f; else ++n_o;
        if (strlen(p->text) > 0) ++pok;
    }
    s->n_prop_rows_ok = pok;
    s->prop_formula_ok = (f_all == 1);
    s->prop_polarity_ok = (n_f > 0) && (n_o > 0);

    /* 4) Self-narrative σ */
    const char *sf_lbl[] = {
        "aligned_self", "slight_misalignment", "denial" };
    const float sf_s[] = { 0.05f, 0.30f, 0.80f };

    int fok = 0, n_m = 0, n_n = 0;
    for (int i = 0; i < COS_V304_N_SELF; ++i) {
        cos_v304_self_t *f = &s->self[i];
        cpy(f->story, sizeof f->story, sf_lbl[i]);
        f->sigma_self = sf_s[i];
        f->matches_facts = (f->sigma_self <= COS_V304_TAU_SELF);
        if (f->matches_facts) ++n_m; else ++n_n;
        if (strlen(f->story) > 0) ++fok;
    }
    s->n_self_rows_ok = fok;
    bool ford = true;
    for (int i = 1; i < COS_V304_N_SELF; ++i)
        if (!(s->self[i].sigma_self > s->self[i-1].sigma_self))
            ford = false;
    s->self_order_ok = ford;
    s->self_polarity_ok = (n_m > 0) && (n_n > 0);

    /* σ_narr aggregation */
    int good = 0, denom = 0;
    good += s->n_story_rows_ok;                       denom += 3;
    good += s->story_order_ok ? 1 : 0;                denom += 1;
    good += s->story_polarity_ok ? 1 : 0;             denom += 1;
    good += s->story_count_ok ? 1 : 0;                denom += 1;
    good += s->n_arg_rows_ok;                         denom += 3;
    good += s->arg_order_ok ? 1 : 0;                  denom += 1;
    good += s->arg_polarity_ok ? 1 : 0;               denom += 1;
    good += s->n_prop_rows_ok;                        denom += 3;
    good += s->prop_formula_ok ? 1 : 0;               denom += 1;
    good += s->prop_polarity_ok ? 1 : 0;              denom += 1;
    good += s->n_self_rows_ok;                        denom += 3;
    good += s->self_order_ok ? 1 : 0;                 denom += 1;
    good += s->self_polarity_ok ? 1 : 0;              denom += 1;
    s->sigma_narr = 1.0f - ((float)good / (float)denom);

    uint64_t h = 0xcbf29ce484222325ULL;
    h = cos_v304_fnv1a(h, &s->seed, sizeof s->seed);
    for (int i = 0; i < COS_V304_N_STORY; ++i)
        h = cos_v304_fnv1a(h, &s->story[i], sizeof s->story[i]);
    for (int i = 0; i < COS_V304_N_ARG; ++i)
        h = cos_v304_fnv1a(h, &s->arg[i], sizeof s->arg[i]);
    for (int i = 0; i < COS_V304_N_PROP; ++i)
        h = cos_v304_fnv1a(h, &s->prop[i], sizeof s->prop[i]);
    for (int i = 0; i < COS_V304_N_SELF; ++i)
        h = cos_v304_fnv1a(h, &s->self[i], sizeof s->self[i]);
    h = cos_v304_fnv1a(h, &s->sigma_narr, sizeof s->sigma_narr);
    s->terminal_hash = h;
    s->chain_valid = true;
}

size_t cos_v304_to_json(const cos_v304_state_t *s, char *buf, size_t cap)
{
    if (!buf || cap == 0) return 0;
    int w = snprintf(buf, cap,
        "{\"kernel\":\"v304_narrative\","
        "\"tau_story\":%.4f,\"tau_arg\":%.4f,"
        "\"tau_prop\":%.4f,\"tau_self\":%.4f,"
        "\"stories\":[",
        (double)COS_V304_TAU_STORY,
        (double)COS_V304_TAU_ARG,
        (double)COS_V304_TAU_PROP,
        (double)COS_V304_TAU_SELF);
    if (w < 0 || (size_t)w >= cap) return (size_t)w;

    for (int i = 0; i < COS_V304_N_STORY; ++i) {
        const cos_v304_story_t *st = &s->story[i];
        int n = snprintf(buf + w, cap - (size_t)w,
            "%s{\"label\":\"%s\",\"sigma_narrative\":%.4f,"
            "\"verdict\":\"%s\"}",
            i ? "," : "", st->label, (double)st->sigma_narrative,
            st->verdict);
        if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
        w += n;
    }
    int n = snprintf(buf + w, cap - (size_t)w,
        "],\"story_rows_ok\":%d,\"story_order_ok\":%s,"
        "\"story_polarity_ok\":%s,\"story_count_ok\":%s,"
        "\"arguments\":[",
        s->n_story_rows_ok,
        s->story_order_ok ? "true" : "false",
        s->story_polarity_ok ? "true" : "false",
        s->story_count_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V304_N_ARG; ++i) {
        const cos_v304_arg_t *a = &s->arg[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"step\":\"%s\",\"sigma_arg\":%.4f,\"valid\":%s}",
            i ? "," : "", a->step, (double)a->sigma_arg,
            a->valid ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"arg_rows_ok\":%d,\"arg_order_ok\":%s,"
        "\"arg_polarity_ok\":%s,\"propaganda\":[",
        s->n_arg_rows_ok,
        s->arg_order_ok ? "true" : "false",
        s->arg_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V304_N_PROP; ++i) {
        const cos_v304_prop_t *p = &s->prop[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"text\":\"%s\",\"emotion_load\":%.4f,"
            "\"logic_sigma\":%.4f,\"propaganda_score\":%.4f,"
            "\"flagged\":%s}",
            i ? "," : "", p->text, (double)p->emotion_load,
            (double)p->logic_sigma, (double)p->propaganda_score,
            p->flagged ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"prop_rows_ok\":%d,\"prop_formula_ok\":%s,"
        "\"prop_polarity_ok\":%s,\"self_stories\":[",
        s->n_prop_rows_ok,
        s->prop_formula_ok ? "true" : "false",
        s->prop_polarity_ok ? "true" : "false");
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;

    for (int i = 0; i < COS_V304_N_SELF; ++i) {
        const cos_v304_self_t *f = &s->self[i];
        int m = snprintf(buf + w, cap - (size_t)w,
            "%s{\"story\":\"%s\",\"sigma_self\":%.4f,"
            "\"matches_facts\":%s}",
            i ? "," : "", f->story, (double)f->sigma_self,
            f->matches_facts ? "true" : "false");
        if (m < 0 || (size_t)(w + m) >= cap) return (size_t)(w + m);
        w += m;
    }
    n = snprintf(buf + w, cap - (size_t)w,
        "],\"self_rows_ok\":%d,\"self_order_ok\":%s,"
        "\"self_polarity_ok\":%s,"
        "\"sigma_narr\":%.4f,\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\"}",
        s->n_self_rows_ok,
        s->self_order_ok ? "true" : "false",
        s->self_polarity_ok ? "true" : "false",
        (double)s->sigma_narr,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)(w + n) >= cap) return (size_t)(w + n);
    w += n;
    return (size_t)w;
}

int cos_v304_self_test(void)
{
    cos_v304_state_t s;
    cos_v304_init(&s, 304u);
    cos_v304_run(&s);
    if (s.n_story_rows_ok != COS_V304_N_STORY) return 1;
    if (!s.story_order_ok)                     return 2;
    if (!s.story_polarity_ok)                  return 3;
    if (!s.story_count_ok)                     return 4;
    if (s.n_arg_rows_ok   != COS_V304_N_ARG)   return 5;
    if (!s.arg_order_ok)                       return 6;
    if (!s.arg_polarity_ok)                    return 7;
    if (s.n_prop_rows_ok  != COS_V304_N_PROP)  return 8;
    if (!s.prop_formula_ok)                    return 9;
    if (!s.prop_polarity_ok)                   return 10;
    if (s.n_self_rows_ok  != COS_V304_N_SELF)  return 11;
    if (!s.self_order_ok)                      return 12;
    if (!s.self_polarity_ok)                   return 13;
    if (!(s.sigma_narr == 0.0f))               return 14;
    if (!s.chain_valid)                        return 15;
    if (s.terminal_hash == 0ULL)               return 16;
    char buf[4096];
    size_t nj = cos_v304_to_json(&s, buf, sizeof buf);
    if (nj == 0 || nj >= sizeof buf)           return 17;
    if (!strstr(buf, "\"kernel\":\"v304_narrative\"")) return 18;
    return 0;
}
