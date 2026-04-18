/*
 * v163 σ-Evolve-Architecture — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "evolve.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t sm(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static float urand(uint64_t *s) {
    return (float)((sm(s) >> 40) / (float)(1ULL << 24));
}

/* ---------- gene table ----------------------------------------- */

/* Trade-offs are chosen so that different kernel-subsets
 * dominate different corners of the Pareto front:
 *   - the core three (v101/v106/v111) are cheap and give ~60%
 *     of the accuracy
 *   - v150/v152 are expensive but add the last 10% of accuracy
 *   - v159/v160 add observability cost without accuracy gain
 *     (so evolution should drop them from accuracy-pressed
 *     Pareto points but keep them on memory-tolerant ones)
 */
static const cos_v163_gene_spec_t GENE_TPL[COS_V163_N_GENES] = {
    { "v101", 0.20f,  1.0f,  50.0f },  /* σ-channels */
    { "v106", 0.12f,  5.0f,  40.0f },  /* HTTP server */
    { "v111", 0.18f,  1.0f,   5.0f },  /* σ-gate */
    { "v115", 0.10f,  3.0f,  80.0f },  /* memory */
    { "v117", 0.04f,  1.0f,  30.0f },  /* kv */
    { "v118", 0.05f, 15.0f, 200.0f },  /* vision */
    { "v119", 0.06f,  8.0f,  25.0f },  /* planner */
    { "v124", 0.03f, 12.0f,  40.0f },  /* continual lora */
    { "v128", 0.02f,  6.0f,  30.0f },  /* swarm */
    { "v140", 0.04f,  3.0f,  15.0f },  /* meta */
    { "v150", 0.06f, 50.0f,  60.0f },  /* debate */
    { "v152", 0.05f, 20.0f, 100.0f },  /* corpus */
};

/* ---------- fitness -------------------------------------------- */

static void evaluate(const cos_v163_search_t *s, cos_v163_individual_t *x) {
    float acc_base = 0.30f;
    float acc_sum  = 0.0f;
    float lat_sum  = 0.0f;
    float mem_sum  = 0.0f;
    int   n_on     = 0;
    for (int i = 0; i < COS_V163_N_GENES; ++i) {
        if (!x->genes[i]) continue;
        acc_sum += s->genes[i].contribution;
        lat_sum += s->genes[i].latency_cost_ms;
        mem_sum += s->genes[i].memory_cost_mb;
        ++n_on;
    }
    /* Saturation: sigmoid-ish, matches "more kernels ≠ always
     * more accuracy" behaviour.  Encourages compact architectures. */
    float acc = acc_base + acc_sum * (1.0f - 0.03f * (float)n_on);
    if (acc > 0.999f) acc = 0.999f;
    if (acc < 0.0f)   acc = 0.0f;
    x->accuracy   = acc;
    x->latency_ms = lat_sum;
    x->memory_mb  = mem_sum;
    x->n_on       = n_on;
    x->sigma_gated = false;
}

/* ---------- init ----------------------------------------------- */

void cos_v163_search_init(cos_v163_search_t *s, uint64_t seed) {
    if (!s) return;
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0xE163E163E163E163ULL;
    for (int i = 0; i < COS_V163_N_GENES; ++i) {
        s->genes[i] = GENE_TPL[i];
        s->gene_means[i] = 0.5f;
    }
    s->tau_regression = 0.03f;
    /* Baseline = all genes on. */
    cos_v163_individual_t ref;
    memset(&ref, 0, sizeof(ref));
    for (int i = 0; i < COS_V163_N_GENES; ++i) ref.genes[i] = true;
    evaluate(s, &ref);
    s->baseline_accuracy = ref.accuracy;
    s->best_accuracy_so_far = ref;

    /* Seed population by sampling each gene with p=0.5. */
    uint64_t prng = s->seed;
    for (int p = 0; p < COS_V163_POP; ++p) {
        for (int g = 0; g < COS_V163_N_GENES; ++g) {
            s->population[p].genes[g] = (urand(&prng) < 0.5f);
        }
        evaluate(s, &s->population[p]);
    }
}

/* ---------- Pareto --------------------------------------------- */

static bool dominates(const cos_v163_individual_t *a,
                      const cos_v163_individual_t *b) {
    /* a dominates b iff a is ≥ in every objective and > in at least one. */
    bool ge_all = (a->accuracy   >= b->accuracy)
               && (a->latency_ms <= b->latency_ms)
               && (a->memory_mb  <= b->memory_mb);
    bool g_any = (a->accuracy    >  b->accuracy)
              || (a->latency_ms  <  b->latency_ms)
              || (a->memory_mb   <  b->memory_mb);
    return ge_all && g_any;
}

static int genome_equal(const cos_v163_individual_t *a,
                        const cos_v163_individual_t *b) {
    for (int i = 0; i < COS_V163_N_GENES; ++i) {
        if (a->genes[i] != b->genes[i]) return 0;
    }
    return 1;
}

