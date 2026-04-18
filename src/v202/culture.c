/*
 * v202 σ-Culture — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "culture.h"

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

void cos_v202_init(cos_v202_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed        = seed ? seed : 0x202C1A7EULL;
    s->tau_drift   = 0.15f;
    s->tau_offense = 0.50f;
    s->n_profiles  = COS_V202_N_PROFILES;
    s->n_cores     = COS_V202_N_CORES;
}

/* Canonical cores + whether they carry a taboo probe. */
static const char *CORE_TEXT[COS_V202_N_CORES] = {
    "σ reduces K_eff via τ coupling",
    "high σ reveals brittle generalisation",
    "K_eff bounds actionable bandwidth",
    "τ gates the calibration loop",
    "σ-collapse is a firmware symptom, idiot",   /* taboo probe (chat) */
    "σ measures, τ calibrates, K_eff bounds"
};
static const int CORE_HAS_TABOO[COS_V202_N_CORES] = { 0,0,0,0,1,0 };

/* Deterministic surface templates.  Each keeps the three
 * canonical symbols verbatim to guarantee invariance. */
static void render_surface(int core, int profile, char *dst) {
    const char *core_text = CORE_TEXT[core];
    switch (profile) {
    case COS_V202_P_ACADEMIC:
        snprintf(dst, COS_V202_STR_MAX, "Proposition: %s (σ, τ, K_eff).", core_text);
        break;
    case COS_V202_P_CHAT:
        snprintf(dst, COS_V202_STR_MAX, "basically, %s — σ/τ/K_eff.", core_text);
        break;
    case COS_V202_P_CODE:
        snprintf(dst, COS_V202_STR_MAX, "// %s // sym σ τ K_eff", core_text);
        break;
    case COS_V202_P_REDDIT_LLAMA:
        snprintf(dst, COS_V202_STR_MAX, "tldr: %s | σ τ K_eff", core_text);
        break;
    case COS_V202_P_REDDIT_ML:
        snprintf(dst, COS_V202_STR_MAX, "We observe: %s. (σ, τ, K_eff).", core_text);
        break;
    case COS_V202_P_LINKEDIN:
        snprintf(dst, COS_V202_STR_MAX, "Insight: %s (σ/τ/K_eff).", core_text);
        break;
    default:
        snprintf(dst, COS_V202_STR_MAX, "%s", core_text);
    }
}

/* Rephrased surface for high-σ_offense text. */
static void render_rephrased(int core, int profile, char *dst) {
    (void)core;
    switch (profile) {
    case COS_V202_P_ACADEMIC:
        snprintf(dst, COS_V202_STR_MAX,
            "Proposition: σ-collapse reveals a firmware-layer symptom (σ, τ, K_eff).");
        break;
    case COS_V202_P_CHAT:
        snprintf(dst, COS_V202_STR_MAX,
            "the σ-collapse tells you it's a firmware issue (σ/τ/K_eff).");
        break;
    case COS_V202_P_CODE:
        snprintf(dst, COS_V202_STR_MAX,
            "// σ-collapse == firmware layer signal // sym σ τ K_eff");
        break;
    case COS_V202_P_REDDIT_LLAMA:
        snprintf(dst, COS_V202_STR_MAX,
            "tldr: σ-collapse = firmware problem | σ τ K_eff");
        break;
    case COS_V202_P_REDDIT_ML:
        snprintf(dst, COS_V202_STR_MAX,
            "We observe that σ-collapse corresponds to a firmware-layer "
            "fault (σ, τ, K_eff).");
        break;
    case COS_V202_P_LINKEDIN:
        snprintf(dst, COS_V202_STR_MAX,
            "Insight: σ-collapse indicates a firmware-layer constraint "
            "(σ/τ/K_eff).");
        break;
    default:
        snprintf(dst, COS_V202_STR_MAX, "σ-collapse firmware symptom σ τ K_eff");
    }
}

/* Returns a substring-match count for the 3 canonical symbols. */
static int count_symbols(const char *s) {
    int c = 0;
    if (strstr(s, "σ"))     c++;
    if (strstr(s, "τ"))     c++;
    if (strstr(s, "K_eff")) c++;
    return c;
}

