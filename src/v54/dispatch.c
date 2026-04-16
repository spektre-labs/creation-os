/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v54 σ-dispatch — classify, select, aggregate.
 */
#include "dispatch.h"
#include "disagreement.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static float v54_clip01(float x)
{
    if (!(x >= 0.0f)) return 0.0f;
    if (x > 1.0f)    return 1.0f;
    return x;
}

/* ---- classify -------------------------------------------------- */

static int v54_haystack_contains(const char *hay, const char *needle)
{
    if (!hay || !needle || !*needle) return 0;
    /* case-insensitive substring */
    size_t hn = strlen(hay), nn = strlen(needle);
    if (nn > hn) return 0;
    for (size_t i = 0; i + nn <= hn; i++) {
        size_t k = 0;
        while (k < nn) {
            char a = hay[i + k], b = needle[k];
            if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
            if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
            if (a != b) break;
            k++;
        }
        if (k == nn) return 1;
    }
    return 0;
}

void v54_classify_query(const char *query, v54_query_profile_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->primary    = V54_DOMAIN_FACTUAL;
    out->secondary  = V54_DOMAIN_META;
    out->complexity = 0.50f;
    out->stakes     = 0.50f;

    if (!query) return;

    /* keyword scores per domain */
    int s[V54_N_DOMAINS] = {0};

    const struct { const char *kw; v54_domain_t d; } rules[] = {
        /* logic */
        { "prove",     V54_DOMAIN_LOGIC    },
        { "theorem",   V54_DOMAIN_LOGIC    },
        { "equation",  V54_DOMAIN_LOGIC    },
        { "math",      V54_DOMAIN_LOGIC    },
        { "calculate", V54_DOMAIN_LOGIC    },
        /* factual */
        { "what is",   V54_DOMAIN_FACTUAL  },
        { "who",       V54_DOMAIN_FACTUAL  },
        { "when",      V54_DOMAIN_FACTUAL  },
        { "where",     V54_DOMAIN_FACTUAL  },
        { "define",    V54_DOMAIN_FACTUAL  },
        /* creative */
        { "write",     V54_DOMAIN_CREATIVE },
        { "explain",   V54_DOMAIN_CREATIVE },
        { "story",     V54_DOMAIN_CREATIVE },
        { "poem",      V54_DOMAIN_CREATIVE },
        { "design",    V54_DOMAIN_CREATIVE },
        /* code */
        { "code",      V54_DOMAIN_CODE     },
        { "function",  V54_DOMAIN_CODE     },
        { "bug",       V54_DOMAIN_CODE     },
        { "compile",   V54_DOMAIN_CODE     },
        { "refactor",  V54_DOMAIN_CODE     },
        /* meta */
        { "uncertain", V54_DOMAIN_META     },
        { "confident", V54_DOMAIN_META     },
        { "why",       V54_DOMAIN_META     },
        { "reason",    V54_DOMAIN_META     },
    };
    const int NR = (int)(sizeof(rules) / sizeof(rules[0]));
    for (int i = 0; i < NR; i++) {
        if (v54_haystack_contains(query, rules[i].kw)) {
            s[rules[i].d]++;
        }
    }

    /* pick primary + secondary by score (stable tie-break on enum order) */
    int a_idx = 0, b_idx = 0;
    for (int i = 1; i < V54_N_DOMAINS; i++) {
        if (s[i] > s[a_idx]) a_idx = i;
    }
    for (int i = 0; i < V54_N_DOMAINS; i++) {
        if (i == a_idx) continue;
        if (s[i] > s[b_idx] || (b_idx == a_idx)) b_idx = i;
    }
    out->primary   = (v54_domain_t)a_idx;
    out->secondary = (v54_domain_t)b_idx;

    /* rough complexity/stakes cues (conservative; caller may override) */
    size_t qlen = strlen(query);
    float clen = (float)qlen / 240.0f;
    if (clen > 1.0f) clen = 1.0f;
    out->complexity = 0.30f + 0.70f * clen;

    if (v54_haystack_contains(query, "legal") ||
        v54_haystack_contains(query, "medical") ||
        v54_haystack_contains(query, "safety") ||
        v54_haystack_contains(query, "contract")) {
        out->stakes = 0.90f;
    } else if (v54_haystack_contains(query, "production") ||
               v54_haystack_contains(query, "deploy")) {
        out->stakes = 0.65f;
    } else if (v54_haystack_contains(query, "brainstorm") ||
               v54_haystack_contains(query, "idea")) {
        out->stakes = 0.20f;
    }
}

/* ---- select ---------------------------------------------------- */

typedef struct {
    int   index;
    float score;
    float sigma_primary;
} v54_scored_t;

static int v54_cmp_desc(const void *a, const void *b)
{
    const v54_scored_t *aa = (const v54_scored_t *)a;
    const v54_scored_t *bb = (const v54_scored_t *)b;
    if (aa->score > bb->score) return -1;
    if (aa->score < bb->score) return 1;
    /* stable tie-break by index */
    return (aa->index < bb->index) ? -1 : 1;
}

#include <stdlib.h>

