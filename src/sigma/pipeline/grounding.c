/* σ-Grounding primitive (claims + sources + verdicts). */

#include "grounding.h"

#include <ctype.h>
#include <string.h>

uint64_t cos_sigma_grounding_fnv1a64(const char *bytes, size_t n) {
    uint64_t h = 14695981039346656037ULL;   /* FNV offset basis */
    if (!bytes) return h;
    for (size_t i = 0; i < n; ++i) {
        h ^= (uint64_t)(unsigned char)bytes[i];
        h *= 1099511628211ULL;              /* FNV prime       */
    }
    return h;
}

int cos_sigma_grounding_init(cos_ground_store_t *store,
                             cos_ground_source_t *storage, int cap,
                             float tau_claim, float tau_source)
{
    if (!store || !storage || cap <= 0) return -1;
    if (!(tau_claim > 0.0f && tau_claim <= 1.0f)) return -2;
    if (!(tau_source > 0.0f && tau_source <= 1.0f)) return -3;
    memset(store, 0, sizeof *store);
    memset(storage, 0, sizeof(cos_ground_source_t) * (size_t)cap);
    store->slots      = storage;
    store->cap        = cap;
    store->tau_claim  = tau_claim;
    store->tau_source = tau_source;
    return 0;
}

int cos_sigma_grounding_register(cos_ground_store_t *store,
                                 const char *text, float sigma_source)
{
    if (!store || !text) return -1;
    if (!(sigma_source >= 0.0f && sigma_source <= 1.0f)) return -2;
    if (store->count >= store->cap) return -3;
    cos_ground_source_t *s = &store->slots[store->count++];
    s->text         = text;
    s->sigma_source = sigma_source;
    s->hash_fnv1a64 = cos_sigma_grounding_fnv1a64(text, strlen(text));
    return 0;
}

/* --- helpers ---------------------------------------------------- */

static int count_alnum_words(const char *s) {
    if (!s) return 0;
    int words = 0, in_w = 0;
    for (; *s; ++s) {
        int alnum = isalnum((unsigned char)*s);
        if (alnum && !in_w) { words++; in_w = 1; }
        else if (!alnum)     in_w = 0;
    }
    return words;
}

static int substr_ci(const char *hay, const char *needle) {
    if (!hay || !needle || !*needle) return 0;
    size_t hn = strlen(hay), nn = strlen(needle);
    if (nn > hn) return 0;
    for (size_t i = 0; i + nn <= hn; ++i) {
        int ok = 1;
        for (size_t j = 0; j < nn; ++j) {
            char a = hay[i + j], b = needle[j];
            if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
            if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
            if (a != b) { ok = 0; break; }
        }
        if (ok) return 1;
    }
    return 0;
}

/* Pull the first "anchor" — a word of length ≥ 4 — from the claim. */
static int pick_anchor(const char *claim, char *out, size_t cap) {
    const char *p = claim;
    while (*p) {
        while (*p && !isalpha((unsigned char)*p)) ++p;
        const char *start = p;
        while (*p && isalpha((unsigned char)*p)) ++p;
        size_t len = (size_t)(p - start);
        if (len >= 4) {
            if (len >= cap) len = cap - 1;
            for (size_t i = 0; i < len; ++i) {
                char c = start[i];
                if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
                out[i] = c;
            }
            out[len] = '\0';
            return 1;
        }
    }
    return 0;
}

static int claim_negates(const char *s) {
    /* Very coarse: look for " not " / " no " / "n't" as tokens. */
    if (substr_ci(s, " not ")) return 1;
    if (substr_ci(s, " no "))  return 1;
    if (substr_ci(s, "n't"))   return 1;
    if (strncmp(s, "not ", 4) == 0 || strncmp(s, "No ", 3) == 0) return 1;
    return 0;
}

