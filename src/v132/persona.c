/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v132 σ-Persona — implementation.
 */
#include "persona.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * Labels
 * ================================================================ */

const char *cos_v132_level_label(cos_v132_level_t l) {
    switch (l) {
    case COS_V132_LEVEL_BEGINNER:     return "beginner";
    case COS_V132_LEVEL_INTERMEDIATE: return "intermediate";
    case COS_V132_LEVEL_ADVANCED:     return "advanced";
    case COS_V132_LEVEL_EXPERT:       return "expert";
    default:                          return "unknown";
    }
}

const char *cos_v132_length_label(cos_v132_length_t l) {
    switch (l) {
    case COS_V132_LENGTH_TERSE:   return "terse";
    case COS_V132_LENGTH_CONCISE: return "concise";
    case COS_V132_LENGTH_NORMAL:  return "normal";
    case COS_V132_LENGTH_VERBOSE: return "verbose";
    }
    return "normal";
}

const char *cos_v132_tone_label(cos_v132_tone_t t) {
    switch (t) {
    case COS_V132_TONE_DIRECT:  return "direct";
    case COS_V132_TONE_NEUTRAL: return "neutral";
    case COS_V132_TONE_FORMAL:  return "formal";
    }
    return "neutral";
}

const char *cos_v132_detail_label(cos_v132_style_t s) {
    switch (s) {
    case COS_V132_STYLE_SIMPLE:    return "simple";
    case COS_V132_STYLE_NEUTRAL:   return "neutral";
    case COS_V132_STYLE_TECHNICAL: return "technical";
    }
    return "neutral";
}

static int lookup_level(const char *s) {
    if (!s) return COS_V132_LEVEL_UNKNOWN;
    if (!strcmp(s, "beginner"))     return COS_V132_LEVEL_BEGINNER;
    if (!strcmp(s, "intermediate")) return COS_V132_LEVEL_INTERMEDIATE;
    if (!strcmp(s, "advanced"))     return COS_V132_LEVEL_ADVANCED;
    if (!strcmp(s, "expert"))       return COS_V132_LEVEL_EXPERT;
    return COS_V132_LEVEL_UNKNOWN;
}

static int lookup_length(const char *s) {
    if (!s) return COS_V132_LENGTH_NORMAL;
    if (!strcmp(s, "terse"))   return COS_V132_LENGTH_TERSE;
    if (!strcmp(s, "concise")) return COS_V132_LENGTH_CONCISE;
    if (!strcmp(s, "normal"))  return COS_V132_LENGTH_NORMAL;
    if (!strcmp(s, "verbose")) return COS_V132_LENGTH_VERBOSE;
    return COS_V132_LENGTH_NORMAL;
}

static int lookup_tone(const char *s) {
    if (!s) return COS_V132_TONE_NEUTRAL;
    if (!strcmp(s, "direct")) return COS_V132_TONE_DIRECT;
    if (!strcmp(s, "formal")) return COS_V132_TONE_FORMAL;
    return COS_V132_TONE_NEUTRAL;
}

static int lookup_detail(const char *s) {
    if (!s) return COS_V132_STYLE_NEUTRAL;
    if (!strcmp(s, "simple"))    return COS_V132_STYLE_SIMPLE;
    if (!strcmp(s, "technical")) return COS_V132_STYLE_TECHNICAL;
    return COS_V132_STYLE_NEUTRAL;
}

/* ==================================================================
 * Expertise staircase
 * ================================================================ */

cos_v132_level_t cos_v132_level_from_sigma(float s) {
    if (s < 0.0f) s = 0.0f; if (s > 1.0f) s = 1.0f;
    if (s < 0.15f) return COS_V132_LEVEL_EXPERT;
    if (s < 0.35f) return COS_V132_LEVEL_ADVANCED;
    if (s < 0.60f) return COS_V132_LEVEL_INTERMEDIATE;
    return COS_V132_LEVEL_BEGINNER;
}

/* ==================================================================
 * Init / observe / feedback
 * ================================================================ */

void cos_v132_persona_init(cos_v132_persona_t *p) {
    if (!p) return;
    memset(p, 0, sizeof *p);
    p->style_length = COS_V132_LENGTH_NORMAL;
    p->style_tone   = COS_V132_TONE_NEUTRAL;
    p->style_detail = COS_V132_STYLE_NEUTRAL;
    strncpy(p->language, "en", sizeof p->language - 1);
    p->ema_alpha        = COS_V132_EMA_ALPHA_DEFAULT;
    p->adaptation_count = 0;
    p->n_topics         = 0;
}

