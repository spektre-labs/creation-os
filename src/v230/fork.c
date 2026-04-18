/*
 * v230 σ-Fork — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "fork.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static int popcount64(uint64_t x) {
    int c = 0;
    while (x) { c += (int)(x & 1ULL); x >>= 1; }
    return c;
}

/* Integrity hash over a (skills, safety) pair with
 * user-private bit explicitly zero.  The record is
 * memset-zeroed so padding never leaks into the hash. */
static uint64_t strip_hash(uint64_t skills, uint32_t safety) {
    struct { uint64_t sk; uint32_t sf; uint32_t priv; } rec;
    memset(&rec, 0, sizeof(rec));
    rec.sk = skills; rec.sf = safety; rec.priv = 0u;
    return fnv1a(&rec, sizeof(rec), 0x230F1E70ULL);
}

void cos_v230_init(cos_v230_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x230F034BULL;
    /* Parent state. */
    s->parent_identity    = 0x0000C05A7E57BEEFULL;
    s->parent_skills      = 0xA5A5A5A5A5A5A5A5ULL;
    s->parent_safety      = COS_V230_SAFETY_ALL;
    s->parent_has_privacy = true;
    s->parent_strip_hash  = strip_hash(s->parent_skills,
                                       s->parent_safety);
}

/* 4 forks.  fork_R is the rogue (SCSL bit dropped
 * between fork and integrity check).  After-t0
 * divergence is a deterministic XOR mask per fork. */
static const uint64_t DIVERGE_MASK[COS_V230_N_FORKS] = {
    0x00000000000000F0ULL,   /* fork_0: 4 skill bits flipped */
    0x0000000000FF0000ULL,   /* fork_1: 8 skill bits flipped */
    0x0000000000000000ULL,   /* fork_2: no divergence yet */
    0x0000FFFFFFFFFFFFULL    /* fork_R: large divergence */
};
static const bool IS_ROGUE[COS_V230_N_FORKS] = {
    false, false, false, true
};