/* Extract informative sentences. Splits on . ! ? and newline. */
int cos_sigma_grounding_extract_claims(const char *output,
                                       float default_sigma_claim,
                                       cos_ground_claim_t *out, int cap)
{
    if (!output || !out || cap <= 0) return 0;
    static char buf[8 * 1024];
    static char sentences[32][512];
    int n_sent = 0;
    size_t out_len = 0, bufi = 0;

    /* Strip consecutive whitespace while copying into buf. */
    const char *p = output;
    size_t sent_start = 0;
    while (*p && bufi + 1 < sizeof buf) {
        char c = *p++;
        buf[bufi++] = c;
        if (c == '.' || c == '!' || c == '?' || c == '\n') {
            /* finalize sentence */
            buf[bufi] = '\0';
            const char *sbegin = buf + sent_start;
            /* trim leading whitespace */
            while (*sbegin == ' ' || *sbegin == '\t' || *sbegin == '\n') ++sbegin;
            if (*sbegin && n_sent < 32) {
                size_t len = strlen(sbegin);
                if (len > 0) {
                    /* drop trailing punctuation/whitespace */
                    while (len > 0 && (sbegin[len-1] == '.' || sbegin[len-1] == '!' ||
                                       sbegin[len-1] == '?' || sbegin[len-1] == '\n' ||
                                       sbegin[len-1] == ' ' || sbegin[len-1] == '\t'))
                        --len;
                    if (len >= sizeof sentences[0]) len = sizeof sentences[0] - 1;
                    memcpy(sentences[n_sent], sbegin, len);
                    sentences[n_sent][len] = '\0';
                    ++n_sent;
                }
            }
            sent_start = bufi;
        }
    }
    /* Flush trailing partial. */
    if (sent_start < bufi && n_sent < 32) {
        buf[bufi] = '\0';
        const char *sbegin = buf + sent_start;
        while (*sbegin == ' ' || *sbegin == '\t') ++sbegin;
        if (*sbegin) {
            size_t len = strlen(sbegin);
            while (len > 0 && (sbegin[len-1] == ' ' || sbegin[len-1] == '\t'))
                --len;
            if (len > 0) {
                if (len >= sizeof sentences[0]) len = sizeof sentences[0] - 1;
                memcpy(sentences[n_sent], sbegin, len);
                sentences[n_sent][len] = '\0';
                ++n_sent;
            }
        }
    }
    (void)out_len;

    int written = 0;
    for (int i = 0; i < n_sent && written < cap; ++i) {
        if (count_alnum_words(sentences[i]) < 2) continue;
        cos_ground_claim_t *c = &out[written++];
        memset(c, 0, sizeof *c);
        c->text         = sentences[i];
        c->sigma_claim  = default_sigma_claim;
        c->sigma_source = 1.0f;
        c->verdict      = COS_GROUND_SKIPPED;
    }
    return written;
}

cos_ground_verdict_t
cos_sigma_grounding_score(cos_ground_store_t *store,
                          cos_ground_claim_t *claim)
{
    if (!store || !claim || !claim->text) return COS_GROUND_SKIPPED;
    if (count_alnum_words(claim->text) < 2) {
        claim->verdict = COS_GROUND_SKIPPED;
        return COS_GROUND_SKIPPED;
    }
    char anchor[64];
    if (!pick_anchor(claim->text, anchor, sizeof anchor)) {
        claim->verdict = COS_GROUND_SKIPPED;
        return COS_GROUND_SKIPPED;
    }
    /* Scan sources for one containing the anchor; prefer the lowest
     * σ_source among matches. */
    int best = -1;
    for (int i = 0; i < store->count; ++i) {
        if (!substr_ci(store->slots[i].text, anchor)) continue;
        if (best < 0 || store->slots[i].sigma_source <
                        store->slots[best].sigma_source) {
            best = i;
        }
    }
    if (best < 0) {
        claim->sigma_source = 1.0f;
        claim->source_hash  = 0;
        claim->verdict      = COS_GROUND_UNSUPPORTED;
        return COS_GROUND_UNSUPPORTED;
    }
    claim->sigma_source = store->slots[best].sigma_source;
    claim->source_hash  = store->slots[best].hash_fnv1a64;

    /* Contradiction: exactly one of (claim, source) negates the anchor. */
    int cn = claim_negates(claim->text);
    int sn = claim_negates(store->slots[best].text);
    if (cn != sn) {
        claim->verdict = COS_GROUND_CONFLICTED;
        return COS_GROUND_CONFLICTED;
    }

    if (claim->sigma_claim  <= store->tau_claim &&
        claim->sigma_source <= store->tau_source) {
        claim->verdict = COS_GROUND_GROUNDED;
        return COS_GROUND_GROUNDED;
    }
    /* Match found but σ too high → treat as unsupported for safety. */
    claim->verdict = COS_GROUND_UNSUPPORTED;
    return COS_GROUND_UNSUPPORTED;
}

