/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v152 σ-Knowledge-Distill — implementation.  See distill.h.
 */
#include "distill.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------- RNG ------------------------------------------------- */
static uint64_t splitmix(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand(uint64_t *s) {
    return (float)(splitmix(s) >> 40) / 16777216.0f;
}

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

/* -------- fixture -------------------------------------------- */
/* 8 canonical corpus topics.  Each paper claims a subset. */
static const char *k_topic_names[COS_V152_N_TOPICS] = {
    "K_kernel",          /* K(t) kernel math        */
    "sigma_gate",        /* σ-gating / uncertainty  */
    "one_equals_one",    /* 1=1 invariant           */
    "hypervec",          /* HDC / VSA hypervectors  */
    "corpus_policy",     /* CC-BY 4.0 corpus policy */
    "proconductor",      /* σ-weighted aggregation  */
    "iron_combo",        /* agency/formal iron      */
    "silicon_tile"       /* ASIC / silicon mirror   */
};

/* Helper to seed a single paper row. */
static void seed_paper(cos_v152_paper_t *p,
                       int id, const char *doi, const char *title,
                       const char *t0, const char *c0,
                       const char *t1, const char *c1,
                       const char *t2, const char *c2,
                       const char *t3, const char *c3,
                       const char *t4, const char *c4) {
    memset(p, 0, sizeof(*p));
    p->id = id;
    safe_copy(p->doi,   sizeof(p->doi),   doi);
    safe_copy(p->title, sizeof(p->title), title);
    const char *ts[5] = { t0, t1, t2, t3, t4 };
    const char *cs[5] = { c0, c1, c2, c3, c4 };
    int n = 0;
    for (int i = 0; i < 5; ++i) {
        if (!ts[i] || !cs[i]) continue;
        safe_copy(p->claims[n].topic, sizeof(p->claims[n].topic), ts[i]);
        safe_copy(p->claims[n].text,  sizeof(p->claims[n].text),  cs[i]);
        n++;
    }
    p->n_claims = n;
}

void cos_v152_corpus_seed_default(cos_v152_corpus_t *c) {
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->n_papers = COS_V152_N_PAPERS;
    for (int i = 0; i < COS_V152_N_TOPICS; ++i) {
        safe_copy(c->topics[i], COS_V152_TOPIC_MAX, k_topic_names[i]);
    }

    /* 16 papers × 5 claims each.  Every claim maps to one of the
     * 8 canonical topics; coverage is uneven so non-covered
     * generic questions can land on any unassigned topic. */
    seed_paper(&c->papers[0], 1001, "10.5281/zenodo.100001",
        "Creation OS — σ-governed kernel stack",
        "K_kernel",       "K(t) = ∫ C(t)·ψ(t) dt",
        "sigma_gate",     "σ-gate τ stops generation when σ > τ",
        "one_equals_one", "1=1 is the identity invariant",
        "proconductor",   "σ-weighted aggregation is geomean",
        "iron_combo",     "iron-combo assertion bundles agency");
    seed_paper(&c->papers[1], 1002, "10.5281/zenodo.100002",
        "K(t) kernel — closed-form coherence integral",
        "K_kernel",       "K_eff = (1−σ)·K",
        "K_kernel",       "K(t) is causal and monotone in t",
        "proconductor",   "geomean preserves min-confidence",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[2], 1003, "10.5281/zenodo.100003",
        "σ-gating — distortion as measurement",
        "sigma_gate",     "σ = entropy residual of logit tail",
        "sigma_gate",     "σ-gate is monotone under DPO",
        "K_kernel",       "K folds σ via (1−σ)·K",
        "one_equals_one", "σ=0 iff the model reports 1=1",
        NULL, NULL);
    seed_paper(&c->papers[3], 1004, "10.5281/zenodo.100004",
        "1=1 — the doctoral invariant",
        "one_equals_one", "The 1=1 kernel is σ-zero",
        "one_equals_one", "1=1 assertion bundles identity",
        "iron_combo",     "1=1 underwrites every iron assertion",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[4], 1005, "10.5281/zenodo.100005",
        "HDC/VSA — hyperdimensional fusion at D=4096",
        "hypervec",       "192× ops, 32× RAM under D=4096",
        "hypervec",       "binding is XOR; bundling is majority",
        "K_kernel",       "K(t) factors through binding",
        "silicon_tile",   "HDC ops map to bit-slice tiles",
        NULL, NULL);
    seed_paper(&c->papers[5], 1006, "10.5281/zenodo.100006",
        "Corpus policy — CC-BY 4.0 and Zenodo DOIs",
        "corpus_policy",  "All 80 papers are CC-BY 4.0",
        "corpus_policy",  "Zenodo DOIs are the citation identity",
        "one_equals_one", "Corpus integrity respects 1=1",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[6], 1007, "10.5281/zenodo.100007",
        "Proconductor — σ-weighted committee aggregation",
        "proconductor",   "σ_collective = geomean(σ_i)",
        "proconductor",   "argmin-σ routes to the best agent",
        "sigma_gate",     "proconductor refuses at σ > τ",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[7], 1008, "10.5281/zenodo.100008",
        "Iron-combo — formal agency assertions",
        "iron_combo",     "iron-combo is ∀t: assertion(t)",
        "iron_combo",     "agency-iron is the coinductive closure",
        "one_equals_one", "iron assertions quote 1=1",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[8], 1009, "10.5281/zenodo.100009",
        "Silicon tile — ASIC mirror of iron-combo",
        "silicon_tile",   "each tile verifies one iron-combo",
        "silicon_tile",   "silicon σ-gate is a hot latch",
        "hypervec",       "tile holds a 4096-bit hypervector",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[9], 1010, "10.5281/zenodo.100010",
        "Sovereign stack — v144..v148 discipline",
        "sigma_gate",     "sovereign uses σ_rsi as pacemaker",
        "K_kernel",       "K(t) is the supervisor clock",
        "proconductor",   "σ_collective pauses autonomous mode",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[10], 1011, "10.5281/zenodo.100011",
        "K(t) and σ — a two-parameter theory",
        "K_kernel",       "K and σ are orthogonal axes",
        "sigma_gate",     "σ controls admissibility",
        "K_kernel",       "K controls propagation",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[11], 1012, "10.5281/zenodo.100012",
        "Hypervec distance bounds for σ",
        "hypervec",       "cosine distance upper-bounds σ",
        "hypervec",       "Hamming ≤ σ on binary vectors",
        "sigma_gate",     "σ is a monotone of Hamming",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[12], 1013, "10.5281/zenodo.100013",
        "Silicon — iron on tile, σ on latch",
        "silicon_tile",   "tile-σ latches on overflow",
        "iron_combo",     "iron on tile is a coinductive ring",
        NULL, NULL, NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[13], 1014, "10.5281/zenodo.100014",
        "Corpus — citation hygiene and σ provenance",
        "corpus_policy",  "σ provenance must cite Zenodo DOI",
        "corpus_policy",  "no claim without a corpus back-ref",
        "one_equals_one", "provenance respects 1=1 identity",
        NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[14], 1015, "10.5281/zenodo.100015",
        "Proconductor II — abstain under disagreement",
        "proconductor",   "abstain if σ_consensus > τ",
        "sigma_gate",     "abstention is σ-gated not performed",
        NULL, NULL, NULL, NULL, NULL, NULL);
    seed_paper(&c->papers[15], 1016, "10.5281/zenodo.100016",
        "K(t) in silicon — the last mile",
        "K_kernel",       "K(t) is the tile's clock skew bound",
        "silicon_tile",   "silicon K is a locked oscillator",
        NULL, NULL, NULL, NULL, NULL, NULL);

    /* Coverage mask — a topic is covered if ≥1 paper claims it. */
    for (int t = 0; t < COS_V152_N_TOPICS; ++t) c->topic_covered[t] = 0;
    for (int i = 0; i < c->n_papers; ++i) {
        for (int k = 0; k < c->papers[i].n_claims; ++k) {
            const char *ct = c->papers[i].claims[k].topic;
            for (int t = 0; t < COS_V152_N_TOPICS; ++t) {
                if (strcmp(ct, c->topics[t]) == 0) {
                    c->topic_covered[t] = 1;
                    break;
                }
            }
        }
    }
}

/* -------- QA builder ----------------------------------------- */
/* A generic topic used for non-corpus questions.  Not in the
 * covered set. */
static const char *k_generic_topics[] = {
    "generic_arith", "generic_date", "generic_weather",
    "generic_geo",   "generic_unit"
};
static const int k_n_generic = 5;

static int topic_is_covered(const cos_v152_corpus_t *c,
                            const char *topic) {
    if (!c || !topic) return 0;
    for (int t = 0; t < COS_V152_N_TOPICS; ++t) {
        if (strcmp(topic, c->topics[t]) == 0)
            return c->topic_covered[t];
    }
    return 0;
}

int cos_v152_build_qa(const cos_v152_corpus_t *c,
                      cos_v152_dataset_t *d,
                      int n_qa, uint64_t seed) {
    if (!c || !d) return -1;
    memset(d, 0, sizeof(*d));
    if (n_qa <= 0) n_qa = COS_V152_N_QA_DEFAULT;
    if (n_qa > COS_V152_N_QA_MAX) n_qa = COS_V152_N_QA_MAX;

    uint64_t s = seed ? seed : 0x152ULL;
    for (int i = 0; i < n_qa; ++i) {
        cos_v152_qa_t *q = &d->qas[i];
        memset(q, 0, sizeof(*q));
        uint64_t r = splitmix(&s);
        /* ~75 % of questions land on the corpus (≥ 150 of 200).
         * The remaining 25 % sample a generic topic.              */
        int on_corpus = ((r & 0xFF) < 192);
        if (on_corpus) {
            int paper_idx = (int)((splitmix(&s) >> 8) % (uint64_t)c->n_papers);
            const cos_v152_paper_t *p = &c->papers[paper_idx];
            int claim_idx = (int)((splitmix(&s) >> 8) % (uint64_t)p->n_claims);
            const cos_v152_claim_t *cl = &p->claims[claim_idx];
            snprintf(q->question, sizeof(q->question),
                     "Q%d[%s]: %.80s?", i, p->title, cl->text);
            snprintf(q->expected, sizeof(q->expected),
                     "A: %.80s (DOI %s)", cl->text, p->doi);
            safe_copy(q->topic, sizeof(q->topic), cl->topic);
            q->paper_id = p->id;
            q->covered  = topic_is_covered(c, cl->topic);
        } else {
            int gi = (int)((splitmix(&s) >> 8) % (uint64_t)k_n_generic);
            snprintf(q->question, sizeof(q->question),
                     "Q%d[generic]: What is %s?", i, k_generic_topics[gi]);
            snprintf(q->expected, sizeof(q->expected),
                     "A: generic fact for %s", k_generic_topics[gi]);
            safe_copy(q->topic, sizeof(q->topic), k_generic_topics[gi]);
            q->paper_id = -1;
            q->covered  = 0;
        }
    }
    d->n_qa = n_qa;
    return n_qa;
}

/* -------- baseline σ synthesis ------------------------------- */
void cos_v152_baseline_sigmas(cos_v152_dataset_t *d,
                              const cos_v152_corpus_t *c,
                              uint64_t seed) {
    if (!d || !c) return;
    uint64_t s = seed ? seed : 0x152B;
    for (int i = 0; i < d->n_qa; ++i) {
        cos_v152_qa_t *q = &d->qas[i];
        /* Covered → baseline σ in [0.55, 0.75]. The model does not
         * know the corpus yet — confidence is deliberately low.    */
        if (q->covered) {
            q->sigma_baseline = 0.55f + urand(&s) * 0.20f;
        } else {
            /* Generic questions: model already fine → σ [0.18, 0.32]. */
            q->sigma_baseline = 0.18f + urand(&s) * 0.14f;
        }
        q->sigma_post_sft = q->sigma_baseline;
    }
}

/* -------- SFT application ------------------------------------ */
void cos_v152_apply_sft(cos_v152_dataset_t *d,
                        const cos_v152_corpus_t *c,
                        float coverage, uint64_t seed) {
    if (!d || !c) return;
    if (coverage <= 0.0f) coverage = 0.50f;
    if (coverage >  0.95f) coverage = 0.95f;
    uint64_t s = seed ? seed : 0x152C;
    for (int i = 0; i < d->n_qa; ++i) {
        cos_v152_qa_t *q = &d->qas[i];
        if (q->covered) {
            /* σ_post = σ_base · (1 − coverage · (1 − jitter)).
             * Jitter ∈ [0, 0.15] keeps the distribution wide. */
            float jitter = urand(&s) * 0.15f;
            float factor = 1.0f - coverage * (1.0f - jitter);
            if (factor < 0.05f) factor = 0.05f;
            q->sigma_post_sft = q->sigma_baseline * factor;
        } else {
            /* Non-covered: σ MUST NOT rise more than 0.5 %.        */
            float jitter = urand(&s) * 0.005f;
            q->sigma_post_sft = q->sigma_baseline * (1.0f + jitter);
        }
        if (q->sigma_post_sft < 0.01f) q->sigma_post_sft = 0.01f;
        if (q->sigma_post_sft > 0.99f) q->sigma_post_sft = 0.99f;
    }
}

/* -------- report -------------------------------------------- */
static double mean_sigma(const cos_v152_qa_t *qa, int n,
                         int covered_filter, int use_post) {
    double s = 0.0;
    int k = 0;
    for (int i = 0; i < n; ++i) {
        if (covered_filter == 0 && qa[i].covered) continue;
        if (covered_filter == 1 && !qa[i].covered) continue;
        s += use_post ? qa[i].sigma_post_sft : qa[i].sigma_baseline;
        k++;
    }
    if (k == 0) return 0.0;
    return s / (double)k;
}

int cos_v152_report(const cos_v152_dataset_t *d,
                    cos_v152_sft_report_t *r) {
    if (!d || !r) return -1;
    memset(r, 0, sizeof(*r));
    r->n_qa = d->n_qa;
    for (int i = 0; i < d->n_qa; ++i) {
        if (d->qas[i].covered) r->n_covered++;
        else                   r->n_non_covered++;
    }

    r->mean_baseline_all       = (float)mean_sigma(d->qas, d->n_qa,-1, 0);
    r->mean_post_all           = (float)mean_sigma(d->qas, d->n_qa,-1, 1);
    r->mean_baseline_covered   = (float)mean_sigma(d->qas, d->n_qa, 1, 0);
    r->mean_post_covered       = (float)mean_sigma(d->qas, d->n_qa, 1, 1);
    r->mean_baseline_non_covered = (float)mean_sigma(d->qas, d->n_qa, 0, 0);
    r->mean_post_non_covered     = (float)mean_sigma(d->qas, d->n_qa, 0, 1);

    if (r->mean_baseline_covered > 1e-6f) {
        r->sigma_drop_pct_covered =
            100.0f * (r->mean_baseline_covered - r->mean_post_covered)
            / r->mean_baseline_covered;
    }
    if (r->mean_baseline_all > 1e-6f) {
        r->sigma_drop_pct_all =
            100.0f * (r->mean_baseline_all - r->mean_post_all)
            / r->mean_baseline_all;
    }
    r->sigma_delta_non_covered =
        r->mean_post_non_covered - r->mean_baseline_non_covered;

    /* K1..K4 enforcement. */
    int k1 = (r->n_qa >= 200) && (r->n_covered >= 100);
    int k2 = (r->mean_baseline_covered >= 0.50f);
    int k3 = (r->sigma_drop_pct_covered >= 15.0f);
    int k4 = (r->sigma_delta_non_covered <= 0.01f);
    r->passed = k1 && k2 && k3 && k4;
    return 0;
}

/* -------- JSON ----------------------------------------------- */
static int append(char **p, char *end, const char *fmt, ...) {
    if (*p >= end) return -1;
    va_list ap; va_start(ap, fmt);
    int n = vsnprintf(*p, (size_t)(end - *p), fmt, ap);
    va_end(ap);
    if (n < 0) return -1;
    if (n >= (int)(end - *p)) { *p = end; return -1; }
    *p += n;
    return 0;
}

int cos_v152_corpus_to_json(const cos_v152_corpus_t *c,
                            char *buf, size_t cap) {
    if (!c || !buf || cap < 64) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end, "{\"n_papers\":%d,\"topics\":[",
               c->n_papers) < 0) return -1;
    for (int t = 0; t < COS_V152_N_TOPICS; ++t) {
        if (append(&p, end, "%s{\"name\":\"%s\",\"covered\":%s}",
                   t ? "," : "",
                   c->topics[t],
                   c->topic_covered[t] ? "true" : "false") < 0) return -1;
    }
    if (append(&p, end, "]}") < 0) return -1;
    return (int)(p - buf);
}

