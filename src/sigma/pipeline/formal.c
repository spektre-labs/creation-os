/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Formal — runtime proof harness for T3 / T4 / T5 / T6. */

#include "formal.h"
#include "substrate.h"
#include "protocol.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

/* ----------- RNG (deterministic xorshift64) ----------- */

static uint64_t xrng_state = 0x9E3779B97F4A7C15ULL;

static void xrng_seed(uint64_t s) {
    if (s == 0) s = 0x9E3779B97F4A7C15ULL;
    xrng_state = s;
}
static uint64_t xrng_u64(void) {
    uint64_t x = xrng_state;
    x ^= x << 13;  x ^= x >> 7;  x ^= x << 17;
    xrng_state = x;
    return x;
}
static float xrng_unit(void) {
    return (float)(xrng_u64() >> 11) / (float)(1ULL << 53);
}

/* ----------- T3: monotonicity of gate over τ ----------- */

int cos_sigma_formal_check_T3(cos_formal_result_t *out) {
    xrng_seed(0xC0FFEE11);
    uint32_t witnesses = 0, violations = 0;
    for (int i = 0; i < 16384; ++i) {
        float sigma = xrng_unit();
        float tau1  = xrng_unit();
        float tau2  = xrng_unit();
        if (tau2 < tau1) { float t = tau1; tau1 = tau2; tau2 = t; }
        int g1 = cos_sigma_substrate_default_gate(sigma, tau1);
        int g2 = cos_sigma_substrate_default_gate(sigma, tau2);
        witnesses++;
        /* τ₁ ≤ τ₂ ∧ gate(σ, τ₁) ⇒ gate(σ, τ₂). */
        if (g1 && !g2) violations++;
    }
    if (out) {
        out->theorem_id    = "T3";
        out->description   = "gate monotone in τ_accept";
        out->witnesses     = witnesses;
        out->violations    = violations;
        out->discharged    = (violations == 0);
        out->t6_median_ns  = 0;
        out->t6_p99_ns     = 0;
        out->t6_bound_ns   = 0;
    }
    return (violations == 0) ? 0 : 3;
}

/* ----------- T4: commutativity of independent gates ----------- */

int cos_sigma_formal_check_T4(cos_formal_result_t *out) {
    xrng_seed(0xDEADBEEF);
    uint32_t witnesses = 0, violations = 0;
    for (int i = 0; i < 16384; ++i) {
        float s1 = xrng_unit();
        float s2 = xrng_unit();
        float t  = xrng_unit();
        int a = cos_sigma_substrate_default_gate(s1, t)
             && cos_sigma_substrate_default_gate(s2, t);
        int b = cos_sigma_substrate_default_gate(s2, t)
             && cos_sigma_substrate_default_gate(s1, t);
        witnesses++;
        if (a != b) violations++;
    }
    if (out) {
        out->theorem_id    = "T4";
        out->description   = "gate(σ₁,τ) ∧ gate(σ₂,τ) commutes";
        out->witnesses     = witnesses;
        out->violations    = violations;
        out->discharged    = (violations == 0);
        out->t6_median_ns  = 0;
        out->t6_p99_ns     = 0;
        out->t6_bound_ns   = 0;
    }
    return (violations == 0) ? 0 : 4;
}

/* ----------- T5: encode/decode idempotence ----------- */

#define T5_BUFCAP 2048

static int t5_roundtrip_one(cos_msg_type_t type, const uint8_t *payload,
                            uint32_t payload_len)
{
    /* Deterministic Ed25519 keypair (CLOSE-3 default signer). */
    uint8_t seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i)
        seed[i] = (uint8_t)('f' + i);
    uint8_t pub[COS_ED25519_PUB_LEN];
    uint8_t priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);
    uint8_t sk[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sk, priv, COS_ED25519_PRIV_LEN);
    memcpy(sk + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    cos_msg_t m = {0};
    m.type          = type;
    m.sender_sigma  = 0.42f;
    m.timestamp_ns  = 1700000000000000000ULL;
    memcpy(m.sender_id, "formal-node", sizeof "formal-node");
    m.payload       = payload;
    m.payload_len   = payload_len;

    uint8_t f1[T5_BUFCAP]; size_t n1 = 0;
    if (cos_sigma_proto_encode(&m, sk, sizeof sk, f1, sizeof f1, &n1) != 0) return -1;

    cos_msg_t dec1 = {0};
    if (cos_sigma_proto_decode(f1, n1, pub, COS_ED25519_PUB_LEN, &dec1) != 0) return -2;

    cos_msg_t m2 = dec1;
    uint8_t f2[T5_BUFCAP]; size_t n2 = 0;
    if (cos_sigma_proto_encode(&m2, sk, sizeof sk, f2, sizeof f2, &n2) != 0) return -3;

    cos_msg_t dec2 = {0};
    if (cos_sigma_proto_decode(f2, n2, pub, COS_ED25519_PUB_LEN, &dec2) != 0) return -4;

    /* Idempotence: dec2 is bit-for-bit the same observable as dec1. */
    if (dec1.type          != dec2.type)          return -5;
    if (dec1.payload_len   != dec2.payload_len)   return -6;
    if (dec1.sender_sigma  != dec2.sender_sigma)  return -7;
    if (dec1.timestamp_ns  != dec2.timestamp_ns)  return -8;
    if (memcmp(dec1.sender_id, dec2.sender_id, sizeof dec1.sender_id) != 0) return -9;
    if (dec1.payload_len > 0
        && memcmp(dec1.payload, dec2.payload, dec1.payload_len) != 0) return -10;
    /* Ed25519 deterministic signatures (SHA-512 nonce, no randomness)
     * mean the two frames are byte-identical. */
    if (n1 != n2)            return -11;
    if (memcmp(f1, f2, n1))  return -12;
    return 0;
}