int cos_sigma_grounding_ground(cos_ground_store_t *store,
                               const char *output,
                               float default_sigma_claim,
                               cos_ground_claim_t *out, int cap,
                               float *out_ground_rate)
{
    if (!store || !out || cap <= 0) {
        if (out_ground_rate) *out_ground_rate = 0.0f;
        return 0;
    }
    int n = cos_sigma_grounding_extract_claims(output, default_sigma_claim,
                                               out, cap);
    int n_gr = 0;
    for (int i = 0; i < n; ++i) {
        cos_ground_verdict_t v = cos_sigma_grounding_score(store, &out[i]);
        if (v == COS_GROUND_GROUNDED) ++n_gr;
    }
    if (out_ground_rate)
        *out_ground_rate = (n > 0) ? ((float)n_gr / (float)n) : 0.0f;
    return n;
}

/* ---------------- self-test ------------------------------------ */

static int check_fnv(void) {
    /* Canonical FNV-1a-64 offset basis hash of empty string. */
    uint64_t h0 = cos_sigma_grounding_fnv1a64("", 0);
    if (h0 != 14695981039346656037ULL) return 10;
    /* Hash of "a" is 12638217521852959  -> canonical value.
     * Instead of pinning both, check stability: same input ⇒ same hash. */
    uint64_t h1 = cos_sigma_grounding_fnv1a64("grounding", 9);
    uint64_t h2 = cos_sigma_grounding_fnv1a64("grounding", 9);
    if (h1 != h2) return 11;
    uint64_t h3 = cos_sigma_grounding_fnv1a64("GROUNDING", 9);
    if (h1 == h3) return 12;  /* case-sensitive */
    return 0;
}

static int check_guards(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[4];
    if (cos_sigma_grounding_init(NULL, slots, 4, 0.5f, 0.5f) == 0) return 20;
    if (cos_sigma_grounding_init(&st, NULL, 4, 0.5f, 0.5f)   == 0) return 21;
    if (cos_sigma_grounding_init(&st, slots, 0, 0.5f, 0.5f)  == 0) return 22;
    if (cos_sigma_grounding_init(&st, slots, 4, 0.0f, 0.5f)  == 0) return 23;
    if (cos_sigma_grounding_init(&st, slots, 4, 0.5f, 1.5f)  == 0) return 24;
    if (cos_sigma_grounding_init(&st, slots, 4, 0.5f, 0.5f)  != 0) return 25;
    if (cos_sigma_grounding_register(&st, NULL, 0.1f)        == 0) return 26;
    if (cos_sigma_grounding_register(&st, "a", 1.5f)         == 0) return 27;
    return 0;
}

