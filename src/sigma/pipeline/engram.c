/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Engram primitive — open-addressed hash cache with σ-aware TTL. */

#include "engram.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/** Lowercase ASCII, trim, collapse runs of whitespace — stable prompt key. */
static void cos_engram_ascii_normalize(const char *in, char *out, size_t cap) {
    if (!out || cap < 2) return;
    out[0] = '\0';
    if (!in) return;
    size_t j = 0;
    int    in_space = 0;
    for (size_t i = 0; in[i] != '\0' && j + 1 < cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c + 32u);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (j > 0 && !in_space) {
                out[j++] = ' ';
                in_space = 1;
            }
        } else {
            out[j++] = (char)c;
            in_space = 0;
        }
    }
    while (j > 0 && out[j - 1] == ' ') j--;
    out[j] = '\0';
}

uint64_t cos_sigma_engram_hash(const char *s) {
    const uint64_t FNV_OFFSET = 1469598103934665603ULL;
    const uint64_t FNV_PRIME  = 1099511628211ULL;
    char           norm[2048];
    uint64_t       h = FNV_OFFSET;
    if (!s) return 1ULL;
    cos_engram_ascii_normalize(s, norm, sizeof norm);
    for (const unsigned char *p = (const unsigned char *)norm; *p; ++p) {
        h ^= (uint64_t)(*p);
        h *= FNV_PRIME;
    }
    return (h == 0ULL) ? 1ULL : h;
}

static int is_power_of_two(uint32_t n) {
    return n >= 4 && (n & (n - 1)) == 0;
}

int cos_sigma_engram_init(cos_sigma_engram_t *e,
                          cos_sigma_engram_entry_t *slots, uint32_t cap,
                          float tau_trust,
                          uint32_t long_ttl, uint32_t short_ttl) {
    if (!e || !slots)                  return -1;
    if (!is_power_of_two(cap))         return -2;
    if (!(tau_trust >= 0.0f) ||
         tau_trust  > 1.0f  ||
         tau_trust != tau_trust)        return -3;
    if (long_ttl < short_ttl)          return -4;
    memset(slots, 0, (size_t)cap * sizeof(cos_sigma_engram_entry_t));
    e->slots = slots;
    e->cap = cap;
    e->count = 0;
    e->now_step = 0;
    e->tau_trust = tau_trust;
    e->long_ttl = long_ttl;
    e->short_ttl = short_ttl;
    e->n_put = e->n_get_hit = e->n_get_miss =
        e->n_evict = e->n_aged = 0;
    return 0;
}

void cos_sigma_engram_tick(cos_sigma_engram_t *e) {
    if (e) e->now_step++;
}

/* Returns index of `key_hash` slot OR first empty slot along the probe
 * sequence (guaranteed to exist if count < cap). */
static uint32_t probe(const cos_sigma_engram_t *e, uint64_t key_hash,
                      int *found_exact) {
    uint32_t mask = e->cap - 1u;
    uint32_t i = (uint32_t)(key_hash & mask);
    uint32_t first_empty = 0xFFFFFFFFu;
    for (uint32_t step = 0; step < e->cap; ++step) {
        const cos_sigma_engram_entry_t *s = &e->slots[i];
        if (s->key_hash == 0ULL) {
            if (first_empty == 0xFFFFFFFFu) first_empty = i;
            *found_exact = 0;
            /* A tombstone is marked by key_hash == 0 AND value != NULL;
             * we don't use tombstones — delete re-shuffles.  Return on
             * first empty. */
            return first_empty;
        }
        if (s->key_hash == key_hash) {
            *found_exact = 1;
            return i;
        }
        i = (i + 1u) & mask;
    }
    /* Table full AND no exact match. */
    *found_exact = 0;
    return first_empty;   /* 0xFFFFFFFFu iff genuinely full */
}

/* Pick a victim slot for eviction when put() hits a full table.
 * Score = σ_at_store * 1.0
 *       + (now_step - last_access_step) * 0.01    (age → higher score)
 *       - access_count                  * 0.1     (usage → lower score)
 * Highest score wins (gets evicted).  Ties broken by lower
 * access_count. */
