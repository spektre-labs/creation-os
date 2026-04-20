/*
 * σ-Engram — O(1) σ-aware context-memory cache (v260.1 live).
 *
 * v260 declared the Engram contract (20–25% static / 75–80% dynamic,
 * O(1) DRAM hash lookup, σ-gated USE vs VERIFY) but did NOT execute
 * real lookups — its v0 header says "v260.1 named, not implemented".
 * This primitive is the live kernel.
 *
 * What it does:
 *
 *   * Fixed-capacity hash-map keyed by FNV-1a-64 of the prompt.
 *   * Each entry stores (key_hash, sigma_at_store, value pointer,
 *     access_count, last_access_step) — no embedding bytes are held
 *     inside the primitive; callers manage the value buffer lifetime.
 *   * Open-addressing with linear probing.  Power-of-two capacity so
 *     `hash & (cap − 1)` replaces modulus — O(1) lookup in practice.
 *   * σ-aware aging:
 *
 *         ttl(σ_stored, σ_now_threshold) :=
 *             (σ_stored ≤ τ_trust) ? LONG_TTL : SHORT_TTL
 *
 *     A confidently-stored entry lives long; an uncertainly-stored
 *     entry is ephemeral.  The sweep is opt-in (caller calls
 *     engram_age_sweep()).
 *   * Eviction on insert-into-full: kick the entry with the highest
 *     `σ_at_store + (age_bonus − access_bonus)` score — i.e. prefer
 *     to keep confident, frequently-used entries.
 *
 * This primitive does not cache embedding vectors — that's a caller
 * concern (see `scripts/sigma_pipeline/engram.py` for the Python side
 * that pairs FNV-1a-64 with generated-text bodies).  The C kernel is
 * small, allocation-free once initialised, and callable from both C
 * and Python.
 *
 * Contracts (v0, discharged at runtime):
 *   1. put() of a fresh key stores the entry; get() returns hit.
 *   2. get() of an unknown key returns miss (value pointer NULL).
 *   3. put() of an existing key updates the entry in place AND
 *      increments access_count on subsequent get()s (no duplicate
 *      slots).
 *   4. Insertion into a full table evicts exactly one entry and
 *      returns the eviction's key_hash so the caller can GC the
 *      associated value buffer.
 *   5. age_sweep() removes every entry whose age (now_step −
 *      last_access_step) exceeds the σ-derived TTL.  Returns the
 *      number of slots freed.
 *   6. Self-test: 10^4-row stress + canonical 8-row hit/miss table +
 *      capacity-overflow + age-sweep scenarios.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ENGRAM_H
#define COS_SIGMA_ENGRAM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Per-entry payload.  Caller-opaque value pointer (NULL means free
 * slot AND zero key_hash — both sentinels). */
typedef struct {
    uint64_t key_hash;
    float    sigma_at_store;
    uint32_t access_count;
    uint32_t last_access_step;
    void    *value;
} cos_sigma_engram_entry_t;

typedef struct {
    cos_sigma_engram_entry_t *slots;
    uint32_t cap;             /* must be power of two                  */
    uint32_t count;           /* live entries                          */
    uint32_t now_step;        /* monotonic logical clock               */
    float    tau_trust;       /* σ ≤ tau_trust  ⇒ LONG_TTL             */
    uint32_t long_ttl;        /* in logical steps                      */
    uint32_t short_ttl;
    /* Counters for self-test / observability. */
    uint32_t n_put;
    uint32_t n_get_hit;
    uint32_t n_get_miss;
    uint32_t n_evict;
    uint32_t n_aged;
} cos_sigma_engram_t;

/* Hash a NUL-terminated string with FNV-1a-64.  Never returns 0
 * (collides with the "free slot" sentinel) — collides to 1 if the raw
 * hash is 0. */
uint64_t cos_sigma_engram_hash(const char *s);

/* Initialise an engram over caller-provided slot array.  `cap` must be
 * a power of two ≥ 4.  Returns 0 on success, negative on error. */
int cos_sigma_engram_init(cos_sigma_engram_t *e,
                          cos_sigma_engram_entry_t *slots, uint32_t cap,
                          float tau_trust,
                          uint32_t long_ttl, uint32_t short_ttl);

/* Advance the logical clock.  Call at most once per turn; get() also
 * uses the current now_step for last_access_step updates. */
void cos_sigma_engram_tick(cos_sigma_engram_t *e);

/* Store (or update) an entry.
 *
 *   If the table is full AND key is new, returns 1 and writes the
 *   evicted entry's key_hash into `*out_evicted_key` so the caller can
 *   GC its value.  Returns 0 on a fresh insert or in-place update.
 *   Returns negative on arg error. */
int cos_sigma_engram_put(cos_sigma_engram_t *e,
                         uint64_t key_hash, float sigma,
                         void *value,
                         uint64_t *out_evicted_key);

/* Lookup.  On hit returns 1 and writes a COPY of the entry into `*out`
 * (so callers see value pointer + access count); bumps access_count
 * and last_access_step.  On miss returns 0 and leaves *out untouched.
 * Returns negative on arg error.
 *
 * `out` MAY be NULL: the access-count bump still happens. */
int cos_sigma_engram_get(cos_sigma_engram_t *e, uint64_t key_hash,
                         cos_sigma_engram_entry_t *out);

/* Explicit delete.  Returns 1 if a slot was freed, 0 if not present. */
int cos_sigma_engram_delete(cos_sigma_engram_t *e, uint64_t key_hash);

/* Sweep entries older than their σ-derived TTL.  Returns the number of
 * slots freed.  Optionally emits each evicted key_hash via the
 * user-supplied callback (may be NULL). */
uint32_t cos_sigma_engram_age_sweep(
    cos_sigma_engram_t *e,
    void (*on_evict)(uint64_t key_hash, void *value, void *ctx),
    void *ctx);

/* Canonical self-test. */
int cos_sigma_engram_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_ENGRAM_H */