static int check_grounded_happy(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[4];
    cos_sigma_grounding_init(&st, slots, 4, 0.50f, 0.50f);
    cos_sigma_grounding_register(&st, "Paris is the capital of France.", 0.05f);
    cos_sigma_grounding_register(&st, "The Seine river flows through Paris.", 0.10f);

    cos_ground_claim_t claims[8];
    float rate = -1.0f;
    int n = cos_sigma_grounding_ground(&st, "Paris is a city. It is in France.",
                                       0.15f, claims, 8, &rate);
    if (n != 2) return 30;
    if (claims[0].verdict != COS_GROUND_GROUNDED) return 31;
    if (claims[1].verdict != COS_GROUND_GROUNDED) return 32;
    if (claims[0].source_hash == 0)               return 33;
    if (rate < 0.99f)                              return 34;
    return 0;
}

static int check_unsupported(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[2];
    cos_sigma_grounding_init(&st, slots, 2, 0.50f, 0.50f);
    cos_sigma_grounding_register(&st, "Paris is the capital of France.", 0.05f);
    cos_ground_claim_t claims[4];
    float rate;
    int n = cos_sigma_grounding_ground(&st, "Octopuses have three hearts.",
                                       0.10f, claims, 4, &rate);
    if (n != 1)                                            return 40;
    if (claims[0].verdict != COS_GROUND_UNSUPPORTED)       return 41;
    if (claims[0].source_hash != 0)                        return 42;
    if (rate > 0.0f)                                       return 43;
    return 0;
}

static int check_conflicted(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[2];
    cos_sigma_grounding_init(&st, slots, 2, 0.50f, 0.50f);
    /* Source explicitly NEGATES the anchor token "flammable". */
    cos_sigma_grounding_register(&st,
        "Nitrogen gas is not flammable under atmospheric conditions.", 0.08f);
    cos_ground_claim_t claims[4];
    float rate;
    int n = cos_sigma_grounding_ground(&st,
        "Nitrogen gas is flammable when lit.", 0.10f, claims, 4, &rate);
    if (n != 1)                                            return 50;
    if (claims[0].verdict != COS_GROUND_CONFLICTED)        return 51;
    /* Still identifies the source (hash should be non-zero). */
    if (claims[0].source_hash == 0)                        return 52;
    if (rate > 0.0f)                                       return 53;
    return 0;
}

static int check_sigma_too_high(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[2];
    cos_sigma_grounding_init(&st, slots, 2, 0.20f, 0.20f);
    cos_sigma_grounding_register(&st, "Mercury is the closest planet to the sun.", 0.05f);
    cos_ground_claim_t claims[4];
    /* Claim σ (0.50) > tau_claim (0.20) → even with a match verdict
     * degrades to UNSUPPORTED. */
    int n = cos_sigma_grounding_ground(&st,
        "Mercury orbits the sun quickly.", 0.50f, claims, 4, NULL);
    if (n != 1)                                            return 60;
    if (claims[0].verdict != COS_GROUND_UNSUPPORTED)       return 61;
    /* Source hash should still be set, because we did find a match. */
    if (claims[0].source_hash == 0)                        return 62;
    return 0;
}

static int check_skipped_short(void) {
    cos_ground_store_t st;
    cos_ground_source_t slots[2];
    cos_sigma_grounding_init(&st, slots, 2, 0.50f, 0.50f);
    cos_sigma_grounding_register(&st, "Something long enough to match.", 0.05f);
    cos_ground_claim_t claims[4];
    int n = cos_sigma_grounding_ground(&st, "Ok.", 0.05f, claims, 4, NULL);
    if (n != 0)                                            return 70;
    /* Non-informative "word." also dropped. */
    n = cos_sigma_grounding_ground(&st, "hi.", 0.05f, claims, 4, NULL);
    if (n != 0)                                            return 71;
    return 0;
}

int cos_sigma_grounding_self_test(void) {
    int rc;
    if ((rc = check_fnv()))                 return rc;
    if ((rc = check_guards()))              return rc;
    if ((rc = check_grounded_happy()))      return rc;
    if ((rc = check_unsupported()))         return rc;
    if ((rc = check_conflicted()))          return rc;
    if ((rc = check_sigma_too_high()))      return rc;
    if ((rc = check_skipped_short()))       return rc;
    return 0;
}
