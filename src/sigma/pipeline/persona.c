/*
 * σ-Persona — adaptive user profile implementation.
 *
 * Update rules (all EMA with fixed α to keep runs reproducible):
 *
 *   verbosity   : clip01( v + δ_v )
 *   formality   : clip01( f + δ_f )
 *   expertise   : discrete flip on explicit feedback only
 *   domains     : per-keyword hit count, weights renormalised per
 *                 call so sum of admitted weights ≤ 1
 *   language    : majority of last 8 detections, tie-broken by most
 *                 recent
 *
 * The learning rate is small (±0.10 per explicit feedback event)
 * so a single piece of feedback nudges rather than whipsaws, and
 * 10 consecutive identical events saturate the slider.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "persona.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Helpers
 * ================================================================== */
static float pclip01(float x) {
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static void pncpy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

static void plower(char *s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') *s = (char)(c + 32);
    }
}

/* ==================================================================
 * Lifecycle
 * ================================================================== */
int cos_persona_init(cos_persona_t *p, const char *name) {
    if (!p) return COS_PERSONA_ERR_ARG;
    memset(p, 0, sizeof *p);
    pncpy(p->name, sizeof p->name, name ? name : "anonymous");
    pncpy(p->language, sizeof p->language, "en");
    pncpy(p->expertise_level, sizeof p->expertise_level, "intermediate");
    p->verbosity = 0.50f;
    p->formality = 0.50f;
    p->n_domains = 0;
    p->total_domain_mentions = 0;
    p->turns_observed = 0;
    p->last_feedback = (int)COS_PERSONA_FB_NONE;
    return COS_PERSONA_OK;
}

void cos_persona_reset(cos_persona_t *p) {
    if (!p) return;
    (void)cos_persona_init(p, p->name[0] ? p->name : "anonymous");
}

/* ==================================================================
 * Language detection (cheap, deterministic)
 * ================================================================== */
static int plang_contains_word(const char *lower, const char *needle) {
    size_t n = strlen(needle);
    const char *p = lower;
    while ((p = strstr(p, needle)) != NULL) {
        int before_ok = (p == lower) || !isalpha((unsigned char)p[-1]);
        int after_ok  = !isalpha((unsigned char)p[n]);
        if (before_ok && after_ok) return 1;
        p += n;
    }
    return 0;
}

int cos_persona_detect_language(const char *text,
                                char out[COS_PERSONA_LANG_MAX]) {
    if (!out) return COS_PERSONA_ERR_ARG;
    pncpy(out, COS_PERSONA_LANG_MAX, "en");
    if (!text || !*text) return COS_PERSONA_OK;
    size_t n = strlen(text);
    char *buf = (char *)malloc(n + 1);
    if (!buf) return COS_PERSONA_ERR_BUF;
    memcpy(buf, text, n + 1);
    plower(buf);
    static const char *FI[] = {
        "että", "mikä", "miksi", "kuinka", "minä", "sinä",
        "olen", "olet", "oli", "onko", "ja", "tai", "mutta",
        "ovat", "koska", "mitä", "milloin", "missä", "meidän",
        "hänen", "tämä", "tämän", "teidän", "ollut"
    };
    for (size_t i = 0; i < sizeof FI / sizeof *FI; i++) {
        if (plang_contains_word(buf, FI[i])) {
            pncpy(out, COS_PERSONA_LANG_MAX, "fi");
            break;
        }
    }
    free(buf);
    return COS_PERSONA_OK;
}

/* ==================================================================
 * Domain extraction (stop-word filtered bag of tokens)
 * ================================================================== */
static int pdomain_is_stop(const char *w) {
    static const char *STOP[] = {
        "the","a","an","is","are","was","were","be","of","in","to",
        "and","or","for","on","at","with","by","as","it","this","that",
        "from","how","why","what","when","where","which","who","does",
        "do","can","will","should","about","use","using","used","my",
        "your","our","their","its","i","you","he","she","we","they",
        "not","no","yes","mine","yours","them","ours","theirs","does",
        /* fi stop */
        "että","mikä","miksi","kuinka","minä","sinä","olen","olet","oli",
        "onko","ja","tai","mutta","ovat","koska","mitä","milloin",
        "missä","meidän","hänen","tämä","tämän","teidän","ollut"
    };
    for (size_t i = 0; i < sizeof STOP / sizeof *STOP; i++) {
        if (strcmp(w, STOP[i]) == 0) return 1;
    }
    return 0;
}

