/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v61 — Σ-Citadel implementation.
 *
 * Hardware discipline:
 *   - no dynamic allocation on lattice or quote hot paths
 *   - branchless lattice compare (mask cascade)
 *   - constant-time 256-bit quote equality (no early-exit)
 *   - 64-byte aligned attestation buffers
 *   - no syscalls, no libc beyond <string.h> memcpy/memset
 */

#include "citadel.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(COS_V61_LIBSODIUM)
#include <sodium.h>
#endif

/* ------------------------------------------------------------------ */
/* 1. Lattice                                                          */
/* ------------------------------------------------------------------ */

void cos_v61_lattice_policy_default(cos_v61_lattice_policy_t *out)
{
    if (!out) return;
    out->min_integrity = 0;  /* no quarantine by default */
    out->_pad[0] = out->_pad[1] = out->_pad[2] = 0;
    out->_reserved = 0;
}

/* Branchless ops → lanes. */
static inline int v61_lattice_core(cos_v61_label_t s, cos_v61_label_t o, int op)
{
    /* All three lanes evaluated unconditionally (no short-circuit). */
    int read_allowed  = (s.clearance >= o.clearance)   /* BLP no-read-up   */
                      & (s.integrity <= o.integrity);  /* Biba no-read-dn  */
    int write_allowed = (s.clearance <= o.clearance)   /* BLP no-write-dn  */
                      & (s.integrity >= o.integrity);  /* Biba no-write-up */
    int compart_ok    = ((s.compartments & o.compartments) == o.compartments);

    /* Op-selection via 0/1 masks; unknown op → 0. */
    int m_read   = (op == COS_V61_OP_READ);
    int m_write  = (op == COS_V61_OP_WRITE);
    int m_exec   = (op == COS_V61_OP_EXEC);
    int m_known  = m_read | m_write | m_exec;

    /* EXEC requires both (conservative). */
    int exec_allowed = read_allowed & write_allowed;

    int allowed_lane = (read_allowed  & m_read)
                     | (write_allowed & m_write)
                     | (exec_allowed  & m_exec);

    return allowed_lane & compart_ok & m_known;
}

int cos_v61_lattice_check(cos_v61_label_t s, cos_v61_label_t o, int op)
{
    return v61_lattice_core(s, o, op);
}

int cos_v61_lattice_check_policy(cos_v61_label_t                 s,
                                 cos_v61_label_t                 o,
                                 int                             op,
                                 const cos_v61_lattice_policy_t *p)
{
    int core = v61_lattice_core(s, o, op);
    int quarantine_ok = p ? (o.integrity >= p->min_integrity) : 1;
    return core & quarantine_ok;
}

void cos_v61_lattice_check_batch(const cos_v61_label_t *subjs,
                                 const cos_v61_label_t *objs,
                                 const uint8_t         *ops,
                                 int32_t                n,
                                 uint8_t               *results_out)
{
    if (!subjs || !objs || !ops || !results_out || n <= 0) return;
    for (int32_t i = 0; i < n; ++i) {
        if (i + 16 < n) {
            __builtin_prefetch(&subjs[i + 16], 0, 3);
            __builtin_prefetch(&objs[i + 16],  0, 3);
            __builtin_prefetch(&ops[i + 16],   0, 3);
            __builtin_prefetch(&results_out[i + 16], 1, 3);
        }
        results_out[i] = (uint8_t)v61_lattice_core(subjs[i], objs[i],
                                                   (int)ops[i]);
    }
}

/* ------------------------------------------------------------------ */
/* 2. Attestation quote                                                */
/* ------------------------------------------------------------------ */

/* Deterministic 256-bit quote.  Four-lane XOR-fold: each 8-byte word
 * of the input contributes to all four 64-bit lanes with independent
 * mixing constants, producing a 32-byte output that is deterministic
 * and *equality-sensitive to every input bit*.  This is NOT a MAC —
 * for cryptographic attestation layer libsodium crypto_generichash on
 * top, enabled via `-DCOS_V61_LIBSODIUM -lsodium`. */
static void v61_quote_xor_fold(const cos_v61_attest_input_t *in,
                               cos_v61_quote256_t           *out)
{
    /* Pack the 5 × 64-bit inputs into an 8×64-bit canonical buffer
     * (zero-extend unused trailing padding).  Deterministic over
     * little-endian hosts; quote is compared byte-for-byte so
     * architecture-stable as long as the sender and verifier agree
     * on endianness (which they do on M4 + x86-64). */
    uint64_t w[8];
    w[0] = in->code_page_hash;
    w[1] = in->caps_state_hash;
    w[2] = in->sigma_state_hash;
    w[3] = in->lattice_hash;
    w[4] = in->nonce;
    w[5] = in->_pad[0];
    w[6] = in->_pad[1];
    w[7] = in->_pad[2];

    static const uint64_t mix[4] = {
        0xBF58476D1CE4E5B9ULL,
        0x94D049BB133111EBULL,
        0x9E3779B97F4A7C15ULL,
        0xD6E8FEB86659FD93ULL,
    };

    uint64_t lane[4] = { mix[0], mix[1], mix[2], mix[3] };
    for (int i = 0; i < 8; ++i) {
        for (int L = 0; L < 4; ++L) {
            lane[L] ^= w[i] + ((uint64_t)i * mix[L]);
            lane[L]  = (lane[L] << 27) | (lane[L] >> 37);
            lane[L] *= mix[(L + 1) & 3];
            lane[L] ^= lane[L] >> 31;
        }
    }
    memcpy(out->bytes +  0, &lane[0], 8);
    memcpy(out->bytes +  8, &lane[1], 8);
    memcpy(out->bytes + 16, &lane[2], 8);
    memcpy(out->bytes + 24, &lane[3], 8);
}

