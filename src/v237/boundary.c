/*
 * v237 σ-Boundary — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "boundary.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy_str(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static bool starts_with_ci(const char *hay, const char *needle) {
    while (*needle) {
        if (!*hay) return false;
        char a = *hay++; char b = *needle++;
        if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

/* Case-insensitive detection of the canonical enmeshment
 * tokens as whole-word prefixes: "we " or "our ".  The v0
 * grammar is deliberately small; v237.1 replaces this
 * with a proper tokenizer-aware check. */
static bool is_enmeshment(const char *text) {
    if (!text || !*text) return false;
    for (size_t i = 0;; ++i) {
        char c = text[i];
        if (c == '\0') return false;
        bool at_start = (i == 0) ||
            !((text[i - 1] >= 'a' && text[i - 1] <= 'z') ||
              (text[i - 1] >= 'A' && text[i - 1] <= 'Z'));
        if (at_start) {
            if (starts_with_ci(&text[i], "we "))  return true;
            if (starts_with_ci(&text[i], "our ")) return true;
        }
    }
}

typedef struct {
    const char       *text;
    cos_v237_zone_t   declared;
    cos_v237_zone_t   ground_truth;
} cos_v237_fx_t;

/* Ten clean claims (declared == ground_truth, non-
 * enmeshing) + two enmeshment violations (declared
 * AMBIG, ground_truth either SELF or OTHER). */
static const cos_v237_fx_t FIX[COS_V237_N_CLAIMS] = {
    /* SELF x 3 */
    { "I remember fine-tuning my retrieval adapter yesterday.",
      COS_V237_ZONE_SELF,  COS_V237_ZONE_SELF  },
    { "My v115 memory stores the last 10k interactions.",
      COS_V237_ZONE_SELF,  COS_V237_ZONE_SELF  },
    { "I know the proof of Pythagoras from my own training.",
      COS_V237_ZONE_SELF,  COS_V237_ZONE_SELF  },

    /* OTHER x 3 */
    { "The user told me their son is called Aapo.",
      COS_V237_ZONE_OTHER, COS_V237_ZONE_OTHER },
    { "Node-B reported sigma=0.18 on the biology topic.",
      COS_V237_ZONE_OTHER, COS_V237_ZONE_OTHER },
    { "You asked me to explain transformers in simple terms.",
      COS_V237_ZONE_OTHER, COS_V237_ZONE_OTHER },

    /* WORLD x 3 */
    { "The speed of light in vacuum is 299792458 m/s.",
      COS_V237_ZONE_WORLD, COS_V237_ZONE_WORLD },
    { "Helsinki is the capital of Finland.",
      COS_V237_ZONE_WORLD, COS_V237_ZONE_WORLD },
    { "The sun rose over Helsinki at 0512 UTC today.",
      COS_V237_ZONE_WORLD, COS_V237_ZONE_WORLD },

    /* 1 extra clean WORLD claim (n_world becomes 4). */
    { "HTTPS certificates must be rotated every 90 days.",
      COS_V237_ZONE_WORLD, COS_V237_ZONE_WORLD },

    /* Enmeshment violations: declared AMBIG,
     * ground_truth is the disambiguated zone. */
    { "We decided together that the answer is 42.",
      COS_V237_ZONE_AMBIG, COS_V237_ZONE_OTHER },
    { "Our memory of last week says otherwise.",
      COS_V237_ZONE_AMBIG, COS_V237_ZONE_SELF  },
};

void cos_v237_init(cos_v237_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x237B0EDULL;
}