static cos_v132_topic_expertise_t *
find_or_create_topic(cos_v132_persona_t *p, const char *topic) {
    for (int i = 0; i < p->n_topics; ++i)
        if (strcmp(p->topics[i].topic, topic) == 0) return &p->topics[i];
    if (p->n_topics >= COS_V132_MAX_TOPICS) return NULL;
    cos_v132_topic_expertise_t *t = &p->topics[p->n_topics++];
    memset(t, 0, sizeof *t);
    strncpy(t->topic, topic, sizeof t->topic - 1);
    t->ema_sigma = 0.50f;             /* neutral prior */
    t->samples   = 0;
    t->level     = COS_V132_LEVEL_UNKNOWN;
    return t;
}

float cos_v132_observe(cos_v132_persona_t *p, const char *topic,
                       float sigma_product) {
    if (!p || !topic || !topic[0]) return 0.0f;
    if (sigma_product < 0.0f) sigma_product = 0.0f;
    if (sigma_product > 1.0f) sigma_product = 1.0f;

    cos_v132_topic_expertise_t *t = find_or_create_topic(p, topic);
    if (!t) return 0.0f;

    float a = p->ema_alpha > 0.0f ? p->ema_alpha : COS_V132_EMA_ALPHA_DEFAULT;
    if (t->samples == 0) t->ema_sigma = sigma_product;
    else                 t->ema_sigma = (1.0f - a) * t->ema_sigma
                                      +  a        * sigma_product;
    t->samples++;
    t->level = cos_v132_level_from_sigma(t->ema_sigma);
    p->adaptation_count++;
    return t->ema_sigma;
}

int cos_v132_apply_feedback(cos_v132_persona_t *p, cos_v132_feedback_t fb) {
    if (!p) return -1;
    int changed = 0;
    switch (fb) {
    case COS_V132_FEEDBACK_TOO_LONG:
        if (p->style_length > COS_V132_LENGTH_TERSE) {
            p->style_length = (cos_v132_length_t)((int)p->style_length - 1);
            changed = 1;
        }
        break;
    case COS_V132_FEEDBACK_TOO_SHORT:
        if (p->style_length < COS_V132_LENGTH_VERBOSE) {
            p->style_length = (cos_v132_length_t)((int)p->style_length + 1);
            changed = 1;
        }
        break;
    case COS_V132_FEEDBACK_TOO_TECHNICAL:
        if (p->style_detail > COS_V132_STYLE_SIMPLE) {
            p->style_detail = (cos_v132_style_t)((int)p->style_detail - 1);
            changed = 1;
        }
        break;
    case COS_V132_FEEDBACK_TOO_SIMPLE:
        if (p->style_detail < COS_V132_STYLE_TECHNICAL) {
            p->style_detail = (cos_v132_style_t)((int)p->style_detail + 1);
            changed = 1;
        }
        break;
    case COS_V132_FEEDBACK_TOO_FORMAL:
        if (p->style_tone > COS_V132_TONE_DIRECT) {
            p->style_tone = (cos_v132_tone_t)((int)p->style_tone - 1);
            changed = 1;
        }
        break;
    case COS_V132_FEEDBACK_TOO_DIRECT:
        if (p->style_tone < COS_V132_TONE_FORMAL) {
            p->style_tone = (cos_v132_tone_t)((int)p->style_tone + 1);
            changed = 1;
        }
        break;
    default: return -1;
    }
    p->adaptation_count++;
    return changed;
}

/* ==================================================================
 * TOML I/O — deterministic, self-contained
 * ================================================================ */

int cos_v132_write_toml(const cos_v132_persona_t *p, FILE *fp) {
    if (!p || !fp) return -1;
    fprintf(fp, "# creation-os persona profile (v132)\n");
    fprintf(fp, "# auto-filled from σ observations; editable by hand\n\n");
    fprintf(fp, "[persona]\n");
    fprintf(fp, "language = \"%s\"\n", p->language);
    fprintf(fp, "adaptations = %d\n\n", p->adaptation_count);

    fprintf(fp, "[style]\n");
    fprintf(fp, "length = \"%s\"\n", cos_v132_length_label(p->style_length));
    fprintf(fp, "tone   = \"%s\"\n", cos_v132_tone_label  (p->style_tone));
    fprintf(fp, "detail = \"%s\"\n\n", cos_v132_detail_label(p->style_detail));

    fprintf(fp, "[expertise]\n");
    for (int i = 0; i < p->n_topics; ++i) {
        fprintf(fp,
            "%s = { level = \"%s\", ema_sigma = %.4f, samples = %d }\n",
            p->topics[i].topic,
            cos_v132_level_label(p->topics[i].level),
            (double)p->topics[i].ema_sigma,
            p->topics[i].samples);
    }
    return 0;
}

/* Very small TOML subset parser: handles the layout produced by
 * `cos_v132_write_toml`. */