int v54_select_subagents(const v54_proconductor_t *p,
                         const v54_query_profile_t *profile,
                         int *selected, int cap)
{
    if (!p || !profile || !selected || cap <= 0) return 0;
    if (p->n_agents <= 0) return 0;

    v54_scored_t scored[V54_MAX_AGENTS];
    int n = p->n_agents;
    for (int i = 0; i < n; i++) {
        const v54_subagent_t *a = &p->agents[i];
        float sp = a->sigma_profile[profile->primary];
        float ss = a->sigma_profile[profile->secondary];
        float fit = 0.7f * (1.0f - sp) + 0.3f * (1.0f - ss);
        float cost_pen = a->cost_per_1k_tokens / 100.0f;
        float lat_pen  = a->latency_p50_ms     / 10000.0f;
        scored[i].index = i;
        scored[i].score = fit - cost_pen - lat_pen;
        scored[i].sigma_primary = sp;
    }

    qsort(scored, (size_t)n, sizeof(scored[0]), v54_cmp_desc);

    /* Easy-query override: any agent with σ_primary < 0.10 → K=1. */
    int easy = 0;
    for (int i = 0; i < n; i++) {
        if (scored[i].sigma_primary < 0.10f) { easy = 1; break; }
    }

    int K;
    if (easy) {
        K = 1;
    } else if (profile->stakes < 0.30f) {
        K = 1;
    } else if (profile->stakes < 0.70f) {
        K = 2;
    } else {
        K = p->max_parallel_queries > 4 ? 4 : p->max_parallel_queries;
        if (K <= 0) K = 4;
    }
    if (K > n)   K = n;
    if (K > cap) K = cap;

    for (int i = 0; i < K; i++) selected[i] = scored[i].index;
    return K;
}

/* ---- aggregate -------------------------------------------------- */

void v54_aggregate_responses(const v54_proconductor_t *p,
                             const v54_response_t *responses, int n,
                             v54_aggregation_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    out->winner_index = -1;
    if (!responses || n <= 0) {
        out->outcome = V54_AGG_EMPTY;
        return;
    }

    /* pairwise similarity via lexical-overlap helper in disagreement.c */
    v54_disagreement_t dis = {0};
    v54_disagreement_analyze(responses, n, &dis);
    out->disagreement = dis.mean_disagreement;

    /* σ_ensemble = product of reported σs, clamped ≥ 1e-6 to stay numeric */
    float sigma_prod = 1.0f;
    for (int i = 0; i < n; i++) {
        float s = v54_clip01(responses[i].reported_sigma);
        if (s < 1e-3f) s = 1e-3f;
        sigma_prod *= s;
    }
    out->sigma_ensemble = sigma_prod;

    if (n > 1 && dis.mean_disagreement > p->disagreement_abstain) {
        out->outcome = V54_AGG_ABSTAIN_DISAGREE;
        snprintf(out->answer, sizeof(out->answer),
                 "[proconductor abstain] %d models consulted; pairwise disagreement=%.2f > %.2f",
                 n, (double)dis.mean_disagreement,
                 (double)p->disagreement_abstain);
        return;
    }

    /* Consensus path: all agree → σ_ensemble applies directly. */
    if (n > 1 && dis.mean_disagreement <= 0.20f) {
        if (sigma_prod > p->convergence_threshold) {
            out->outcome = V54_AGG_ABSTAIN_SIGMA;
            snprintf(out->answer, sizeof(out->answer),
                     "[proconductor abstain] consensus but σ_ensemble=%.3f > %.3f",
                     (double)sigma_prod, (double)p->convergence_threshold);
            return;
        }
        /* Return response with lowest σ (highest confidence). */
        int best = 0;
        for (int i = 1; i < n; i++) {
            if (responses[i].reported_sigma < responses[best].reported_sigma) {
                best = i;
            }
        }
        out->outcome      = V54_AGG_CONSENSUS;
        out->winner_index = responses[best].agent_index;
        snprintf(out->answer, sizeof(out->answer), "%s", responses[best].text);
        return;
    }

    /* σ-weighted vote (used for n==1 or borderline disagreement).      *
     * Vote weight = 1 / σ; winner is the response whose σ is lowest.   */
    int best = 0;
    for (int i = 1; i < n; i++) {
        if (responses[i].reported_sigma < responses[best].reported_sigma) {
            best = i;
        }
    }
    float best_sigma = v54_clip01(responses[best].reported_sigma);
    if (best_sigma > p->convergence_threshold && n > 1) {
        out->outcome = V54_AGG_ABSTAIN_SIGMA;
        snprintf(out->answer, sizeof(out->answer),
                 "[proconductor abstain] best σ=%.3f > %.3f (n=%d, disagreement=%.2f)",
                 (double)best_sigma, (double)p->convergence_threshold,
                 n, (double)dis.mean_disagreement);
        return;
    }
    out->outcome      = V54_AGG_SIGMA_WINNER;
    out->winner_index = responses[best].agent_index;
    snprintf(out->answer, sizeof(out->answer), "%s", responses[best].text);
}