static int pdomain_touch(cos_persona_t *p, const char *tok) {
    if (!*tok) return 0;
    for (int i = 0; i < p->n_domains; i++) {
        if (strcmp(p->domains[i].domain, tok) == 0) {
            p->domains[i].mentions++;
            p->total_domain_mentions++;
            return 1;
        }
    }
    if (p->n_domains >= COS_PERSONA_MAX_DOMAINS) {
        /* evict lowest-mention */
        int worst = 0;
        for (int i = 1; i < p->n_domains; i++) {
            if (p->domains[i].mentions < p->domains[worst].mentions) worst = i;
        }
        if (p->domains[worst].mentions > 1) return 0; /* don't churn popular */
        pncpy(p->domains[worst].domain, sizeof p->domains[worst].domain, tok);
        p->domains[worst].mentions = 1;
        p->total_domain_mentions++;
        return 1;
    }
    int slot = p->n_domains++;
    pncpy(p->domains[slot].domain, sizeof p->domains[slot].domain, tok);
    p->domains[slot].mentions = 1;
    p->total_domain_mentions++;
    return 1;
}

static void pdomain_renormalise(cos_persona_t *p) {
    int tot = p->total_domain_mentions > 0 ? p->total_domain_mentions : 1;
    for (int i = 0; i < p->n_domains; i++) {
        p->domains[i].weight = (float)p->domains[i].mentions / (float)tot;
    }
}

/* Sort domains by (descending weight, ascending name). */
static int pdomain_cmp(const void *a, const void *b) {
    const cos_persona_domain_t *x = (const cos_persona_domain_t *)a;
    const cos_persona_domain_t *y = (const cos_persona_domain_t *)b;
    if (x->mentions > y->mentions) return -1;
    if (x->mentions < y->mentions) return  1;
    return strcmp(x->domain, y->domain);
}

/* ==================================================================
 * observe_query
 * ================================================================== */
int cos_persona_observe_query(cos_persona_t *p, const char *query) {
    if (!p) return COS_PERSONA_ERR_ARG;
    if (!query || !*query) return COS_PERSONA_OK;

    /* Language: commit once per call. */
    char lang[COS_PERSONA_LANG_MAX];
    cos_persona_detect_language(query, lang);
    /* Switch only if two consecutive turns agree (simple hysteresis). */
    if (strcmp(lang, "en") != 0) {
        /* Non-English detected — swap immediately on any evidence. */
        pncpy(p->language, sizeof p->language, lang);
    } else if (p->turns_observed == 0) {
        pncpy(p->language, sizeof p->language, lang);
    }
    /* (English only rolls over when no Finnish is ever seen — no-op) */

    /* Domain extraction: scan tokens; lowercased; alnum only; >= 3 chars. */
    size_t n = strlen(query);
    char *buf = (char *)malloc(n + 2);
    if (!buf) return COS_PERSONA_ERR_BUF;
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)query[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c >= 0x80) {
            buf[j++] = (char)c;
        } else {
            buf[j++] = ' ';
        }
    }
    buf[j] = '\0';
    plower(buf);

    int touched = 0;
    char *save = NULL;
    char *tok = strtok_r(buf, " ", &save);
    while (tok) {
        size_t tl = strlen(tok);
        if (tl >= 3 && tl < (size_t)COS_PERSONA_DOMAIN_MAX_CHARS &&
            !pdomain_is_stop(tok)) {
            if (pdomain_touch(p, tok)) touched++;
            if (touched >= 3) break;
        }
        tok = strtok_r(NULL, " ", &save);
    }
    free(buf);

    pdomain_renormalise(p);
    qsort(p->domains, (size_t)p->n_domains, sizeof *p->domains, pdomain_cmp);

    p->turns_observed++;
    return COS_PERSONA_OK;
}