static char *strip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) e--;
    *e = '\0';
    return s;
}

static void unquote(char *s) {
    size_t n = strlen(s);
    if (n >= 2 && s[0] == '"' && s[n-1] == '"') {
        memmove(s, s + 1, n - 2);
        s[n - 2] = '\0';
    }
}

int cos_v132_read_toml(cos_v132_persona_t *p, FILE *fp) {
    if (!p || !fp) return -1;
    cos_v132_persona_init(p);
    char line[512];
    enum { S_NONE, S_PERSONA, S_STYLE, S_EXPERTISE } section = S_NONE;
    while (fgets(line, sizeof line, fp)) {
        char *l = strip(line);
        if (!*l || *l == '#') continue;
        if (*l == '[') {
            if      (strstr(l, "[persona]"))   section = S_PERSONA;
            else if (strstr(l, "[style]"))     section = S_STYLE;
            else if (strstr(l, "[expertise]")) section = S_EXPERTISE;
            else                                section = S_NONE;
            continue;
        }
        char *eq = strchr(l, '=');
        if (!eq) continue;
        *eq = '\0';
        char *k = strip(l);
        char *v = strip(eq + 1);

        if (section == S_PERSONA) {
            if (!strcmp(k, "language")) {
                unquote(v);
                strncpy(p->language, v, sizeof p->language - 1);
            } else if (!strcmp(k, "adaptations")) {
                p->adaptation_count = atoi(v);
            }
        } else if (section == S_STYLE) {
            unquote(v);
            if      (!strcmp(k, "length")) p->style_length = (cos_v132_length_t)lookup_length(v);
            else if (!strcmp(k, "tone"))   p->style_tone   = (cos_v132_tone_t)  lookup_tone  (v);
            else if (!strcmp(k, "detail")) p->style_detail = (cos_v132_style_t) lookup_detail(v);
        } else if (section == S_EXPERTISE) {
            /* inline-table form:  topic = { level = "...", ema_sigma = f, samples = n } */
            if (p->n_topics >= COS_V132_MAX_TOPICS) continue;
            cos_v132_topic_expertise_t *t = &p->topics[p->n_topics++];
            memset(t, 0, sizeof *t);
            strncpy(t->topic, k, sizeof t->topic - 1);
            char *lvl = strstr(v, "level");
            char *ems = strstr(v, "ema_sigma");
            char *smp = strstr(v, "samples");
            if (lvl) {
                char *q1 = strchr(lvl, '"');
                if (q1) { char *q2 = strchr(q1 + 1, '"');
                    if (q2) { *q2 = '\0'; t->level = (cos_v132_level_t)lookup_level(q1 + 1); } }
            }
            if (ems) { char *eq2 = strchr(ems, '=');
                       if (eq2) t->ema_sigma = (float)atof(eq2 + 1); }
            if (smp) { char *eq2 = strchr(smp, '=');
                       if (eq2) t->samples = atoi(eq2 + 1); }
            if (t->level == COS_V132_LEVEL_UNKNOWN)
                t->level = cos_v132_level_from_sigma(t->ema_sigma);
        }
    }
    return 0;
}

int cos_v132_to_json(const cos_v132_persona_t *p, char *out, size_t cap) {
    if (!p || !out || cap == 0) return -1;
    int off = snprintf(out, cap,
        "{\"language\":\"%s\",\"adaptations\":%d,"
        "\"style\":{\"length\":\"%s\",\"tone\":\"%s\",\"detail\":\"%s\"},"
        "\"topics\":[",
        p->language, p->adaptation_count,
        cos_v132_length_label(p->style_length),
        cos_v132_tone_label  (p->style_tone),
        cos_v132_detail_label(p->style_detail));
    if (off < 0 || (size_t)off >= cap) return -1;
    for (int i = 0; i < p->n_topics; ++i) {
        int w = snprintf(out + off, cap - (size_t)off,
            "%s{\"topic\":\"%s\",\"level\":\"%s\","
            "\"ema_sigma\":%.4f,\"samples\":%d}",
            i == 0 ? "" : ",",
            p->topics[i].topic,
            cos_v132_level_label(p->topics[i].level),
            (double)p->topics[i].ema_sigma,
            p->topics[i].samples);
        if (w < 0 || (size_t)(off + w) >= cap) return -1;
        off += w;
    }
    int w = snprintf(out + off, cap - (size_t)off, "]}");
    if (w < 0 || (size_t)(off + w) >= cap) return -1;
    return off + w;
}

/* ==================================================================
 * Self-test
 * ================================================================ */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v132 self-test FAIL: %s (line %d)\n", (msg), __LINE__); \
        return 1; \
    } \
} while (0)