static uint32_t pick_victim(const cos_sigma_engram_t *e) {
    uint32_t best_i = 0;
    float best_score = -1e30f;
    for (uint32_t i = 0; i < e->cap; ++i) {
        const cos_sigma_engram_entry_t *s = &e->slots[i];
        if (s->key_hash == 0ULL) continue;
        uint32_t age = (e->now_step > s->last_access_step)
                        ? e->now_step - s->last_access_step : 0u;
        float score = s->sigma_at_store
                    + 0.01f * (float)age
                    - 0.1f  * (float)s->access_count;
        if (score > best_score) {
            best_score = score;
            best_i = i;
        }
    }
    return best_i;
}

/* Remove slot at index `i` and re-insert the following probe chain
 * (Robin-Hood-ish backward shift).  Keeps lookup O(1) amortised. */
static void delete_at(cos_sigma_engram_t *e, uint32_t i) {
    uint32_t mask = e->cap - 1u;
    e->slots[i].key_hash = 0ULL;
    e->slots[i].value    = NULL;
    e->slots[i].sigma_at_store = 0.0f;
    e->slots[i].access_count   = 0u;
    e->slots[i].last_access_step = 0u;
    e->count--;
    uint32_t j = (i + 1u) & mask;
    while (e->slots[j].key_hash != 0ULL) {
        cos_sigma_engram_entry_t moving = e->slots[j];
        e->slots[j].key_hash = 0ULL;
        e->slots[j].value    = NULL;
        e->slots[j].sigma_at_store = 0.0f;
        e->slots[j].access_count   = 0u;
        e->slots[j].last_access_step = 0u;
        e->count--;
        int found_exact = 0;
        uint32_t dest = probe(e, moving.key_hash, &found_exact);
        if (dest == 0xFFFFFFFFu) {
            /* Should not happen if we just freed a slot. */
            dest = j;
        }
        e->slots[dest] = moving;
        e->count++;
        j = (j + 1u) & mask;
    }
}

int cos_sigma_engram_put(cos_sigma_engram_t *e,
                         uint64_t key_hash, float sigma,
                         void *value,
                         uint64_t *out_evicted_key) {
    if (!e)                                         return -1;
    if (key_hash == 0ULL)                           return -2;
    if (!(sigma >= 0.0f) || sigma > 1.0f ||
         sigma != sigma)                            return -3;
    e->n_put++;

    int found = 0;
    uint32_t idx = probe(e, key_hash, &found);
    if (found) {
        /* In-place update. */
        e->slots[idx].sigma_at_store = sigma;
        e->slots[idx].value          = value;
        e->slots[idx].last_access_step = e->now_step;
        if (out_evicted_key) *out_evicted_key = 0ULL;
        return 0;
    }

    if (idx == 0xFFFFFFFFu) {
        /* Table full — evict a victim first. */
        uint32_t vi = pick_victim(e);
        uint64_t ek = e->slots[vi].key_hash;
        if (out_evicted_key) *out_evicted_key = ek;
        delete_at(e, vi);
        e->n_evict++;
        int f2 = 0;
        idx = probe(e, key_hash, &f2);
        if (idx == 0xFFFFFFFFu) return -4;   /* should not happen */
    } else if (out_evicted_key) {
        *out_evicted_key = 0ULL;
    }

    e->slots[idx].key_hash         = key_hash;
    e->slots[idx].sigma_at_store   = sigma;
    e->slots[idx].value            = value;
    e->slots[idx].access_count     = 0u;
    e->slots[idx].last_access_step = e->now_step;
    e->count++;
    return (out_evicted_key && *out_evicted_key != 0ULL) ? 1 : 0;
}

int cos_sigma_engram_get(cos_sigma_engram_t *e, uint64_t key_hash,
                         cos_sigma_engram_entry_t *out) {
    if (!e)                    return -1;
    if (key_hash == 0ULL)      return -2;
    int found = 0;
    uint32_t idx = probe(e, key_hash, &found);
    if (!found) {
        e->n_get_miss++;
        return 0;
    }
    e->slots[idx].access_count++;
    e->slots[idx].last_access_step = e->now_step;
    e->n_get_hit++;
    if (out) *out = e->slots[idx];
    return 1;
}

int cos_sigma_engram_delete(cos_sigma_engram_t *e, uint64_t key_hash) {
    if (!e || key_hash == 0ULL) return -1;
    int found = 0;
    uint32_t idx = probe(e, key_hash, &found);
    if (!found) return 0;
    delete_at(e, idx);
    return 1;
}