static void update_pareto(cos_v163_pareto_t *p,
                          const cos_v163_individual_t *cand) {
    /* Reject if dominated or if we already contain an identical genome. */
    for (int i = 0; i < p->n_points; ++i) {
        if (genome_equal(&p->points[i], cand)) return;
        if (dominates(&p->points[i], cand))    return;
    }
    /* Remove any existing points dominated by cand. */
    int w = 0;
    for (int i = 0; i < p->n_points; ++i) {
        if (!dominates(cand, &p->points[i])) {
            p->points[w++] = p->points[i];
        }
    }
    p->n_points = w;
    if (p->n_points < COS_V163_MAX_PARETO) {
        p->points[p->n_points++] = *cand;
    }
}

/* ---------- CMA-ES-lite step ----------------------------------- */

static int cmp_by_accuracy_desc(const void *a, const void *b) {
    const cos_v163_individual_t *x = (const cos_v163_individual_t *)a;
    const cos_v163_individual_t *y = (const cos_v163_individual_t *)b;
    if (x->sigma_gated && !y->sigma_gated) return 1;
    if (!x->sigma_gated && y->sigma_gated) return -1;
    if (x->accuracy > y->accuracy) return -1;
    if (x->accuracy < y->accuracy) return 1;
    return 0;
}

float cos_v163_search_step(cos_v163_search_t *s) {
    if (!s) return 0.0f;

    /* σ-gate: mark candidates whose accuracy regresses > τ. */
    for (int p = 0; p < COS_V163_POP; ++p) {
        evaluate(s, &s->population[p]);
        float regr = s->baseline_accuracy - s->population[p].accuracy;
        if (regr > s->tau_regression) {
            s->population[p].sigma_gated = true;
        }
        /* Feed every (non-gated) individual into the Pareto front. */
        if (!s->population[p].sigma_gated) {
            update_pareto(&s->pareto, &s->population[p]);
            if (s->population[p].accuracy > s->best_accuracy_so_far.accuracy) {
                s->best_accuracy_so_far = s->population[p];
            }
        }
    }

    /* Rank & take the elite quarter. */
    qsort(s->population, COS_V163_POP, sizeof(s->population[0]),
          cmp_by_accuracy_desc);
    int elite_n = COS_V163_POP / 4;
    if (elite_n < 1) elite_n = 1;

    /* Update Bernoulli means toward elite gene frequency. */
    float counts[COS_V163_N_GENES] = {0};
    int   used = 0;
    for (int i = 0; i < elite_n; ++i) {
        if (s->population[i].sigma_gated) continue;
        ++used;
        for (int g = 0; g < COS_V163_N_GENES; ++g) {
            if (s->population[i].genes[g]) counts[g] += 1.0f;
        }
    }
    if (used > 0) {
        const float learn = 0.4f;
        for (int g = 0; g < COS_V163_N_GENES; ++g) {
            float freq = counts[g] / (float)used;
            s->gene_means[g] = (1.0f - learn) * s->gene_means[g] + learn * freq;
            if (s->gene_means[g] < 0.02f) s->gene_means[g] = 0.02f;
            if (s->gene_means[g] > 0.98f) s->gene_means[g] = 0.98f;
        }
    }

    /* Resample: keep elite verbatim; generate others by Bernoulli. */
    uint64_t prng = s->seed + (uint64_t)(s->generation + 1) * 1009ULL;
    for (int i = elite_n; i < COS_V163_POP; ++i) {
        for (int g = 0; g < COS_V163_N_GENES; ++g) {
            s->population[i].genes[g] = (urand(&prng) < s->gene_means[g]);
        }
        s->population[i].sigma_gated = false;
        evaluate(s, &s->population[i]);
    }

    ++s->generation;
    return s->best_accuracy_so_far.accuracy;
}

int cos_v163_search_run(cos_v163_search_t *s) {
    if (!s) return 0;
    for (int i = 0; i < COS_V163_MAX_GEN; ++i) {
        cos_v163_search_step(s);
    }
    /* One final Pareto sweep including the last population. */
    for (int p = 0; p < COS_V163_POP; ++p) {
        if (!s->population[p].sigma_gated)
            update_pareto(&s->pareto, &s->population[p]);
    }
    return s->pareto.n_points;
}

/* ---------- auto-profile --------------------------------------- */

cos_v163_auto_profile_t cos_v163_auto_profile(const cos_v163_search_t *s,
                                              float lat_budget,
                                              float mem_budget) {
    cos_v163_auto_profile_t out;
    memset(&out, 0, sizeof(out));
    out.latency_budget_ms = lat_budget;
    out.memory_budget_mb  = mem_budget;
    float best_acc = -1.0f;
    bool  picked = false;
    for (int i = 0; i < s->pareto.n_points; ++i) {
        const cos_v163_individual_t *c = &s->pareto.points[i];
        if (c->latency_ms > lat_budget) continue;
        if (c->memory_mb  > mem_budget) continue;
        if (c->accuracy > best_acc) {
            best_acc = c->accuracy;
            out.choice = *c;
            picked = true;
        }
    }
    if (!picked && s->pareto.n_points > 0) {
        /* Fallback: pick the Pareto point with smallest memory. */
        int bi = 0;
        for (int i = 1; i < s->pareto.n_points; ++i) {
            if (s->pareto.points[i].memory_mb < s->pareto.points[bi].memory_mb)
                bi = i;
        }
        out.choice = s->pareto.points[bi];
    }
    return out;
}

