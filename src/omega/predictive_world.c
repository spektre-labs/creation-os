/* Predictive σ-world — running statistics over Ω goals (lab module). */

#include "predictive_world.h"

#include "continual_learning.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

static cos_predictive_world_t g_pw;
static int                    g_pw_inited;

void cos_predictive_world_init(cos_predictive_world_t *wm)
{
    int i;

    if (wm == NULL)
        return;
    memset(wm, 0, sizeof *wm);
    for (i = 0; i < 16; ++i)
        wm->domain_sigma[i] = 0.45f;
    wm->complexity_coeffs[0] = 0.08f;
    wm->complexity_coeffs[1] = 0.12f;
    wm->complexity_coeffs[2] = 0.04f;
    wm->complexity_coeffs[3] = 0.06f;
    for (i = 0; i < 8; ++i)
        wm->model_ceiling[i] = 0.85f;
    wm->trend_window = 32;
}

cos_predictive_world_t *cos_predictive_world_singleton(void)
{
    if (!g_pw_inited) {
        cos_predictive_world_init(&g_pw);
        g_pw_inited = 1;
    }
    return &g_pw;
}

static int contains_ci(const char *hay, const char *needle)
{
    const char *h = hay;
    const char *n = needle;
    size_t      ln;

    if (hay == NULL || needle == NULL)
        return 0;
    ln = strlen(needle);
    if (ln == 0)
        return 0;
    for (; *h; ++h) {
        size_t i;
        for (i = 0; i < ln && h[i]; ++i) {
            char a = (char)tolower((unsigned char)h[i]);
            char b = (char)tolower((unsigned char)n[i]);
            if (a != b)
                break;
        }
        if (i == ln)
            return 1;
    }
    return 0;
}

int cos_predictive_classify_domain(const char *prompt)
{
    if (prompt == NULL || prompt[0] == '\0')
        return 0;
    if (contains_ci(prompt, "theorem") || contains_ci(prompt, "prove ")
        || contains_ci(prompt, "lean"))
        return 1;
    if (contains_ci(prompt, "def ") || contains_ci(prompt, "import ")
        || contains_ci(prompt, "void ") || strstr(prompt, "```") != NULL)
        return 2;
    if (contains_ci(prompt, "poem") || contains_ci(prompt, "story ")
        || contains_ci(prompt, "creative"))
        return 3;
    if (contains_ci(prompt, "legal ") || contains_ci(prompt, "law "))
        return 4;
    if (contains_ci(prompt, "dosage") || contains_ci(prompt, "medical"))
        return 5;
    return 0;
}

int cos_predictive_count_entities(const char *p)
{
    int    n = 0;
    int    prev_space = 1;

    if (p == NULL)
        return 0;
    for (; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (isspace(c)) {
            prev_space = 1;
            continue;
        }
        if (prev_space && isupper(c) && p[1] != '\0' && !isspace((unsigned char)p[1]))
            n++;
        if (isdigit(c))
            n++;
        prev_space = 0;
    }
    return n;
}

int cos_predictive_count_negations(const char *p)
{
    static const char *neg[] = { " not ", " no ", " never ", "n't",
                                 " cannot ", "can't", "won't", "don't" };
    char               low[4096];
    size_t             i, j, cap = sizeof low - 1;

    if (p == NULL)
        return 0;
    for (i = 0; p[i] && i < cap; ++i)
        low[i] = (char)tolower((unsigned char)p[i]);
    low[i] = '\0';
    j = 0;
    for (i = 0; i < ARRAY_SIZE(neg); ++i) {
        const char *s = low;
        size_t      ln = strlen(neg[i]);
        while ((s = strstr(s, neg[i])) != NULL) {
            j++;
            s += ln;
        }
    }
    return (int)j;
}

float cos_predictive_sigma_precall(const cos_predictive_world_t *wm,
                                   const char *prompt,
                                   const char *model_id)
{
    int   d, len, ent, neg;
    float base, pred;
    float len_scale;

    if (wm == NULL || prompt == NULL)
        return 0.5f;
    d   = cos_predictive_classify_domain(prompt);
    len = (int)strlen(prompt);
    ent = cos_predictive_count_entities(prompt);
    neg = cos_predictive_count_negations(prompt);
    if (len < 0)
        len = 0;
    base     = wm->domain_sigma[d < 16 ? d : 0];
    len_scale = (float)len / 400.0f;
    pred      = base + wm->complexity_coeffs[0];
    pred += wm->complexity_coeffs[1] * len_scale;
    pred += wm->complexity_coeffs[2] * (float)ent * 0.08f;
    pred += wm->complexity_coeffs[3] * (float)neg * 0.1f;
    (void)model_id;
    if (pred < 0.f)
        pred = 0.f;
    if (pred > 1.f)
        pred = 1.f;
    return pred;
}

