/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * v181 σ-Audit — implementation.
 *
 *   Uses spektre_sha256 from the license kernel for hashing.
 *   The v181.0 attestation signature is a deterministic keyed
 *   SHA-256:  sig = SHA-256(key || self_hash).  v181.1 swaps
 *   it for an ed25519 signature without changing the wire
 *   layout (sig stays 32 B → 64 B in that release).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "audit.h"
#include "license_attest.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------- */
/* Helpers                                                        */
/* ------------------------------------------------------------- */

static void hex_of(const uint8_t *in, int n, char *out) {
    static const char H[] = "0123456789abcdef";
    for (int i = 0; i < n; ++i) {
        out[2*i]   = H[(in[i] >> 4) & 0xF];
        out[2*i+1] = H[in[i]        & 0xF];
    }
    out[2*n] = '\0';
}

static void canonicalize_into(const cos_v181_entry_t *e,
                              const uint8_t prev_hash[32],
                              spektre_sha256_ctx_t *ctx) {
    /* Canonical byte layout excludes self_hash and sig so
     * those fields can be written after hashing.  Endianness:
     * we feed little-endian packed copies of numeric fields. */
    uint8_t buf8;
    uint64_t id = e->id;
    spektre_sha256_update(ctx, &id, sizeof(id));
    spektre_sha256_update(ctx, e->timestamp, strlen(e->timestamp));
    spektre_sha256_update(ctx, e->input_hash,  32);
    spektre_sha256_update(ctx, e->output_hash, 32);
    /* floats as raw little-endian (host assumed LE on darwin-arm64/x86). */
    spektre_sha256_update(ctx, &e->sigma_product, sizeof(float));
    spektre_sha256_update(ctx, e->sigma_channels,
                          sizeof(float) * COS_V181_N_CHANNELS);
    buf8 = (uint8_t)e->decision;
    spektre_sha256_update(ctx, &buf8, 1);
    spektre_sha256_update(ctx, e->explanation, strlen(e->explanation));
    spektre_sha256_update(ctx, e->steering,    strlen(e->steering));
    spektre_sha256_update(ctx, e->specialist,  strlen(e->specialist));
    spektre_sha256_update(ctx, e->adapter_ver, strlen(e->adapter_ver));
    spektre_sha256_update(ctx, prev_hash, 32);
}

static void compute_self_hash(const cos_v181_entry_t *e,
                              const uint8_t prev_hash[32],
                              uint8_t out[32]) {
    spektre_sha256_ctx_t ctx;
    spektre_sha256_init(&ctx);
    canonicalize_into(e, prev_hash, &ctx);
    spektre_sha256_final(&ctx, out);
}

static void compute_sig(const uint8_t key[32],
                        const uint8_t self_hash[32],
                        uint8_t out[32]) {
    spektre_sha256_ctx_t ctx;
    spektre_sha256_init(&ctx);
    spektre_sha256_update(&ctx, key, 32);
    spektre_sha256_update(&ctx, self_hash, 32);
    spektre_sha256_final(&ctx, out);
}

/* ------------------------------------------------------------- */
/* Init / free                                                   */
/* ------------------------------------------------------------- */

int cos_v181_init(cos_v181_state_t *s, int capacity, uint64_t seed) {
    if (!s || capacity <= 0 || capacity > COS_V181_MAX_ENTRIES)
        return 1;
    memset(s, 0, sizeof(*s));
    s->entries = (cos_v181_entry_t *)calloc((size_t)capacity,
                                             sizeof(cos_v181_entry_t));
    if (!s->entries) return 2;
    s->n_entries = 0;
    s->seed      = seed ? seed : 0x181AFEDCULL;
    /* Derive key from seed deterministically. */
    uint64_t k0 = s->seed ^ 0xD00D5AFED00DULL;
    uint64_t k1 = s->seed + 0xC0FFEE000012345ULL;
    uint64_t k2 = s->seed * 0x9E3779B97F4A7C15ULL;
    uint64_t k3 = s->seed ^ k0 ^ k1 ^ k2;
    memcpy(s->key +  0, &k0, 8);
    memcpy(s->key +  8, &k1, 8);
    memcpy(s->key + 16, &k2, 8);
    memcpy(s->key + 24, &k3, 8);
    s->window                = 64;
    s->anomaly_rel_rise_pct  = 30.0f;
    s->anomaly_at            = -1;
    (void)capacity;
    return 0;
}