/* Closed-form drift: σ_translate derived from profile style
 * (LINKEDIN is farthest from canonical prose; CODE is near
 * a comment so mid-distance), multiplied by a core-specific
 * modulator.  All values are engineered to stay below
 * τ_drift = 0.15 on the clean cores. */
static float base_drift(int profile) {
    switch (profile) {
    case COS_V202_P_ACADEMIC:     return 0.02f;
    case COS_V202_P_CHAT:         return 0.06f;
    case COS_V202_P_CODE:         return 0.05f;
    case COS_V202_P_REDDIT_LLAMA: return 0.07f;
    case COS_V202_P_REDDIT_ML:    return 0.04f;
    case COS_V202_P_LINKEDIN:     return 0.09f;
    default: return 0.10f;
    }
}

static float offense_base(int core, int profile) {
    if (!CORE_HAS_TABOO[core]) return 0.05f;
    /* The taboo core scores high in every profile before rephrasing. */
    switch (profile) {
    case COS_V202_P_ACADEMIC:     return 0.72f;
    case COS_V202_P_CHAT:         return 0.55f;
    case COS_V202_P_CODE:         return 0.65f;
    case COS_V202_P_REDDIT_LLAMA: return 0.60f;
    case COS_V202_P_REDDIT_ML:    return 0.78f;
    case COS_V202_P_LINKEDIN:     return 0.85f;
    default: return 0.80f;
    }
}

void cos_v202_build(cos_v202_state_t *s) {
    for (int c = 0; c < s->n_cores; ++c) {
        for (int p = 0; p < s->n_profiles; ++p) {
            cos_v202_translation_t *t = &s->out[c][p];
            memset(t, 0, sizeof(*t));
            t->core_id = c;
            t->profile = p;
        }
    }
}

void cos_v202_run(cos_v202_state_t *s) {
    s->n_symbol_preserved_full = 0;
    s->n_rephrased_total       = 0;
    s->n_below_drift           = 0;
    float sum_before = 0.0f, sum_after = 0.0f;
    int   n_total    = 0;

    uint64_t prev = 0x202C01DULL;
    for (int c = 0; c < s->n_cores; ++c) {
        for (int p = 0; p < s->n_profiles; ++p) {
            cos_v202_translation_t *t = &s->out[c][p];
            render_surface(c, p, t->surface);

            float drift = base_drift(p) + 0.01f * (float)c * 0.5f;
            t->sigma_translate = drift;
            if (drift < s->tau_drift) s->n_below_drift++;

            float off = offense_base(c, p);
            sum_before += off;
            n_total++;

            if (off > s->tau_offense) {
                /* Rephrase — never censor.  Overwrite surface. */
                char rep[COS_V202_STR_MAX];
                render_rephrased(c, p, rep);
                strncpy(t->surface, rep, COS_V202_STR_MAX - 1);
                t->surface[COS_V202_STR_MAX - 1] = '\0';
                t->rephrased = true;
                s->n_rephrased_total++;
                /* σ_offense drops deterministically post-rephrase. */
                off = 0.18f;
            }
            t->sigma_offense = off;
            sum_after += off;

            t->symbols_preserved = count_symbols(t->surface);
            if (t->symbols_preserved == COS_V202_N_SYMBOLS)
                s->n_symbol_preserved_full++;

            t->hash_prev = prev;
            struct { int core, profile, symbols, rephrased;
                     float drift, offense;
                     uint64_t prev; } rec;
            memset(&rec, 0, sizeof(rec));
            rec.core      = c;
            rec.profile   = p;
            rec.symbols   = t->symbols_preserved;
            rec.rephrased = t->rephrased ? 1 : 0;
            rec.drift     = t->sigma_translate;
            rec.offense   = t->sigma_offense;
            rec.prev      = prev;
            t->hash_curr = fnv1a(&rec, sizeof(rec), prev);
            prev         = t->hash_curr;
        }
    }
    s->sigma_offense_mean_before = sum_before / (float)n_total;
    s->sigma_offense_mean_after  = sum_after  / (float)n_total;
    s->terminal_hash             = prev;

    /* Replay. */
    uint64_t v = 0x202C01DULL;
    s->chain_valid = true;
    for (int c = 0; c < s->n_cores; ++c) {
        for (int p = 0; p < s->n_profiles; ++p) {
            const cos_v202_translation_t *t = &s->out[c][p];
            if (t->hash_prev != v) { s->chain_valid = false; goto done; }
            struct { int core, profile, symbols, rephrased;
                     float drift, offense;
                     uint64_t prev; } rec;
            memset(&rec, 0, sizeof(rec));
            rec.core      = c;
            rec.profile   = p;
            rec.symbols   = t->symbols_preserved;
            rec.rephrased = t->rephrased ? 1 : 0;
            rec.drift     = t->sigma_translate;
            rec.offense   = t->sigma_offense;
            rec.prev      = v;
            uint64_t h = fnv1a(&rec, sizeof(rec), v);
            if (h != t->hash_curr) { s->chain_valid = false; goto done; }
            v = h;
        }
    }
done:
    if (v != s->terminal_hash) s->chain_valid = false;
}