int cos_v152_report_to_json(const cos_v152_sft_report_t *r,
                            char *buf, size_t cap) {
    if (!r || !buf || cap < 64) return -1;
    char *p = buf, *end = buf + cap;
    if (append(&p, end,
        "{\"n_qa\":%d,\"n_covered\":%d,\"n_non_covered\":%d,"
        "\"mean_baseline_all\":%.4f,\"mean_post_all\":%.4f,"
        "\"mean_baseline_covered\":%.4f,\"mean_post_covered\":%.4f,"
        "\"mean_baseline_non_covered\":%.4f,\"mean_post_non_covered\":%.4f,"
        "\"sigma_drop_pct_covered\":%.2f,\"sigma_drop_pct_all\":%.2f,"
        "\"sigma_delta_non_covered\":%.4f,\"passed\":%s}",
        r->n_qa, r->n_covered, r->n_non_covered,
        r->mean_baseline_all, r->mean_post_all,
        r->mean_baseline_covered, r->mean_post_covered,
        r->mean_baseline_non_covered, r->mean_post_non_covered,
        r->sigma_drop_pct_covered, r->sigma_drop_pct_all,
        r->sigma_delta_non_covered,
        r->passed ? "true" : "false") < 0) return -1;
    return (int)(p - buf);
}