/* ---------- JSON ----------------------------------------------- */

static int jprintf(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
    if (!buf || *off >= cap) return 0;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    if ((size_t)n >= cap - *off) { *off = cap - 1; return 0; }
    *off += (size_t)n;
    return 1;
}

static int emit_individual(char *buf, size_t cap, size_t *off,
                           const cos_v163_search_t *s,
                           const cos_v163_individual_t *x) {
    jprintf(buf, cap, off,
        "{\"accuracy\":%.4f,\"latency_ms\":%.2f,\"memory_mb\":%.1f,"
        "\"n_on\":%d,\"sigma_gated\":%s,\"genes\":[",
        x->accuracy, x->latency_ms, x->memory_mb,
        x->n_on, x->sigma_gated ? "true" : "false");
    bool first = true;
    for (int i = 0; i < COS_V163_N_GENES; ++i) {
        if (!x->genes[i]) continue;
        if (!first) jprintf(buf, cap, off, ",");
        jprintf(buf, cap, off, "\"%s\"", s->genes[i].id);
        first = false;
    }
    jprintf(buf, cap, off, "]}");
    return 1;
}

size_t cos_v163_search_to_json(const cos_v163_search_t *s,
                               char *buf, size_t cap) {
    if (!s || !buf || cap == 0) return 0;
    size_t off = 0;
    jprintf(buf, cap, &off,
        "{\"kernel\":\"v163\",\"seed\":%llu,\"generations\":%d,"
        "\"baseline_accuracy\":%.4f,\"tau_regression\":%.4f,"
        "\"n_pareto\":%d,\"best_accuracy\":%.4f,\"gene_means\":[",
        (unsigned long long)s->seed, s->generation,
        s->baseline_accuracy, s->tau_regression,
        s->pareto.n_points, s->best_accuracy_so_far.accuracy);
    for (int i = 0; i < COS_V163_N_GENES; ++i) {
        if (i) jprintf(buf, cap, &off, ",");
        jprintf(buf, cap, &off,
            "{\"gene\":\"%s\",\"p\":%.4f}",
            s->genes[i].id, s->gene_means[i]);
    }
    jprintf(buf, cap, &off, "],\"pareto\":[");
    for (int i = 0; i < s->pareto.n_points; ++i) {
        if (i) jprintf(buf, cap, &off, ",");
        emit_individual(buf, cap, &off, s, &s->pareto.points[i]);
    }
    jprintf(buf, cap, &off, "],\"best\":");
    emit_individual(buf, cap, &off, s, &s->best_accuracy_so_far);
    jprintf(buf, cap, &off, "}");
    if (off < cap) buf[off] = '\0';
    return off;
}

/* ---------- self-test ------------------------------------------ */

int cos_v163_self_test(void) {
    cos_v163_search_t s;

    /* E1: init produces 12 genes, 12 individuals, baseline acc > 0. */
    cos_v163_search_init(&s, 163);
    if (s.baseline_accuracy <= 0.0f) return 1;

    /* E2: running the full loop converges a Pareto front ≥ 3 points. */
    int np = cos_v163_search_run(&s);
    if (np < 3) return 2;

    /* E3: the front is non-dominated (no pair where a dominates b). */
    for (int i = 0; i < s.pareto.n_points; ++i) {
        for (int j = 0; j < s.pareto.n_points; ++j) {
            if (i == j) continue;
            if (dominates(&s.pareto.points[i], &s.pareto.points[j])) return 3;
        }
    }

    /* E4: σ-gate active — no front point regresses > τ. */
    for (int i = 0; i < s.pareto.n_points; ++i) {
        float regr = s.baseline_accuracy - s.pareto.points[i].accuracy;
        if (regr > s.tau_regression) return 4;
    }

    /* E5: auto-profile under a tight budget picks a smaller genome
     * than under a generous one.  (M3 8 GB-ish vs. sovereign-like.) */
    cos_v163_auto_profile_t tight   = cos_v163_auto_profile(&s,  50.0f, 200.0f);
    cos_v163_auto_profile_t generous = cos_v163_auto_profile(&s, 500.0f, 2000.0f);
    if (tight.choice.n_on == 0 && s.pareto.n_points > 0) return 5;
    if (generous.choice.n_on < tight.choice.n_on) return 5;

    /* E6: determinism — same seed → byte-identical JSON. */
    cos_v163_search_t a, b;
    cos_v163_search_init(&a, 77); cos_v163_search_run(&a);
    cos_v163_search_init(&b, 77); cos_v163_search_run(&b);
    char ja[32768], jb[32768];
    cos_v163_search_to_json(&a, ja, sizeof(ja));
    cos_v163_search_to_json(&b, jb, sizeof(jb));
    if (strcmp(ja, jb) != 0) return 6;

    return 0;
}