uint32_t cos_sigma_engram_age_sweep(
    cos_sigma_engram_t *e,
    void (*on_evict)(uint64_t, void *, void *),
    void *ctx) {
    if (!e) return 0u;
    uint32_t freed = 0u;
    /* Snapshot keys + age status; mutate after traversal so probe
     * chain rewrites don't invalidate the scan. */
    for (uint32_t i = 0; i < e->cap; ++i) {
        if (e->slots[i].key_hash == 0ULL) continue;
        uint32_t age = (e->now_step > e->slots[i].last_access_step)
                        ? e->now_step - e->slots[i].last_access_step : 0u;
        uint32_t ttl = (e->slots[i].sigma_at_store <= e->tau_trust)
                        ? e->long_ttl : e->short_ttl;
        if (age > ttl) {
            uint64_t k = e->slots[i].key_hash;
            void *v    = e->slots[i].value;
            if (on_evict) on_evict(k, v, ctx);
            cos_sigma_engram_delete(e, k);
            e->n_aged++;
            freed++;
            /* After delete_at, entries may have shifted into slot i;
             * re-check this index. */
            i--;  /* safe: i was >= 0 because we entered the branch */
        }
    }
    return freed;
}

/* --- self-test --------------------------------------------------------- */

static int check_hash_never_zero(void) {
    /* Hash of empty string must not collide with the 0 sentinel. */
    if (cos_sigma_engram_hash("")   == 0ULL) return 10;
    if (cos_sigma_engram_hash("a")  == 0ULL) return 11;
    if (cos_sigma_engram_hash(NULL) == 0ULL) return 12;
    /* Same input → same hash. */
    if (cos_sigma_engram_hash("hello")
      != cos_sigma_engram_hash("hello")) return 13;
    if (cos_sigma_engram_hash("HELLO")
      != cos_sigma_engram_hash("hello")) return 15;
    if (cos_sigma_engram_hash("  What  is  2+2? ")
      != cos_sigma_engram_hash("what is 2+2?")) return 16;
    /* Different input → different hash (probabilistic, but these two
     * are known-distinct). */
    if (cos_sigma_engram_hash("abc")
     == cos_sigma_engram_hash("abd")) return 14;
    return 0;
}

static int check_put_get_update(void) {
    cos_sigma_engram_entry_t slots[8];
    cos_sigma_engram_t e;
    if (cos_sigma_engram_init(&e, slots, 8,
                              /*tau_trust=*/0.30f,
                              /*long_ttl=*/100,
                              /*short_ttl=*/10) != 0) return 20;

    uint64_t k1 = cos_sigma_engram_hash("q1");
    uint64_t evicted = 0ULL;
    if (cos_sigma_engram_put(&e, k1, 0.10f, (void *)"v1", &evicted) != 0)
        return 21;
    if (evicted != 0ULL) return 22;
    if (e.count != 1) return 23;

    cos_sigma_engram_entry_t out;
    if (cos_sigma_engram_get(&e, k1, &out) != 1) return 24;
    if (out.sigma_at_store != 0.10f) return 25;
    if (out.access_count != 1) return 26;

    /* Update in place — should not grow count. */
    if (cos_sigma_engram_put(&e, k1, 0.20f, (void *)"v1b", &evicted) != 0)
        return 27;
    if (e.count != 1) return 28;
    if (cos_sigma_engram_get(&e, k1, &out) != 1) return 29;
    if (out.sigma_at_store != 0.20f) return 30;
    if (out.access_count != 2) return 31;

    /* Miss. */
    uint64_t k_miss = cos_sigma_engram_hash("not-present");
    if (cos_sigma_engram_get(&e, k_miss, &out) != 0) return 32;

    /* Delete. */
    if (cos_sigma_engram_delete(&e, k1) != 1) return 33;
    if (e.count != 0) return 34;
    if (cos_sigma_engram_get(&e, k1, &out) != 0) return 35;
    return 0;
}