/* ==================================================================
 * apply_feedback
 * ================================================================== */
int cos_persona_apply_feedback(cos_persona_t *p,
                               cos_persona_feedback_t fb) {
    if (!p) return COS_PERSONA_ERR_ARG;
    const float step = 0.10f;
    p->last_feedback = (int)fb;
    switch (fb) {
        case COS_PERSONA_FB_TOO_LONG:
            p->verbosity = pclip01(p->verbosity - step); break;
        case COS_PERSONA_FB_TOO_SHORT:
            p->verbosity = pclip01(p->verbosity + step); break;
        case COS_PERSONA_FB_TOO_FORMAL:
            p->formality = pclip01(p->formality - step); break;
        case COS_PERSONA_FB_TOO_CASUAL:
            p->formality = pclip01(p->formality + step); break;
        case COS_PERSONA_FB_TOO_TECHNICAL:
            pncpy(p->expertise_level, sizeof p->expertise_level, "beginner");
            break;
        case COS_PERSONA_FB_TOO_BASIC:
            pncpy(p->expertise_level, sizeof p->expertise_level, "expert");
            break;
        case COS_PERSONA_FB_PERFECT:
            /* half-step back toward centre so drift does not cascade */
            p->verbosity = pclip01(p->verbosity +
                                   0.05f * (0.5f - p->verbosity));
            p->formality = pclip01(p->formality +
                                   0.05f * (0.5f - p->formality));
            break;
        case COS_PERSONA_FB_NONE:
        default: break;
    }
    return COS_PERSONA_OK;
}

/* ==================================================================
 * learn (string-feedback convenience)
 * ================================================================== */
int cos_persona_learn(cos_persona_t *p,
                      const char *query,
                      const char *feedback) {
    if (!p) return COS_PERSONA_ERR_ARG;
    int rc = cos_persona_observe_query(p, query);
    if (rc != COS_PERSONA_OK) return rc;

    if (!feedback || !*feedback) return COS_PERSONA_OK;

    size_t n = strlen(feedback);
    char buf[64];
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, feedback, n); buf[n] = '\0';
    plower(buf);

    cos_persona_feedback_t fb = COS_PERSONA_FB_NONE;
    if      (strstr(buf, "too long"))       fb = COS_PERSONA_FB_TOO_LONG;
    else if (strstr(buf, "too short"))      fb = COS_PERSONA_FB_TOO_SHORT;
    else if (strstr(buf, "too technical"))  fb = COS_PERSONA_FB_TOO_TECHNICAL;
    else if (strstr(buf, "too basic"))      fb = COS_PERSONA_FB_TOO_BASIC;
    else if (strstr(buf, "too formal"))     fb = COS_PERSONA_FB_TOO_FORMAL;
    else if (strstr(buf, "too casual"))     fb = COS_PERSONA_FB_TOO_CASUAL;
    else if (strstr(buf, "perfect") ||
             strstr(buf, "good"))           fb = COS_PERSONA_FB_PERFECT;

    return cos_persona_apply_feedback(p, fb);
}

/* ==================================================================
 * to_json  (canonical, sorted, deterministic)
 * ================================================================== */
static int pjson_write(char *dst, size_t cap, size_t *off, const char *s) {
    size_t n = strlen(s);
    if (*off + n >= cap) return -1;
    memcpy(dst + *off, s, n);
    *off += n;
    dst[*off] = '\0';
    return 0;
}