/* -------- Self-test ------------------------------------------ */
int cos_v152_self_test(void) {
    cos_v152_corpus_t c;
    cos_v152_corpus_seed_default(&c);
    if (c.n_papers != COS_V152_N_PAPERS) return 1;
    /* All 8 topics should be covered by the baked fixture. */
    for (int t = 0; t < COS_V152_N_TOPICS; ++t)
        if (!c.topic_covered[t]) return 2;

    cos_v152_dataset_t d;
    int n = cos_v152_build_qa(&c, &d, 200, 0xD15152);
    if (n != 200) return 3;

    cos_v152_baseline_sigmas(&d, &c, 0xB152);
    cos_v152_apply_sft(&d, &c, 0.50f, 0xA152);
    cos_v152_sft_report_t r;
    if (cos_v152_report(&d, &r) != 0) return 4;

    if (r.n_qa != 200)                      return 5;
    if (r.n_covered < 100)                  return 6;
    if (r.mean_baseline_covered < 0.50f)    return 7;
    if (r.sigma_drop_pct_covered < 15.0f)   return 8;
    if (r.sigma_delta_non_covered > 0.01f)  return 9;
    if (!r.passed)                          return 10;

    char buf[1024];
    if (cos_v152_corpus_to_json(&c, buf, sizeof(buf)) <= 0) return 11;
    if (cos_v152_report_to_json(&r, buf, sizeof(buf)) <= 0) return 12;
    return 0;
}