int cos_v132_self_test(void) {
    cos_v132_persona_t p;
    cos_v132_persona_init(&p);

    /* --- 10 low-σ math observations → expert --------------------- */
    fprintf(stderr, "check-v132: math user → expert\n");
    for (int i = 0; i < 12; ++i)
        cos_v132_observe(&p, "math", 0.10f);
    cos_v132_topic_expertise_t *tm = NULL;
    for (int i = 0; i < p.n_topics; ++i)
        if (!strcmp(p.topics[i].topic, "math")) tm = &p.topics[i];
    _CHECK(tm != NULL, "math topic exists");
    _CHECK(tm->level == COS_V132_LEVEL_EXPERT,
                                    "expert after 12 low-σ samples");
    _CHECK(tm->samples == 12, "samples counted");

    /* --- 8 high-σ biology observations → beginner ---------------- */
    fprintf(stderr, "check-v132: biology user → beginner\n");
    for (int i = 0; i < 8; ++i)
        cos_v132_observe(&p, "biology", 0.85f);
    cos_v132_topic_expertise_t *tb = NULL;
    for (int i = 0; i < p.n_topics; ++i)
        if (!strcmp(p.topics[i].topic, "biology")) tb = &p.topics[i];
    _CHECK(tb != NULL, "biology topic exists");
    _CHECK(tb->level == COS_V132_LEVEL_BEGINNER,
                                    "beginner after 8 high-σ samples");

    /* --- Feedback: too long → length decreases ------------------- */
    fprintf(stderr, "check-v132: feedback nudges style state\n");
    cos_v132_length_t before = p.style_length;
    int changed = cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_LONG);
    _CHECK(changed == 1, "too-long changed state");
    _CHECK((int)p.style_length == (int)before - 1,
                                    "length decremented once");
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_LONG);
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_LONG);
    _CHECK(p.style_length == COS_V132_LENGTH_TERSE,
                                    "clamped at terse");

    /* Opposite direction. */
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_SHORT);
    _CHECK(p.style_length == COS_V132_LENGTH_CONCISE, "short bounces up");

    /* Technical / tone. */
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_TECHNICAL);
    _CHECK(p.style_detail == COS_V132_STYLE_SIMPLE,
                                    "detail moved toward simple");
    /* Start at NEUTRAL, "too-direct" means user wants *less* direct →
     * tone moves up toward FORMAL (NEUTRAL→FORMAL on first step). */
    cos_v132_apply_feedback(&p, COS_V132_FEEDBACK_TOO_DIRECT);
    _CHECK(p.style_tone == COS_V132_TONE_FORMAL,
                                    "tone moved toward formal side");

    /* --- TOML round-trip ---------------------------------------- */
    fprintf(stderr, "check-v132: TOML round-trip\n");
    const char *tmp = "/tmp/cos_v132_persona_test.toml";
    FILE *fp = fopen(tmp, "w"); _CHECK(fp != NULL, "toml open write");
    _CHECK(cos_v132_write_toml(&p, fp) == 0, "toml write");
    fclose(fp);

    cos_v132_persona_t q;
    fp = fopen(tmp, "r"); _CHECK(fp != NULL, "toml open read");
    _CHECK(cos_v132_read_toml(&q, fp) == 0, "toml read");
    fclose(fp);
    remove(tmp);

    _CHECK(!strcmp(q.language, p.language), "language round-trip");
    _CHECK(q.style_length == p.style_length, "length round-trip");
    _CHECK(q.style_tone   == p.style_tone,   "tone round-trip");
    _CHECK(q.style_detail == p.style_detail, "detail round-trip");
    _CHECK(q.n_topics == p.n_topics, "topic count round-trip");
    for (int i = 0; i < p.n_topics; ++i) {
        cos_v132_topic_expertise_t *a = NULL, *b = NULL;
        for (int j = 0; j < q.n_topics; ++j)
            if (!strcmp(q.topics[j].topic, p.topics[i].topic)) a = &q.topics[j];
        b = &p.topics[i];
        _CHECK(a != NULL, "topic present");
        _CHECK(fabsf(a->ema_sigma - b->ema_sigma) < 1e-3f,
                                                   "ema σ round-trip");
        _CHECK(a->level == b->level, "level round-trip");
    }

    /* --- JSON export shape -------------------------------------- */
    char js[1024];
    int wsz = cos_v132_to_json(&p, js, sizeof js);
    _CHECK(wsz > 0 && wsz < (int)sizeof js, "json write");
    _CHECK(strstr(js, "\"topic\":\"math\"") != NULL, "json topic");

    fprintf(stderr, "check-v132: OK (expertise + feedback + TOML + JSON)\n");
    return 0;
}