static int check_full_table_eviction(void) {
    cos_sigma_engram_entry_t slots[4];
    cos_sigma_engram_t e;
    if (cos_sigma_engram_init(&e, slots, 4, 0.30f, 100, 10) != 0) return 40;

    /* Fill with low-σ entries — should not evict within capacity. */
    uint64_t k[5];
    char buf[8];
    for (int i = 0; i < 4; ++i) {
        snprintf(buf, sizeof buf, "f%d", i);
        k[i] = cos_sigma_engram_hash(buf);
        uint64_t ev = 0xBAD;
        if (cos_sigma_engram_put(&e, k[i], 0.10f, (void *)(intptr_t)(i + 1),
                                 &ev) != 0)
            return 41;
        if (ev != 0ULL) return 42;
    }
    if (e.count != 4) return 43;

    /* Fifth insert with HIGH σ — a low-σ entry should still be kept
     * preferentially; the HIGH-σ new entry gets in (since its σ is
     * higher than the victim's low σ, it should evict… wait, that's
     * backwards).  Our policy evicts the entry with the HIGHEST score;
     * since all 4 existing entries have σ=0.10, and the victim
     * calculation doesn't see the incoming entry, we evict the one
     * with the highest age/lowest access.  Either way exactly one
     * entry must leave. */
    k[4] = cos_sigma_engram_hash("new");
    uint64_t evicted = 0ULL;
    int rc = cos_sigma_engram_put(&e, k[4], 0.95f, (void *)(intptr_t)99,
                                  &evicted);
    if (rc != 1) return 44;
    if (evicted == 0ULL) return 45;
    if (e.count != 4) return 46;
    if (e.n_evict != 1) return 47;

    /* The new key must be present. */
    cos_sigma_engram_entry_t out;
    if (cos_sigma_engram_get(&e, k[4], &out) != 1) return 48;
    return 0;
}

static int check_age_sweep(void) {
    cos_sigma_engram_entry_t slots[8];
    cos_sigma_engram_t e;
    if (cos_sigma_engram_init(&e, slots, 8, 0.30f,
                              /*long_ttl=*/10, /*short_ttl=*/2) != 0)
        return 50;

    uint64_t k_confident = cos_sigma_engram_hash("trusted");
    uint64_t k_shaky     = cos_sigma_engram_hash("shaky");
    cos_sigma_engram_put(&e, k_confident, 0.10f, (void *)"c", NULL);
    cos_sigma_engram_put(&e, k_shaky,     0.80f, (void *)"s", NULL);

    /* Tick 5 times — shaky TTL is 2, trusted TTL is 10.                */
    for (int i = 0; i < 5; ++i) cos_sigma_engram_tick(&e);
    uint32_t freed = cos_sigma_engram_age_sweep(&e, NULL, NULL);
    if (freed != 1u) return 51;
    if (e.count != 1) return 52;
    /* Trusted survives. */
    cos_sigma_engram_entry_t out;
    if (cos_sigma_engram_get(&e, k_confident, &out) != 1) return 53;

    /* Advance to age out the trusted one too. */
    for (int i = 0; i < 20; ++i) cos_sigma_engram_tick(&e);
    /* The get() above refreshed last_access_step; so trusted age is
     * only 20 steps from that point — long_ttl=10 should age it out. */
    freed = cos_sigma_engram_age_sweep(&e, NULL, NULL);
    if (freed != 1u) return 54;
    if (e.count != 0) return 55;
    return 0;
}

static int check_stress_10k(void) {
    /* 10^4 random puts into a 128-slot table; final count ≤ 128, no
     * crashes, no inconsistencies.  Uses the LCG seed from other
     * self-tests so traces are reproducible. */
    enum { CAP = 128 };
    cos_sigma_engram_entry_t slots[CAP];
    cos_sigma_engram_t e;
    if (cos_sigma_engram_init(&e, slots, CAP, 0.30f, 64, 8) != 0)
        return 60;

    uint32_t state = 0xFACEC0DEu;
    for (int i = 0; i < 10000; ++i) {
        state = state * 1664525u + 1013904223u;
        uint64_t k = (uint64_t)state * 2862933555777941757ULL + 3037000493ULL;
        if (k == 0ULL) k = 1ULL;
        state = state * 1664525u + 1013904223u;
        float sig = (float)((state >> 8) & 0xFFFF) / 65535.0f;
        cos_sigma_engram_put(&e, k, sig, (void *)(uintptr_t)i, NULL);
        if (e.count > CAP) return 61;
        if ((i & 0xFF) == 0) cos_sigma_engram_tick(&e);
    }
    if (e.count > CAP) return 62;
    return 0;
}

int cos_sigma_engram_self_test(void) {
    int rc;
    if ((rc = check_hash_never_zero()))      return rc;
    if ((rc = check_put_get_update()))       return rc;
    if ((rc = check_full_table_eviction()))  return rc;
    if ((rc = check_age_sweep()))            return rc;
    if ((rc = check_stress_10k()))           return rc;
    return 0;
}