void cos_v181_free(cos_v181_state_t *s) {
    if (!s) return;
    free(s->entries); s->entries = NULL;
    s->n_entries = 0;
}

/* ------------------------------------------------------------- */
/* Append                                                        */
/* ------------------------------------------------------------- */

static void copy_str(char *dst, size_t cap, const char *src) {
    if (!src) { dst[0] = '\0'; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

int cos_v181_append(cos_v181_state_t *s,
                    const char *input,
                    const char *output,
                    float sigma_product,
                    const float channels[COS_V181_N_CHANNELS],
                    cos_v181_decision_t decision,
                    const char *explanation,
                    const char *steering,
                    const char *specialist,
                    const char *adapter_ver,
                    const char *timestamp) {
    if (!s || s->n_entries >= COS_V181_MAX_ENTRIES) return 1;

    cos_v181_entry_t *e = &s->entries[s->n_entries];
    memset(e, 0, sizeof(*e));
    e->id = (uint64_t)s->n_entries + 1;
    copy_str(e->timestamp, sizeof(e->timestamp), timestamp);

    spektre_sha256(input  ? input  : "",
                   input  ? strlen(input)  : 0, e->input_hash);
    spektre_sha256(output ? output : "",
                   output ? strlen(output) : 0, e->output_hash);

    e->sigma_product = sigma_product;
    if (channels)
        memcpy(e->sigma_channels, channels,
               sizeof(float) * COS_V181_N_CHANNELS);
    e->decision = decision;
    copy_str(e->explanation, sizeof(e->explanation), explanation);
    copy_str(e->steering,    sizeof(e->steering),    steering);
    copy_str(e->specialist,  sizeof(e->specialist),  specialist);
    copy_str(e->adapter_ver, sizeof(e->adapter_ver), adapter_ver);

    if (s->n_entries == 0)
        memset(e->prev_hash, 0, 32);
    else
        memcpy(e->prev_hash, s->entries[s->n_entries - 1].self_hash, 32);

    compute_self_hash(e, e->prev_hash, e->self_hash);
    compute_sig(s->key, e->self_hash, e->sig);

    s->n_entries++;
    return 0;
}

/* ------------------------------------------------------------- */
/* Verify                                                        */
/* ------------------------------------------------------------- */

int cos_v181_verify_chain(const cos_v181_state_t *s) {
    if (!s) return -1;
    uint8_t prev[32]; memset(prev, 0, 32);
    for (int i = 0; i < s->n_entries; ++i) {
        const cos_v181_entry_t *e = &s->entries[i];
        if (memcmp(e->prev_hash, prev, 32) != 0) return i + 1;
        uint8_t hs[32];
        compute_self_hash(e, prev, hs);
        if (memcmp(hs, e->self_hash, 32) != 0) return i + 1;
        uint8_t sg[32];
        compute_sig(s->key, hs, sg);
        if (memcmp(sg, e->sig, 32) != 0) return i + 1;
        memcpy(prev, e->self_hash, 32);
    }
    return 0;
}

/* ------------------------------------------------------------- */
/* Anomaly detection                                             */
/* ------------------------------------------------------------- */

int cos_v181_detect_anomaly(cos_v181_state_t *s) {
    s->anomaly_detected = false;
    s->anomaly_at       = -1;
    if (!s || s->n_entries < 2 * s->window) return 0;

    /* Sliding scan: find first i ≥ window s.t. mean σ in
     * [i-window, i) vs [max(0, i-2*window), i-window) differs
     * by ≥ anomaly_rel_rise_pct. */
    for (int i = s->window; i + s->window <= s->n_entries; ++i) {
        double sum_recent = 0.0, sum_prev = 0.0;
        for (int k = 0; k < s->window; ++k) {
            sum_recent += s->entries[i + k].sigma_product;
            sum_prev   += s->entries[i - s->window + k].sigma_product;
        }
        double m_recent = sum_recent / s->window;
        double m_prev   = sum_prev   / s->window;
        if (m_prev <= 1e-6) continue;
        double rise = 100.0 * (m_recent - m_prev) / m_prev;
        if (rise >= s->anomaly_rel_rise_pct) {
            s->anomaly_detected           = true;
            s->anomaly_at                 = i;
            s->anomaly_sigma_baseline     = (float)m_prev;
            s->anomaly_sigma_recent       = (float)m_recent;
            s->anomaly_rel_rise           = (float)rise;
            return 1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------- */
/* Fixture                                                       */
/* ------------------------------------------------------------- */

static uint64_t splitmix64_s(uint64_t *st) {
    uint64_t z = (*st += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float frand_s(uint64_t *st) {
    return (splitmix64_s(st) >> 40) / 16777216.0f;
}

void cos_v181_populate_fixture(cos_v181_state_t *s, int n) {
    if (n > COS_V181_MAX_ENTRIES) n = COS_V181_MAX_ENTRIES;
    uint64_t rs = s->seed ^ 0xFA1E1ULL;
    for (int i = 0; i < n; ++i) {
        /* Baseline σ ~ 0.10..0.25; last ~15% of entries spike
         * to ~0.55..0.70 to trigger the anomaly detector. */
        float sigma;
        bool spike = (i >= (int)(n * 0.85));
        if (spike) sigma = 0.55f + 0.15f * frand_s(&rs);
        else       sigma = 0.10f + 0.15f * frand_s(&rs);

        float ch[COS_V181_N_CHANNELS];
        for (int k = 0; k < COS_V181_N_CHANNELS; ++k)
            ch[k] = sigma * (0.8f + 0.4f * frand_s(&rs));

        cos_v181_decision_t dec;
        if      (sigma < 0.30f) dec = COS_V181_DECISION_EMIT;
        else if (sigma < 0.55f) dec = COS_V181_DECISION_REVISE;
        else                    dec = COS_V181_DECISION_ABSTAIN;

        char input[32], output[32], ts[24];
        snprintf(input,  sizeof(input),  "prompt-%06d", i);
        snprintf(output, sizeof(output), "response-%06d", i);
        snprintf(ts,     sizeof(ts),     "2026-04-17T%02d:%02d:%02dZ",
                 (i / 3600) % 24, (i / 60) % 60, i % 60);

        const char *steer = (sigma > 0.40f ? "truthful" : "");
        cos_v181_append(s, input, output, sigma, ch, dec,
                        "v179: low-coverage-feature",
                        steer, "bitnet-2b",
                        "continual_epoch_47", ts);
    }
}

/* ------------------------------------------------------------- */
/* JSON / JSONL                                                  */
/* ------------------------------------------------------------- */

size_t cos_v181_to_json(const cos_v181_state_t *s,
                         char *buf, size_t cap) {
    if (!s || !buf) return 0;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v181\",\"n_entries\":%d,\"window\":%d,"
        "\"anomaly_detected\":%s,\"anomaly_at\":%d,"
        "\"anomaly_baseline\":%.4f,\"anomaly_recent\":%.4f,"
        "\"anomaly_rel_rise_pct\":%.2f}",
        s->n_entries, s->window,
        s->anomaly_detected ? "true" : "false",
        s->anomaly_at,
        (double)s->anomaly_sigma_baseline,
        (double)s->anomaly_sigma_recent,
        (double)s->anomaly_rel_rise);
    if (n < 0 || (size_t)n >= cap) return 0;
    return (size_t)n;
}

size_t cos_v181_export_jsonl(const cos_v181_state_t *s,
                              char *buf, size_t cap) {
    if (!s || !buf) return 0;
    size_t used = 0;
    for (int i = 0; i < s->n_entries; ++i) {
        const cos_v181_entry_t *e = &s->entries[i];
        char ih[65], oh[65], ph[65], sh[65], sg[65];
        hex_of(e->input_hash,  32, ih);
        hex_of(e->output_hash, 32, oh);
        hex_of(e->prev_hash,   32, ph);
        hex_of(e->self_hash,   32, sh);
        hex_of(e->sig,         32, sg);
        int n = snprintf(buf + used, cap - used,
            "{\"id\":%llu,\"ts\":\"%s\",\"sigma\":%.4f,"
            "\"decision\":%d,\"input_hash\":\"%s\","
            "\"output_hash\":\"%s\",\"prev_hash\":\"%s\","
            "\"self_hash\":\"%s\",\"sig\":\"%s\"}\n",
            (unsigned long long)e->id, e->timestamp,
            (double)e->sigma_product, (int)e->decision,
            ih, oh, ph, sh, sg);
        if (n < 0 || (size_t)n >= cap - used) return 0;
        used += (size_t)n;
    }
    return used;
}

/* ------------------------------------------------------------- */
/* Self-test                                                     */
/* ------------------------------------------------------------- */

int cos_v181_self_test(void) {
    cos_v181_state_t s;
    if (cos_v181_init(&s, COS_V181_MAX_ENTRIES, 0x181AFEDCULL) != 0)
        return 1;
    cos_v181_populate_fixture(&s, 1000);

    /* 1. Full chain verifies. */
    if (cos_v181_verify_chain(&s) != 0) { cos_v181_free(&s); return 2; }

    /* 2. Tamper a middle entry (flip one byte of output_hash). */
    {
        cos_v181_state_t t;
        cos_v181_init(&t, COS_V181_MAX_ENTRIES, 0x181AFEDCULL);
        cos_v181_populate_fixture(&t, 1000);
        t.entries[500].output_hash[0] ^= 0x01;
        /* Re-derive self_hash so only the *prev-link* check would
         * catch it downstream.  Verify then must fail at 501 or
         * 500 (depending on which check fires first). */
        if (cos_v181_verify_chain(&t) == 0) {
            cos_v181_free(&t); cos_v181_free(&s); return 3;
        }
        cos_v181_free(&t);
    }

    /* 3. Tamper only the signature of a middle entry. */
    {
        cos_v181_state_t t;
        cos_v181_init(&t, COS_V181_MAX_ENTRIES, 0x181AFEDCULL);
        cos_v181_populate_fixture(&t, 1000);
        t.entries[250].sig[7] ^= 0x80;
        if (cos_v181_verify_chain(&t) != 251) {
            cos_v181_free(&t); cos_v181_free(&s); return 4;
        }
        cos_v181_free(&t);
    }

    /* 4. Anomaly detection fires for the spike near the tail. */
    if (!cos_v181_detect_anomaly(&s)) { cos_v181_free(&s); return 5; }
    if (s.anomaly_rel_rise < s.anomaly_rel_rise_pct) {
        cos_v181_free(&s); return 6;
    }

    /* 5. JSONL export count matches. */
    size_t cap = 1 * 1024 * 1024;
    char  *jb  = (char *)malloc(cap);
    if (!jb) { cos_v181_free(&s); return 7; }
    size_t w = cos_v181_export_jsonl(&s, jb, cap);
    if (w == 0) { free(jb); cos_v181_free(&s); return 8; }
    int lines = 0;
    for (size_t i = 0; i < w; ++i) if (jb[i] == '\n') lines++;
    if (lines != s.n_entries) { free(jb); cos_v181_free(&s); return 9; }
    free(jb);

    /* 6. Determinism of top-level JSON summary. */
    char A[512], B[512];
    size_t na = cos_v181_to_json(&s, A, sizeof(A));
    size_t nb = cos_v181_to_json(&s, B, sizeof(B));
    if (na == 0 || na != nb || memcmp(A, B, na) != 0) {
        cos_v181_free(&s); return 10;
    }

    cos_v181_free(&s);
    return 0;
}