int cos_sigma_formal_check_T5(cos_formal_result_t *out) {
    xrng_seed(0xF00DD00D);
    uint32_t witnesses = 0, violations = 0;
    static const cos_msg_type_t types[7] = {
        COS_MSG_QUERY, COS_MSG_RESPONSE, COS_MSG_HEARTBEAT,
        COS_MSG_CAPABILITY, COS_MSG_FEDERATION,
        COS_MSG_ENGRAM_SHARE, COS_MSG_UNLEARN
    };
    for (int iter = 0; iter < 128; ++iter) {
        for (int t = 0; t < 7; ++t) {
            uint8_t payload[256];
            uint32_t plen = (uint32_t)(xrng_u64() % sizeof payload);
            for (uint32_t k = 0; k < plen; ++k)
                payload[k] = (uint8_t)(xrng_u64() & 0xFF);
            witnesses++;
            if (t5_roundtrip_one(types[t], payload, plen) != 0) violations++;
        }
    }
    if (out) {
        out->theorem_id    = "T5";
        out->description   = "decode ∘ encode is idempotent";
        out->witnesses     = witnesses;
        out->violations    = violations;
        out->discharged    = (violations == 0);
        out->t6_median_ns  = 0;
        out->t6_p99_ns     = 0;
        out->t6_bound_ns   = 0;
    }
    return (violations == 0) ? 0 : 5;
}

/* ----------- T6: per-call latency bound ----------- */

static uint64_t now_ns(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t) != 0) return 0;
    return (uint64_t)t.tv_sec * 1000000000ULL + (uint64_t)t.tv_nsec;
}

/* Insertion-sort into a small buffer (N ≤ 64). */
static void isort(uint64_t *a, int n) {
    for (int i = 1; i < n; ++i) {
        uint64_t v = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > v) { a[j + 1] = a[j]; j--; }
        a[j + 1] = v;
    }
}

int cos_sigma_formal_check_T6(cos_formal_result_t *out) {
    /* Repeated batches; record the per-call median & p99 across
     * a population of batch timings. */
    enum { N_BATCHES = 64, N_PER_BATCH = 100000 };
    volatile int accept_accum = 0;
    uint64_t per_call_ns[N_BATCHES];

    xrng_seed(0xBADC0DE);
    /* Warm-up run. */
    for (int i = 0; i < N_PER_BATCH; ++i) {
        float s = xrng_unit(), t = xrng_unit();
        accept_accum += cos_sigma_substrate_default_gate(s, t);
    }

    for (int b = 0; b < N_BATCHES; ++b) {
        uint64_t t0 = now_ns();
        for (int i = 0; i < N_PER_BATCH; ++i) {
            float s = xrng_unit(), t = xrng_unit();
            accept_accum += cos_sigma_substrate_default_gate(s, t);
        }
        uint64_t t1 = now_ns();
        uint64_t delta_ns = (t1 >= t0) ? (t1 - t0) : 0;
        per_call_ns[b] = delta_ns / (uint64_t)N_PER_BATCH;
    }

    isort(per_call_ns, N_BATCHES);
    uint64_t median = per_call_ns[N_BATCHES / 2];
    uint64_t p99    = per_call_ns[(N_BATCHES * 99) / 100];
    if (p99 < per_call_ns[N_BATCHES - 1]) p99 = per_call_ns[N_BATCHES - 1];

    uint64_t bound = (uint64_t)COS_FORMAL_T6_NS_BOUND;
    int      pass  = (p99 < bound);

    if (out) {
        out->theorem_id    = "T6";
        out->description   = "execution_time(gate) < bound";
        out->witnesses     = (uint32_t)(N_BATCHES * N_PER_BATCH);
        out->violations    = pass ? 0 : 1;
        out->discharged    = pass;
        out->t6_median_ns  = median;
        out->t6_p99_ns     = p99;
        out->t6_bound_ns   = bound;
    }
    /* accept_accum prevents the optimiser from deleting the loop. */
    (void)accept_accum;
    return pass ? 0 : 6;
}

/* ----------- umbrella ----------- */

int cos_sigma_formal_check_all(cos_formal_result_t out[4]) {
    int rc = 0;
    int r;
    r = cos_sigma_formal_check_T3(&out[0]);
    if (r != 0 && rc == 0) rc = r;
    r = cos_sigma_formal_check_T4(&out[1]);
    if (r != 0 && rc == 0) rc = r;
    r = cos_sigma_formal_check_T5(&out[2]);
    if (r != 0 && rc == 0) rc = r;
    r = cos_sigma_formal_check_T6(&out[3]);
    if (r != 0 && rc == 0) rc = r;
    return rc;
}

int cos_sigma_formal_self_test(void) {
    cos_formal_result_t r[4];
    return cos_sigma_formal_check_all(r);
}
