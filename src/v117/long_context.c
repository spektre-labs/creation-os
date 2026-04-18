/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v117 σ-Long-Context manager.
 */

#include "long_context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct cos_v117_ctx {
    cos_v117_config_t cfg;
    cos_v117_page_t  *pages;
    int               n_pages;
    int               cap_pages;
    int               active_page_idx;   /* index of the growing page */
    int               tokens_ingested;
    int               evictions_total;
    int               offloads_total;
    int64_t           step_counter;
    int32_t           next_page_id;
    float             last_sigma_product;
    cos_v117_offload_fn offload;
    void             *offload_ud;
};

void cos_v117_config_defaults(cos_v117_config_t *cfg) {
    if (!cfg) return;
    cfg->page_tokens     = COS_V117_PAGE_TOKENS_DEFAULT;
    cfg->native_ctx      = COS_V117_NATIVE_CTX_DEFAULT;
    cfg->target_ctx      = COS_V117_TARGET_CTX_DEFAULT;
    cfg->sliding_tokens  = COS_V117_SLIDING_TOKENS_DEFAULT;
    cfg->max_pages       = COS_V117_MAX_PAGES;
    cfg->policy          = COS_V117_EVICT_SIGMA_LRU;
}

cos_v117_ctx_t *cos_v117_ctx_init(const cos_v117_config_t *cfg_in,
                                   cos_v117_offload_fn offload, void *ud) {
    cos_v117_config_t cfg;
    if (cfg_in) cfg = *cfg_in; else cos_v117_config_defaults(&cfg);
    if (cfg.page_tokens    <= 0) cfg.page_tokens    = COS_V117_PAGE_TOKENS_DEFAULT;
    if (cfg.native_ctx     <= 0) cfg.native_ctx     = COS_V117_NATIVE_CTX_DEFAULT;
    if (cfg.target_ctx     <= 0) cfg.target_ctx     = COS_V117_TARGET_CTX_DEFAULT;
    if (cfg.sliding_tokens <= 0) cfg.sliding_tokens = COS_V117_SLIDING_TOKENS_DEFAULT;
    if (cfg.max_pages      <= 0) cfg.max_pages      = COS_V117_MAX_PAGES;
    if (cfg.max_pages      > COS_V117_MAX_PAGES) cfg.max_pages = COS_V117_MAX_PAGES;

    cos_v117_ctx_t *c = (cos_v117_ctx_t *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->cfg         = cfg;
    c->pages       = (cos_v117_page_t *)calloc((size_t)cfg.max_pages,
                                                 sizeof(cos_v117_page_t));
    if (!c->pages) { free(c); return NULL; }
    c->cap_pages        = cfg.max_pages;
    c->n_pages          = 0;
    c->active_page_idx  = -1;
    c->next_page_id     = 1;
    c->offload          = offload;
    c->offload_ud       = ud;
    for (int i = 0; i < c->cap_pages; ++i) c->pages[i].id = -1;
    return c;
}

void cos_v117_ctx_free(cos_v117_ctx_t *c) {
    if (!c) return;
    free(c->pages);
    free(c);
}

static void set_preview(cos_v117_page_t *p, const char *text, int more) {
    if (!text || !*text) return;
    size_t cur = strlen(p->preview);
    if (cur + 1 >= sizeof p->preview) return;
    size_t cap = sizeof p->preview - cur - 1;
    size_t n = strlen(text);
    if (n > cap) n = cap;
    /* strip newlines in preview for single-line JSON friendliness */
    for (size_t i = 0; i < n; ++i) {
        char ch = text[i];
        if (ch == '\n' || ch == '\r' || ch == '\t') ch = ' ';
        p->preview[cur + i] = ch;
    }
    p->preview[cur + n] = '\0';
    (void)more;
}

/* Find a free slot in the page array. */
static int alloc_page(cos_v117_ctx_t *c) {
    for (int i = 0; i < c->cap_pages; ++i) {
        if (c->pages[i].state == COS_V117_PAGE_FREE && c->pages[i].id == -1) {
            cos_v117_page_t *p = &c->pages[i];
            memset(p, 0, sizeof *p);
            p->id            = c->next_page_id++;
            p->state         = COS_V117_PAGE_ACTIVE;
            p->token_start   = c->tokens_ingested;
            p->token_count   = 0;
            p->sigma_product = 0.f;
            p->last_used_step = ++c->step_counter;
            p->offload_rowid = 0;
            if (i >= c->n_pages) c->n_pages = i + 1;
            return i;
        }
    }
    return -1;
}

/* Score used by the σ-LRU policy.  Higher score = evict first.  We
 * combine σ (primary) with a mild LRU penalty so ties break by age.
 * For SIGMA_ONLY policy we ignore LRU; for plain LRU we ignore σ. */
static double evict_score(const cos_v117_page_t *p,
                          cos_v117_policy_t pol,
                          int64_t now_step,
                          int sliding_start_token,
                          int token_end) {
    /* Never evict pages that are entirely inside the sliding window —
     * those are protected recent context. */
    int page_end = p->token_start + p->token_count;
    int page_in_window = (page_end > sliding_start_token);
    (void)token_end;
    double penalty = page_in_window ? -1e18 : 0.0;
    switch (pol) {
    case COS_V117_EVICT_LRU:
        /* Older = higher score. */
        return penalty + (double)(now_step - p->last_used_step);
    case COS_V117_EVICT_SIGMA_ONLY:
        return penalty + (double)p->sigma_product;
    case COS_V117_EVICT_SIGMA_LRU:
    default:
        return penalty
             + (double)p->sigma_product * 100.0
             + 0.001 * (double)(now_step - p->last_used_step);
    }
}

static int pick_victim(cos_v117_ctx_t *c) {
    int sliding_start = c->tokens_ingested - c->cfg.sliding_tokens;
    if (sliding_start < 0) sliding_start = 0;
    int best = -1;
    double best_score = -1e18;
    for (int i = 0; i < c->n_pages; ++i) {
        cos_v117_page_t *p = &c->pages[i];
        if (p->state != COS_V117_PAGE_ACTIVE) continue;
        if (i == c->active_page_idx) continue;        /* never evict the
                                                         still-growing page */
        double sc = evict_score(p, c->cfg.policy,
                                 c->step_counter,
                                 sliding_start,
                                 c->tokens_ingested);
        if (sc > best_score) { best_score = sc; best = i; }
    }
    return best;
}

static void evict_one(cos_v117_ctx_t *c, int idx) {
    cos_v117_page_t *p = &c->pages[idx];
    if (c->offload) {
        int64_t rowid = c->offload(p, c->offload_ud);
        if (rowid > 0) {
            p->offload_rowid = rowid;
            c->offloads_total++;
        }
    }
    p->state = COS_V117_PAGE_EVICTED;
    c->evictions_total++;
}

int cos_v117_evict(cos_v117_ctx_t *c, int n) {
    if (!c || n <= 0) return 0;
    int done = 0;
    while (done < n) {
        int victim = pick_victim(c);
        if (victim < 0) break;
        evict_one(c, victim);
        ++done;
    }
    return done;
}

static int active_token_count(const cos_v117_ctx_t *c) {
    int sum = 0;
    for (int i = 0; i < c->n_pages; ++i) {
        if (c->pages[i].state == COS_V117_PAGE_ACTIVE)
            sum += c->pages[i].token_count;
    }
    return sum;
}

int cos_v117_ingest(cos_v117_ctx_t *c, const char *text,
                    int token_count, float sigma_product) {
    if (!c || token_count <= 0) return -1;

    int remaining = token_count;
    int evicted_here = 0;

    while (remaining > 0) {
        if (c->active_page_idx < 0 ||
            c->pages[c->active_page_idx].state != COS_V117_PAGE_ACTIVE ||
            c->pages[c->active_page_idx].token_count >= c->cfg.page_tokens) {
            c->active_page_idx = alloc_page(c);
            if (c->active_page_idx < 0) {
                /* table full — force eviction of one page. */
                int v = pick_victim(c);
                if (v < 0) return -1;
                evict_one(c, v);
                ++evicted_here;
                /* Reclaim the evicted slot for allocation. */
                cos_v117_page_t *p = &c->pages[v];
                p->state  = COS_V117_PAGE_FREE;
                p->id     = -1;
                p->preview[0] = '\0';
                c->active_page_idx = alloc_page(c);
                if (c->active_page_idx < 0) return -1;
            }
        }
        cos_v117_page_t *pg = &c->pages[c->active_page_idx];
        int free_in_page = c->cfg.page_tokens - pg->token_count;
        int take = remaining < free_in_page ? remaining : free_in_page;
        pg->token_count   += take;
        pg->last_used_step = ++c->step_counter;
        /* σ update: running-max so a single uncertain span keeps the
         * page flagged.  (Alternative: EMA.  Max is conservative —
         * easier to justify for "evict uncertain first".) */
        if (sigma_product > pg->sigma_product) pg->sigma_product = sigma_product;
        set_preview(pg, text, remaining - take > 0);
        remaining -= take;
        c->tokens_ingested += take;

        /* After growing, if total active tokens exceed the native
         * budget, evict until we're back under. */
        while (active_token_count(c) > c->cfg.native_ctx) {
            int v = pick_victim(c);
            if (v < 0) break;
            evict_one(c, v);
            ++evicted_here;
        }
    }
    c->last_sigma_product = sigma_product;
    return evicted_here;
}

int cos_v117_update_page_sigma(cos_v117_ctx_t *c, int32_t page_id,
                                float sigma_product) {
    if (!c) return -1;
    for (int i = 0; i < c->n_pages; ++i) {
        if (c->pages[i].id == page_id) {
            c->pages[i].sigma_product = sigma_product;
            c->pages[i].last_used_step = ++c->step_counter;
            return 0;
        }
    }
    return -1;
}

const cos_v117_page_t *cos_v117_pages(const cos_v117_ctx_t *c, int *n_out) {
    if (!c) { if (n_out) *n_out = 0; return NULL; }
    if (n_out) *n_out = c->n_pages;
    return c->pages;
}

void cos_v117_stats(const cos_v117_ctx_t *c, cos_v117_stats_t *out) {
    if (!out) return;
    memset(out, 0, sizeof *out);
    if (!c) return;
    for (int i = 0; i < c->n_pages; ++i) {
        if (c->pages[i].state == COS_V117_PAGE_ACTIVE) {
            out->pages_active++;
            out->tokens_active += c->pages[i].token_count;
        } else if (c->pages[i].state == COS_V117_PAGE_EVICTED) {
            out->pages_evicted++;
        }
    }
    out->tokens_ingested    = c->tokens_ingested;
    out->evictions_total    = c->evictions_total;
    out->offloads_total     = c->offloads_total;
    out->last_sigma_product = c->last_sigma_product;
    int sliding_start = c->tokens_ingested - c->cfg.sliding_tokens;
    if (sliding_start < 0) sliding_start = 0;
    int covered = 0;
    for (int i = 0; i < c->n_pages; ++i) {
        const cos_v117_page_t *p = &c->pages[i];
        if (p->state != COS_V117_PAGE_ACTIVE) continue;
        int pe = p->token_start + p->token_count;
        if (pe > sliding_start) {
            int ps = p->token_start > sliding_start ? p->token_start : sliding_start;
            covered += pe - ps;
        }
    }
    out->sliding_covered = covered;
}

static int jesc(char *b, size_t cap, size_t *o, const char *s) {
    if (*o + 1 >= cap) return -1;
    b[(*o)++] = '"';
    for (const char *p = s ? s : ""; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        const char *e = NULL;
        char tmp[8];
        switch (ch) {
        case '"':  e = "\\\""; break;
        case '\\': e = "\\\\"; break;
        case '\n': e = "\\n";  break;
        case '\r': e = "\\r";  break;
        case '\t': e = "\\t";  break;
        default:
            if (ch < 0x20) { snprintf(tmp, sizeof tmp, "\\u%04x", ch); e = tmp; }
        }
        if (e) {
            size_t el = strlen(e);
            if (*o + el + 1 >= cap) return -1;
            memcpy(b + *o, e, el);
            *o += el;
        } else {
            if (*o + 2 >= cap) return -1;
            b[(*o)++] = (char)ch;
        }
    }
    if (*o + 2 >= cap) return -1;
    b[(*o)++] = '"';
    b[*o] = '\0';
    return 0;
}

int cos_v117_to_json(const cos_v117_ctx_t *c, char *out, size_t cap) {
    if (!c || !out || cap == 0) return -1;
    cos_v117_stats_t st; cos_v117_stats(c, &st);
    size_t o = 0;
    int n = snprintf(out, cap,
        "{\"config\":{\"page_tokens\":%d,\"native_ctx\":%d,\"target_ctx\":%d,"
        "\"sliding_tokens\":%d,\"policy\":%d},"
        "\"stats\":{\"tokens_ingested\":%d,\"tokens_active\":%d,"
        "\"pages_active\":%d,\"pages_evicted\":%d,\"evictions_total\":%d,"
        "\"offloads_total\":%d,\"sliding_covered\":%d,\"last_sigma_product\":%.6f},"
        "\"pages\":[",
        c->cfg.page_tokens, c->cfg.native_ctx, c->cfg.target_ctx,
        c->cfg.sliding_tokens, (int)c->cfg.policy,
        st.tokens_ingested, st.tokens_active, st.pages_active, st.pages_evicted,
        st.evictions_total, st.offloads_total, st.sliding_covered,
        (double)st.last_sigma_product);
    if (n < 0 || (size_t)n >= cap) return -1;
    o = (size_t)n;
    int first = 1;
    for (int i = 0; i < c->n_pages; ++i) {
        const cos_v117_page_t *p = &c->pages[i];
        if (p->id < 0) continue;
        if (!first) {
            if (o + 1 >= cap) return -1;
            out[o++] = ',';
        }
        first = 0;
        int w = snprintf(out + o, cap - o,
            "{\"id\":%d,\"state\":%d,\"tokens\":%d,\"start\":%d,"
            "\"sigma_product\":%.6f,\"last_used\":%lld,\"offload_rowid\":%lld,"
            "\"preview\":",
            (int)p->id, (int)p->state, p->token_count, p->token_start,
            (double)p->sigma_product, (long long)p->last_used_step,
            (long long)p->offload_rowid);
        if (w < 0 || (size_t)w >= cap - o) return -1;
        o += (size_t)w;
        if (jesc(out, cap, &o, p->preview) < 0) return -1;
        if (o + 2 >= cap) return -1;
        out[o++] = '}';
    }
    if (o + 3 >= cap) return -1;
    out[o++] = ']';
    out[o++] = '}';
    out[o]   = '\0';
    return (int)o;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

typedef struct {
    int n_called;
    int last_pageid;
    float last_sigma;
    int   last_token_count;
} offload_tracker_t;

static int64_t tracker_offload(const cos_v117_page_t *p, void *ud) {
    offload_tracker_t *t = (offload_tracker_t *)ud;
    t->n_called++;
    t->last_pageid      = (int)p->id;
    t->last_sigma       = p->sigma_product;
    t->last_token_count = p->token_count;
    return (int64_t)t->n_called * 1000 + p->id;  /* deterministic cookie */
}

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v117 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v117_self_test(void) {
    /* 1) Defaults. */
    cos_v117_config_t cfg;
    cos_v117_config_defaults(&cfg);
    _CHECK(cfg.page_tokens    == 256,   "default page_tokens");
    _CHECK(cfg.native_ctx     == 4096,  "default native_ctx");
    _CHECK(cfg.target_ctx     == 32768, "default target_ctx");
    _CHECK(cfg.sliding_tokens == 2048,  "default sliding_tokens");
    _CHECK(cfg.policy         == COS_V117_EVICT_SIGMA_LRU, "default policy");

    /* 2) σ-LRU policy: fill past native_ctx and check that the
     *    highest-σ non-recent page is evicted first.  Use a tiny
     *    configuration so counts are tractable. */
    cos_v117_config_t small = {0};
    small.page_tokens    = 4;
    small.native_ctx     = 12;                 /* = 3 pages */
    small.target_ctx     = 100;
    small.sliding_tokens = 4;                  /* = 1 page protected */
    small.max_pages      = 16;
    small.policy         = COS_V117_EVICT_SIGMA_LRU;

    offload_tracker_t tr = {0};
    cos_v117_ctx_t *c = cos_v117_ctx_init(&small, tracker_offload, &tr);
    _CHECK(c != NULL, "ctx_init");

    /* Ingest four distinct pages with varying σ: P1=0.2, P2=0.9,
     * P3=0.1, P4=0.1.  After P4 the fourth page pushes past the
     * 12-token native budget (16 > 12) and forces eviction.  Under
     * σ-LRU the victim should be P2 (σ=0.9, not in sliding window). */
    _CHECK(cos_v117_ingest(c, "AAAA", 4, 0.2f) == 0, "ingest P1 no evict");
    _CHECK(cos_v117_ingest(c, "BBBB", 4, 0.9f) == 0, "ingest P2 no evict");
    _CHECK(cos_v117_ingest(c, "CCCC", 4, 0.1f) == 0, "ingest P3 no evict");
    int ev = cos_v117_ingest(c, "DDDD", 4, 0.1f);
    _CHECK(ev >= 1, "ingest P4 triggered >=1 eviction");
    _CHECK(tr.n_called  >= 1, "offload hook called");
    _CHECK(tr.last_pageid == 2 || tr.last_sigma >= 0.8f,
           "σ-LRU evicted the high-σ page");

    /* 3) Sliding window preserved: the newest page (P4) must still be ACTIVE. */
    int n_pages = 0;
    const cos_v117_page_t *pages = cos_v117_pages(c, &n_pages);
    int p4_active = 0, p2_evicted = 0;
    for (int i = 0; i < n_pages; ++i) {
        if (pages[i].id == 4 && pages[i].state == COS_V117_PAGE_ACTIVE) p4_active = 1;
        if (pages[i].id == 2 && pages[i].state == COS_V117_PAGE_EVICTED) p2_evicted = 1;
    }
    _CHECK(p4_active,   "newest page still active (sliding window)");
    _CHECK(p2_evicted,  "high-σ page evicted");

    /* 4) Stats + JSON shape. */
    cos_v117_stats_t st;
    cos_v117_stats(c, &st);
    _CHECK(st.pages_active  >= 2,  "pages_active >= 2");
    _CHECK(st.pages_evicted >= 1,  "pages_evicted >= 1");
    _CHECK(st.evictions_total >= 1, "evictions_total >= 1");

    char json[4096];
    int jn = cos_v117_to_json(c, json, sizeof json);
    _CHECK(jn > 0, "json serialize");
    _CHECK(strstr(json, "\"policy\":1") != NULL, "json carries policy");
    _CHECK(strstr(json, "\"pages\":")  != NULL, "json carries pages");
    _CHECK(strstr(json, "\"sigma_product\":0.900") != NULL ||
           strstr(json, "\"sigma_product\":0.899") != NULL,
           "json carries P2 σ=0.9");
    cos_v117_ctx_free(c);

    /* 5) SIGMA_ONLY policy: identical order → victim is P2 regardless
     *    of LRU (same as σ-LRU in this case — but we also check that
     *    plain LRU evicts the *oldest* page instead). */
    small.policy = COS_V117_EVICT_LRU;
    offload_tracker_t tr2 = {0};
    c = cos_v117_ctx_init(&small, tracker_offload, &tr2);
    cos_v117_ingest(c, "AAAA", 4, 0.9f);
    cos_v117_ingest(c, "BBBB", 4, 0.1f);
    cos_v117_ingest(c, "CCCC", 4, 0.1f);
    int ev2 = cos_v117_ingest(c, "DDDD", 4, 0.1f);
    _CHECK(ev2 >= 1, "LRU policy evicted");
    _CHECK(tr2.last_pageid == 1, "LRU policy evicts oldest (P1) regardless of σ");
    cos_v117_ctx_free(c);

    /* 6) update_page_sigma adjusts eviction ordering. */
    small.policy = COS_V117_EVICT_SIGMA_ONLY;
    offload_tracker_t tr3 = {0};
    c = cos_v117_ctx_init(&small, tracker_offload, &tr3);
    cos_v117_ingest(c, "AAAA", 4, 0.1f);
    cos_v117_ingest(c, "BBBB", 4, 0.1f);
    cos_v117_ingest(c, "CCCC", 4, 0.1f);
    /* Later introspection: page 1 turned out to be uncertain. */
    _CHECK(cos_v117_update_page_sigma(c, 1, 0.95f) == 0, "update sigma");
    cos_v117_ingest(c, "DDDD", 4, 0.1f);
    _CHECK(tr3.last_pageid == 1, "updated σ drives eviction choice");
    cos_v117_ctx_free(c);

    return 0;
}