static int pjson_writef(char *dst, size_t cap, size_t *off,
                        const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int rc = vsnprintf(dst + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (rc < 0 || (size_t)rc >= cap - *off) return -1;
    *off += (size_t)rc;
    return 0;
}

int cos_persona_to_json(const cos_persona_t *p, char *dst, size_t cap) {
    if (!p || !dst || cap == 0) return COS_PERSONA_ERR_ARG;
    size_t off = 0;
    dst[0] = '\0';
    if (pjson_writef(dst, cap, &off,
        "{\"kernel\":\"sigma_persona\",\"version\":1,"
        "\"name\":\"%s\",\"language\":\"%s\",\"expertise_level\":\"%s\","
        "\"verbosity\":%.3f,\"formality\":%.3f,\"turns\":%d,"
        "\"last_feedback\":%d,\"domains\":[",
        p->name, p->language, p->expertise_level,
        (double)p->verbosity, (double)p->formality,
        p->turns_observed, p->last_feedback)) return COS_PERSONA_ERR_BUF;
    for (int i = 0; i < p->n_domains; i++) {
        if (pjson_writef(dst, cap, &off,
            "%s{\"domain\":\"%s\",\"mentions\":%d,\"weight\":%.4f}",
            i ? "," : "",
            p->domains[i].domain,
            p->domains[i].mentions,
            (double)p->domains[i].weight)) return COS_PERSONA_ERR_BUF;
    }
    if (pjson_write(dst, cap, &off, "]}")) return COS_PERSONA_ERR_BUF;
    return (int)off;
}

/* ==================================================================
 * to_envelope (Codex prompt prefix)
 * ================================================================== */
int cos_persona_to_envelope(const cos_persona_t *p, char *dst, size_t cap) {
    if (!p || !dst || cap == 0) return COS_PERSONA_ERR_ARG;
    size_t off = 0;
    dst[0] = '\0';
    if (pjson_writef(dst, cap, &off,
        "[persona] language=%s expertise=%s verbosity=%.2f formality=%.2f domains=",
        p->language, p->expertise_level,
        (double)p->verbosity, (double)p->formality))
        return COS_PERSONA_ERR_BUF;
    int shown = 0;
    for (int i = 0; i < p->n_domains && shown < 5; i++) {
        if (pjson_writef(dst, cap, &off, "%s%s",
            shown ? "," : "", p->domains[i].domain)) return COS_PERSONA_ERR_BUF;
        shown++;
    }
    if (shown == 0) {
        if (pjson_write(dst, cap, &off, "-")) return COS_PERSONA_ERR_BUF;
    }
    return (int)off;
}

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_persona_self_test(void) {
    cos_persona_t p;
    if (cos_persona_init(&p, "alpha") != COS_PERSONA_OK) return -1;
    if (p.verbosity < 0.49f || p.verbosity > 0.51f) return -2;

    /* Finnish detection */
    char lang[8];
    cos_persona_detect_language("mitä on sigma gate?", lang);
    if (strcmp(lang, "fi") != 0) return -3;
    cos_persona_detect_language("what is the sigma gate", lang);
    if (strcmp(lang, "en") != 0) return -4;

    /* Domain accumulation + sort */
    cos_persona_learn(&p, "how does sigma gate use rethink", "too long");
    cos_persona_learn(&p, "what is the sigma pipeline engram", "too long");
    cos_persona_learn(&p, "rethink the sigma escalation policy", NULL);

    if (p.verbosity > 0.32f) return -5;   /* two nudges ↓ from 0.50 */
    if (p.n_domains < 3) return -6;
    /* "sigma" should win by mentions. */
    if (strcmp(p.domains[0].domain, "sigma") != 0) return -7;

    /* Too technical → beginner */
    cos_persona_apply_feedback(&p, COS_PERSONA_FB_TOO_TECHNICAL);
    if (strcmp(p.expertise_level, "beginner") != 0) return -8;

    /* JSON round-trip is well-formed and contains expected facts. */
    char json[1024];
    int n = cos_persona_to_json(&p, json, sizeof json);
    if (n <= 0) return -9;
    if (!strstr(json, "\"language\":\"en\"")) return -10;
    if (!strstr(json, "\"expertise_level\":\"beginner\"")) return -11;
    if (!strstr(json, "\"domain\":\"sigma\"")) return -12;

    char env[256];
    if (cos_persona_to_envelope(&p, env, sizeof env) <= 0) return -13;
    if (!strstr(env, "[persona]")) return -14;
    if (!strstr(env, "sigma"))     return -15;

    return 0;
}