void cos_predictive_world_record_observation(cos_predictive_world_t *wm,
                                             int domain, float observed_sigma)
{
    int   d = domain;
    float prev, alpha = 0.15f;

    if (wm == NULL)
        return;
    if (d < 0 || d >= 16)
        d = 0;
    if (observed_sigma < 0.f)
        observed_sigma = 0.f;
    if (observed_sigma > 1.f)
        observed_sigma = 1.f;
    wm->domain_query_count[d]++;
    prev               = wm->domain_sigma[d];
    wm->domain_sigma[d] = prev * (1.f - alpha) + observed_sigma * alpha;
    wm->sigma_trend     = wm->domain_sigma[d] - prev;
}

void cos_predictive_world_omega_note_turn(const char *goal, float observed_sigma,
                                          int turn_count_after_step)
{
    cos_predictive_world_t *wm = cos_predictive_world_singleton();
    int                     d  = cos_predictive_classify_domain(goal);

    cos_predictive_world_record_observation(wm, d, observed_sigma);
    cos_continual_learning_tick(wm, turn_count_after_step);
}

void cos_predictive_world_fprint_report(FILE *fp, const char *prompt_opt,
                                        const char *model_opt)
{
    cos_predictive_world_t *wm = cos_predictive_world_singleton();
    int                     i, best = -1, worst = -1, hi = 0,
        lo = 2147483647;
    const char *labels[] = { "general", "math/logic", "code", "creative",
                              "legal",   "medical" };

    if (fp == NULL)
        return;
    fputs("=== SELF-MODEL (predictive σ-world, lab) ===\n", fp);
    fputs("Functional statistics only — not consciousness; see "
          "docs/architecture/agi_stack.md.\n\n",
          fp);
    for (i = 0; i < 6; ++i) {
        int q = wm->domain_query_count[i];
        if (q > 0) {
            if (q >= hi) {
                hi   = q;
                best = i;
            }
            if (q <= lo) {
                lo    = q;
                worst = i;
            }
        }
    }
    if (best < 0) {
        fputs("No domain observations yet (run cos omega … to populate).\n",
              fp);
    } else {
        fprintf(fp, "Most-sampled coarse domain: %s (n=%d, σ_ema=%.3f)\n",
                labels[best < 6 ? best : 0], wm->domain_query_count[best],
                (double)wm->domain_sigma[best]);
        if (worst >= 0 && worst != best)
            fprintf(fp, "Least-sampled (among seen): %s (n=%d, σ_ema=%.3f)\n",
                    labels[worst < 6 ? worst : 0],
                    wm->domain_query_count[worst],
                    (double)wm->domain_sigma[worst]);
    }
    fprintf(fp, "Last δσ (EMA step): %.4f | trend_window field=%d\n",
            (double)wm->sigma_trend, wm->trend_window);
    fprintf(fp, "Complexity coeffs [a,b,c,d]: %.3f %.3f %.3f %.3f\n",
            (double)wm->complexity_coeffs[0], (double)wm->complexity_coeffs[1],
            (double)wm->complexity_coeffs[2], (double)wm->complexity_coeffs[3]);
    if (prompt_opt != NULL && prompt_opt[0] != '\0') {
        float pr =
            cos_predictive_sigma_precall(wm, prompt_opt, model_opt);
        fprintf(fp, "\nPrecall σ estimate for prompt: %.3f (coarse; not a "
                    "harness metric)\n",
                (double)pr);
    }
    fputs("\nBlind spots (policy): high-σ coarse buckets for legal/medical "
          "prompts — always verify with domain experts.\n",
          fp);
}

int cos_predictive_world_self_test(void)
{
    cos_predictive_world_t w;
    float                  p;

    cos_predictive_world_init(&w);
    if (cos_predictive_classify_domain("prove this lemma for lean") != 1)
        return 1;
    if (cos_predictive_classify_domain("void foo(){") != 2)
        return 2;
    if (cos_predictive_count_negations("this is not good") < 1)
        return 3;
    p = cos_predictive_sigma_precall(&w, "short", NULL);
    if (p < 0.f || p > 1.f)
        return 4;
    cos_predictive_world_record_observation(&w, 0, 0.6f);
    if (w.domain_query_count[0] != 1)
        return 5;
    return 0;
}