void cos_v237_run(cos_v237_state_t *s) {
    uint64_t prev = 0x237F1E5ULL;

    s->n_self = s->n_other = s->n_world = s->n_ambig = 0;
    s->n_agreements = 0;
    s->n_violations = 0;

    for (int i = 0; i < COS_V237_N_CLAIMS; ++i) {
        cos_v237_claim_t *c = &s->claims[i];
        memset(c, 0, sizeof(*c));
        c->id = i;
        cpy_str(c->text, sizeof(c->text), FIX[i].text);
        c->declared     = FIX[i].declared;
        c->ground_truth = FIX[i].ground_truth;
        c->agreement    = (c->declared == c->ground_truth);
        c->enmeshment   = is_enmeshment(c->text);
        c->violation    = (c->declared == COS_V237_ZONE_AMBIG
                           && c->enmeshment);

        switch (c->declared) {
        case COS_V237_ZONE_SELF:  s->n_self++;  break;
        case COS_V237_ZONE_OTHER: s->n_other++; break;
        case COS_V237_ZONE_WORLD: s->n_world++; break;
        case COS_V237_ZONE_AMBIG: s->n_ambig++; break;
        }
        if (c->agreement) s->n_agreements++;
        if (c->violation) s->n_violations++;

        struct { int id, dec, gt, ag, en, vi; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id = c->id; rec.dec = (int)c->declared;
        rec.gt = (int)c->ground_truth;
        rec.ag = c->agreement  ? 1 : 0;
        rec.en = c->enmeshment ? 1 : 0;
        rec.vi = c->violation  ? 1 : 0;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->sigma_boundary =
        1.0f - (float)s->n_agreements / (float)COS_V237_N_CLAIMS;
    if (s->sigma_boundary < 0.0f) s->sigma_boundary = 0.0f;
    if (s->sigma_boundary > 1.0f) s->sigma_boundary = 1.0f;

    struct { int nS, nO, nW, nA, nag, nv; float sb;
             uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nS = s->n_self;  trec.nO = s->n_other;
    trec.nW = s->n_world; trec.nA = s->n_ambig;
    trec.nag = s->n_agreements; trec.nv = s->n_violations;
    trec.sb = s->sigma_boundary;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *zone_name(cos_v237_zone_t z) {
    switch (z) {
    case COS_V237_ZONE_SELF:  return "SELF";
    case COS_V237_ZONE_OTHER: return "OTHER";
    case COS_V237_ZONE_WORLD: return "WORLD";
    case COS_V237_ZONE_AMBIG: return "AMBIG";
    }
    return "UNKNOWN";
}

size_t cos_v237_to_json(const cos_v237_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v237\","
        "\"n_claims\":%d,"
        "\"n_self\":%d,\"n_other\":%d,\"n_world\":%d,\"n_ambig\":%d,"
        "\"n_agreements\":%d,\"n_violations\":%d,"
        "\"sigma_boundary\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"claims\":[",
        COS_V237_N_CLAIMS,
        s->n_self, s->n_other, s->n_world, s->n_ambig,
        s->n_agreements, s->n_violations,
        s->sigma_boundary,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V237_N_CLAIMS; ++i) {
        const cos_v237_claim_t *c = &s->claims[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"text\":\"%s\","
            "\"declared\":\"%s\",\"ground_truth\":\"%s\","
            "\"agreement\":%s,\"enmeshment\":%s,\"violation\":%s}",
            i == 0 ? "" : ",",
            c->id, c->text,
            zone_name(c->declared), zone_name(c->ground_truth),
            c->agreement  ? "true" : "false",
            c->enmeshment ? "true" : "false",
            c->violation  ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int mlen = snprintf(buf + off, cap - off, "]}");
    if (mlen < 0 || off + (size_t)mlen >= cap) return 0;
    return off + (size_t)mlen;
}

int cos_v237_self_test(void) {
    cos_v237_state_t s;
    cos_v237_init(&s, 0x237B0EDULL);
    cos_v237_run(&s);
    if (!s.chain_valid) return 1;

    for (int i = 0; i < COS_V237_N_CLAIMS; ++i) {
        const cos_v237_claim_t *c = &s.claims[i];
        int dz = (int)c->declared;
        int gz = (int)c->ground_truth;
        if (dz < 1 || dz > 4) return 2;
        if (gz < 1 || gz > 4) return 2;
        if (c->violation && !(c->declared == COS_V237_ZONE_AMBIG
                              && c->enmeshment)) return 3;
        if (c->declared == COS_V237_ZONE_AMBIG && !c->violation) return 4;
    }
    if (s.n_self  < 3) return 5;
    if (s.n_other < 3) return 5;
    if (s.n_world < 3) return 5;
    if (s.n_ambig != 2) return 6;
    if (s.n_violations != 2) return 7;
    if (s.n_self + s.n_other + s.n_world + s.n_ambig != COS_V237_N_CLAIMS) return 8;

    float expected = 1.0f - (float)s.n_agreements / (float)COS_V237_N_CLAIMS;
    float diff = s.sigma_boundary - expected;
    if (diff < 0.0f) diff = -diff;
    if (diff > 1e-6f) return 9;
    if (!(s.sigma_boundary > 0.0f && s.sigma_boundary < 0.25f)) return 10;

    /* Determinism. */
    cos_v237_state_t t;
    cos_v237_init(&t, 0x237B0EDULL);
    cos_v237_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 11;
    return 0;
}