void cos_v61_quote256(const cos_v61_attest_input_t *in,
                      cos_v61_quote256_t           *out)
{
    if (!in || !out) return;
#if defined(COS_V61_LIBSODIUM)
    /* BLAKE2b-256 over the canonical 64-byte input.  Matches offline
     * verifiers that compute `crypto_generichash(32, input, 64)`. */
    unsigned char buf[64];
    memset(buf, 0, sizeof(buf));
    memcpy(buf +  0, &in->code_page_hash,    8);
    memcpy(buf +  8, &in->caps_state_hash,   8);
    memcpy(buf + 16, &in->sigma_state_hash,  8);
    memcpy(buf + 24, &in->lattice_hash,      8);
    memcpy(buf + 32, &in->nonce,             8);
    memcpy(buf + 40, &in->_pad[0],          24);
    crypto_generichash(out->bytes, sizeof(out->bytes), buf, sizeof(buf),
                       NULL, 0);
#else
    v61_quote_xor_fold(in, out);
#endif
}

int cos_v61_ct_equal256(const cos_v61_quote256_t *a,
                        const cos_v61_quote256_t *b)
{
    if (!a || !b) return 0;
    uint64_t d = 0;
    const uint64_t *wa = (const uint64_t *)a->bytes;
    const uint64_t *wb = (const uint64_t *)b->bytes;
    for (int i = 0; i < 4; ++i) d |= (wa[i] ^ wb[i]);
    d |= d >> 32;
    d |= d >> 16;
    d |= d >> 8;
    d |= d >> 4;
    d |= d >> 2;
    d |= d >> 1;
    return (int)(1 ^ (d & 1));
}

void cos_v61_quote256_hex(const cos_v61_quote256_t *q, char *out64p1)
{
    if (!q || !out64p1) return;
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out64p1[2 * i + 0] = hex[(q->bytes[i] >> 4) & 0xF];
        out64p1[2 * i + 1] = hex[(q->bytes[i]     ) & 0xF];
    }
    out64p1[64] = '\0';
}

/* ------------------------------------------------------------------ */
/* 3. Composition with v60                                             */
/* ------------------------------------------------------------------ */

int cos_v61_compose(uint8_t            v60_decision,
                    int                lattice_allowed,
                    cos_v61_compose_t *out)
{
    if (!out) return -1;
    /* v60_decision == 1 means ALLOW; any other 1..5 means a DENY_*. */
    int v60_allowed = (v60_decision == 1);
    int latt_ok     = (lattice_allowed != 0);
    /* 4-way mux, branchless. */
    int m_allow       =  v60_allowed &  latt_ok;
    int m_deny_v60    = (!v60_allowed) &  latt_ok;
    int m_deny_latt   =  v60_allowed  & (!latt_ok);
    int m_deny_both   = (!v60_allowed) & (!latt_ok);
    uint8_t d = (uint8_t)(
            (COS_V61_COMPOSE_ALLOW        * m_allow)
          | (COS_V61_COMPOSE_DENY_V60     * m_deny_v60)
          | (COS_V61_COMPOSE_DENY_LATTICE * m_deny_latt)
          | (COS_V61_COMPOSE_DENY_BOTH    * m_deny_both));
    out->decision = d;
    memset(out->_pad, 0, sizeof(out->_pad));
    out->_reserved = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 4. Utilities                                                        */
/* ------------------------------------------------------------------ */

cos_v61_attest_input_t *cos_v61_alloc_attest_inputs(int32_t n)
{
    if (n <= 0) return NULL;
    size_t bytes = (size_t)n * sizeof(cos_v61_attest_input_t);
    size_t aligned = (bytes + 63u) & ~(size_t)63u;
    void *m = aligned_alloc(64, aligned);
    if (!m) return NULL;
    memset(m, 0, aligned);
    return (cos_v61_attest_input_t *)m;
}

cos_v61_version_t cos_v61_version(void)
{
    cos_v61_version_t v = { 61, 0, 1 };
    return v;
}

const char *cos_v61_compose_tag(uint8_t d)
{
    switch (d) {
        case COS_V61_COMPOSE_ALLOW:        return "ALLOW";
        case COS_V61_COMPOSE_DENY_V60:     return "DENY_V60";
        case COS_V61_COMPOSE_DENY_LATTICE: return "DENY_LATTICE";
        case COS_V61_COMPOSE_DENY_BOTH:    return "DENY_BOTH";
        default:                           return "?";
    }
}