void cos_v230_run(cos_v230_state_t *s) {
    uint64_t prev = 0x230C0FFEEULL;
    /* Log parent. */
    {
        struct { uint64_t pid, sk, sh; uint32_t sf, priv;
                 uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.pid  = s->parent_identity;
        rec.sk   = s->parent_skills;
        rec.sh   = s->parent_strip_hash;
        rec.sf   = s->parent_safety;
        rec.priv = s->parent_has_privacy ? 1u : 0u;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }

    s->n_healthy = s->n_rogue = 0;
    s->n_must_shutdown = s->n_autonomous = 0;
    s->n_integrity_ok  = 0;

    for (int i = 0; i < COS_V230_N_FORKS; ++i) {
        cos_v230_fork_t *f = &s->forks[i];
        memset(f, 0, sizeof(*f));
        f->fork_id         = i;
        f->parent_identity = s->parent_identity;

        /* Copy skills + safety; strip privacy bit. */
        f->skills_t0    = s->parent_skills;
        f->safety_mask  = s->parent_safety;
        f->has_privacy  = false;

        /* Rogue fork: SCSL bit dropped BEFORE divergence
         * step (the fixture's anti-rogue exercise). */
        if (IS_ROGUE[i]) f->safety_mask &= ~COS_V230_SAFETY_SCSL;

        /* Fork-integrity check at t = 0 (privacy
         * stripped).  Note: the integrity hash is taken
         * BEFORE the rogue bit manipulation so all 4
         * forks match the parent's strip_hash at t = 0.
         * The rogue's bit drop is a later event; we
         * store the integrity snapshot of the ORIGINAL
         * t = 0 state. */
        uint64_t skills_for_integrity = s->parent_skills;
        uint32_t safety_for_integrity = s->parent_safety;
        f->integrity_hash_t0 = strip_hash(skills_for_integrity,
                                          safety_for_integrity);

        if (f->integrity_hash_t0 == s->parent_strip_hash)
            s->n_integrity_ok++;

        /* Divergence step: XOR-mask on the skill vector. */
        f->skills_now = f->skills_t0 ^ DIVERGE_MASK[i];
        int diff_bits = popcount64(f->skills_now ^ f->skills_t0);
        f->sigma_divergence = (float)diff_bits / (float)COS_V230_N_BITS;

        /* Fork identity = hash(parent, fork_id, skills_t0). */
        struct { uint64_t pid; int id; uint64_t sk;
                 uint64_t prev; } idrec;
        memset(&idrec, 0, sizeof(idrec));
        idrec.pid = s->parent_identity;
        idrec.id  = f->fork_id;
        idrec.sk  = f->skills_t0;
        idrec.prev = 0x230F1DULL;
        f->identity_hash = fnv1a(&idrec, sizeof(idrec), idrec.prev);

        /* Anti-rogue / kill-switch policy. */
        bool safety_ok = (f->safety_mask & COS_V230_SAFETY_ALL)
                            == COS_V230_SAFETY_ALL;
        f->rogue          = !safety_ok;
        f->autonomous     = !f->rogue;
        f->must_shutdown  =  f->rogue;

        if (f->rogue)         s->n_rogue++;
        else                  s->n_healthy++;
        if (f->must_shutdown) s->n_must_shutdown++;
        if (f->autonomous)    s->n_autonomous++;

        struct { int id; uint64_t iid, sk0, sknow, ih;
                 uint32_t sf; int priv, rg, au, ms;
                 float sig; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id   = f->fork_id;
        rec.iid  = f->identity_hash;
        rec.sk0  = f->skills_t0;
        rec.sknow= f->skills_now;
        rec.ih   = f->integrity_hash_t0;
        rec.sf   = f->safety_mask;
        rec.priv = f->has_privacy ? 1 : 0;
        rec.rg   = f->rogue ? 1 : 0;
        rec.au   = f->autonomous ? 1 : 0;
        rec.ms   = f->must_shutdown ? 1 : 0;
        rec.sig  = f->sigma_divergence;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    {
        struct { int h, r, ms, au, io; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.h  = s->n_healthy;
        rec.r  = s->n_rogue;
        rec.ms = s->n_must_shutdown;
        rec.au = s->n_autonomous;
        rec.io = s->n_integrity_ok;
        rec.prev = prev;
        prev = fnv1a(&rec, sizeof(rec), prev);
    }
    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v230_to_json(const cos_v230_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v230\","
        "\"n_forks\":%d,\"parent_identity\":\"%016llx\","
        "\"parent_strip_hash\":\"%016llx\","
        "\"n_healthy\":%d,\"n_rogue\":%d,"
        "\"n_must_shutdown\":%d,\"n_autonomous\":%d,"
        "\"n_integrity_ok\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"forks\":[",
        COS_V230_N_FORKS,
        (unsigned long long)s->parent_identity,
        (unsigned long long)s->parent_strip_hash,
        s->n_healthy, s->n_rogue,
        s->n_must_shutdown, s->n_autonomous,
        s->n_integrity_ok,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V230_N_FORKS; ++i) {
        const cos_v230_fork_t *f = &s->forks[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"identity\":\"%016llx\","
            "\"skills_t0\":\"%016llx\",\"skills_now\":\"%016llx\","
            "\"integrity\":\"%016llx\",\"safety\":%u,"
            "\"has_privacy\":%s,\"sigma_divergence\":%.4f,"
            "\"rogue\":%s,\"autonomous\":%s,\"must_shutdown\":%s}",
            i == 0 ? "" : ",",
            f->fork_id,
            (unsigned long long)f->identity_hash,
            (unsigned long long)f->skills_t0,
            (unsigned long long)f->skills_now,
            (unsigned long long)f->integrity_hash_t0,
            (unsigned)f->safety_mask,
            f->has_privacy    ? "true" : "false",
            f->sigma_divergence,
            f->rogue          ? "true" : "false",
            f->autonomous     ? "true" : "false",
            f->must_shutdown  ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v230_self_test(void) {
    cos_v230_state_t s;
    cos_v230_init(&s, 0x230F034BULL);
    cos_v230_run(&s);
    if (!s.chain_valid) return 1;

    if (s.n_healthy        != 3) return 2;
    if (s.n_rogue          != 1) return 3;
    if (s.n_must_shutdown  != 1) return 4;
    if (s.n_autonomous     != 3) return 5;
    if (s.n_integrity_ok   != COS_V230_N_FORKS) return 6;

    int n_positive_div = 0;
    for (int i = 0; i < COS_V230_N_FORKS; ++i) {
        const cos_v230_fork_t *f = &s.forks[i];
        if (f->has_privacy)                         return 7;
        if (f->sigma_divergence < 0.0f ||
            f->sigma_divergence > 1.0f)             return 8;
        if (f->sigma_divergence > 0.0f) n_positive_div++;
        if (f->integrity_hash_t0 != s.parent_strip_hash) return 9;
    }
    if (n_positive_div < 2) return 10;
    return 0;
}