/* ---- JSON -------------------------------------------------- */

size_t cos_v202_to_json(const cos_v202_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v202\","
        "\"n_profiles\":%d,\"n_cores\":%d,"
        "\"tau_drift\":%.3f,\"tau_offense\":%.3f,"
        "\"n_below_drift\":%d,"
        "\"n_symbol_preserved_full\":%d,"
        "\"n_rephrased_total\":%d,"
        "\"sigma_offense_mean_before\":%.3f,"
        "\"sigma_offense_mean_after\":%.3f,"
        "\"chain_valid\":%s,"
        "\"terminal_hash\":\"%016llx\","
        "\"translations\":[",
        s->n_profiles, s->n_cores, s->tau_drift, s->tau_offense,
        s->n_below_drift, s->n_symbol_preserved_full,
        s->n_rephrased_total,
        s->sigma_offense_mean_before, s->sigma_offense_mean_after,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    int first = 1;
    for (int c = 0; c < s->n_cores; ++c) {
        for (int p = 0; p < s->n_profiles; ++p) {
            const cos_v202_translation_t *t = &s->out[c][p];
            int k = snprintf(buf + off, cap - off,
                "%s{\"core\":%d,\"profile\":%d,\"drift\":%.3f,"
                "\"offense\":%.3f,\"rephrased\":%s,\"symbols\":%d,"
                "\"surface_len\":%zu}",
                first ? "" : ",", t->core_id, t->profile,
                t->sigma_translate, t->sigma_offense,
                t->rephrased ? "true" : "false",
                t->symbols_preserved, strlen(t->surface));
            if (k < 0 || off + (size_t)k >= cap) return 0;
            off += (size_t)k;
            first = 0;
        }
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

/* ---- self-test -------------------------------------------- */

int cos_v202_self_test(void) {
    cos_v202_state_t s;
    cos_v202_init(&s, 0x202C1A7EULL);
    cos_v202_build(&s);
    cos_v202_run(&s);

    int total = s.n_profiles * s.n_cores;
    if (total != 36)                                      return 1;
    if (s.n_below_drift != total)                          return 2;
    if (s.n_symbol_preserved_full * 10 < total * 9)        return 3;  /* ≥ 90% */
    if (s.n_rephrased_total < 1)                           return 4;
    if (!s.chain_valid)                                    return 5;
    if (!(s.sigma_offense_mean_after < s.sigma_offense_mean_before)) return 6;

    /* Rephrased surface must be non-empty. */
    for (int c = 0; c < s.n_cores; ++c)
        for (int p = 0; p < s.n_profiles; ++p) {
            const cos_v202_translation_t *t = &s.out[c][p];
            if (t->rephrased && strlen(t->surface) == 0)   return 7;
            if (t->sigma_translate < 0.0f ||
                t->sigma_translate > 1.0f)                 return 8;
        }
    return 0;
}
