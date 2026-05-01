/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Protocol — Ed25519 signer/verifier shim over vendored orlp/ed25519.
 *
 * This module slots Ed25519 into the cos_sigma_proto_sign_fn /
 * cos_sigma_proto_verify_fn contract.  The signature slot in the
 * σ-Protocol wire frame is already 64 bytes, so no framing change
 * is required to swap the FNV-1a stub for real asymmetric crypto.
 *
 * Key packing convention (see protocol.h):
 *
 *     sign():   key = private(64) || public(32)  → key_len = 96
 *     verify(): key = public(32)                 → key_len = 32
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Third-party notice: uses orlp/ed25519 (zlib license) vendored
 * under third_party/ed25519/.
 */

#include "protocol.h"

#include "../../../third_party/ed25519/src/ed25519.h"

#include <stdio.h>
#include <string.h>

void cos_sigma_proto_ed25519_sign(const uint8_t *data, size_t n,
                                  const uint8_t *key,  size_t key_len,
                                  uint8_t *sig)
{
    if (!sig) return;
    if (key_len != COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN || !key) {
        memset(sig, 0, COS_ED25519_SIG_LEN);
        return;
    }
    const unsigned char *priv = key;
    const unsigned char *pub  = key + COS_ED25519_PRIV_LEN;
    ed25519_sign(sig, data, n, pub, priv);
}

int cos_sigma_proto_ed25519_verify(const uint8_t *data, size_t n,
                                   const uint8_t *key,  size_t key_len,
                                   const uint8_t *sig)
{
    if (!sig) return 0;
    if (key_len != COS_ED25519_PUB_LEN || !key) return 0;
    return ed25519_verify(sig, data, n, key) == 1;
}

int cos_sigma_proto_ed25519_keypair_from_seed(const uint8_t *seed,
                                              uint8_t *out_pub,
                                              uint8_t *out_priv)
{
    if (!seed || !out_pub || !out_priv) return -1;
    ed25519_create_keypair(out_pub, out_priv, seed);
    return 0;
}

int cos_sigma_proto_ed25519_keypair(uint8_t *out_pub,
                                    uint8_t *out_priv)
{
    if (!out_pub || !out_priv) return -1;
    unsigned char seed[COS_ED25519_SEED_LEN];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) return -2;
    size_t got = fread(seed, 1, sizeof seed, f);
    fclose(f);
    if (got != sizeof seed) return -3;
    ed25519_create_keypair(out_pub, out_priv, seed);
    return 0;
}

/* ---------- self-test ---------- */

static int ed25519_roundtrip(void) {
    unsigned char seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) seed[i] = (unsigned char)i;

    uint8_t pub[COS_ED25519_PUB_LEN], priv[COS_ED25519_PRIV_LEN];
    if (cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv) != 0)
        return 100;

    /* Pack the sign-key = priv(64) || pub(32). */
    uint8_t sk[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sk, priv, COS_ED25519_PRIV_LEN);
    memcpy(sk + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    const uint8_t msg[] = "σ-gate: declared == realized";
    uint8_t sig[COS_ED25519_SIG_LEN];
    cos_sigma_proto_ed25519_sign(msg, sizeof msg - 1,
                                 sk, sizeof sk, sig);

    /* Good path: verifies with pub. */
    if (!cos_sigma_proto_ed25519_verify(msg, sizeof msg - 1,
                                        pub, COS_ED25519_PUB_LEN, sig))
        return 101;

    /* Tamper: flip one byte → must reject. */
    uint8_t tampered[sizeof msg - 1];
    memcpy(tampered, msg, sizeof tampered);
    tampered[0] ^= 0x01;
    if (cos_sigma_proto_ed25519_verify(tampered, sizeof tampered,
                                       pub, COS_ED25519_PUB_LEN, sig))
        return 102;

    /* Wrong key: second keypair must reject the first's signature. */
    unsigned char seed2[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) seed2[i] = (unsigned char)(0xA5 ^ i);
    uint8_t pub2[COS_ED25519_PUB_LEN], priv2[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed2, pub2, priv2);
    if (cos_sigma_proto_ed25519_verify(msg, sizeof msg - 1,
                                       pub2, COS_ED25519_PUB_LEN, sig))
        return 103;

    return 0;
}

static int ed25519_bad_args(void) {
    uint8_t sig[COS_ED25519_SIG_LEN];
    /* sign with wrong key_len → zeros signature (graceful). */
    memset(sig, 0xFF, sizeof sig);
    cos_sigma_proto_ed25519_sign((const uint8_t*)"x", 1,
                                 (const uint8_t*)"short", 5, sig);
    int all_zero = 1;
    for (int i = 0; i < COS_ED25519_SIG_LEN; ++i) if (sig[i]) { all_zero = 0; break; }
    if (!all_zero) return 200;

    /* verify with wrong key_len → 0 (reject). */
    if (cos_sigma_proto_ed25519_verify((const uint8_t*)"x", 1,
                                       (const uint8_t*)"short", 5, sig))
        return 201;
    return 0;
}

static int ed25519_proto_integration(void) {
    /* Exercise the full σ-protocol wire frame through an Ed25519
     * signer by temporarily masquerading as the default signer.
     * We do this by hand at the wire level since the codec is
     * keyed on the FNV default; the Ed25519 path is for callers
     * that sign outside the codec (signed envelopes, certificate
     * exchange, capability ads). */
    unsigned char seed[COS_ED25519_SEED_LEN];
    for (int i = 0; i < COS_ED25519_SEED_LEN; ++i) seed[i] = (unsigned char)(i + 0x10);
    uint8_t pub[COS_ED25519_PUB_LEN], priv[COS_ED25519_PRIV_LEN];
    cos_sigma_proto_ed25519_keypair_from_seed(seed, pub, priv);

    uint8_t sk[COS_ED25519_PRIV_LEN + COS_ED25519_PUB_LEN];
    memcpy(sk, priv, COS_ED25519_PRIV_LEN);
    memcpy(sk + COS_ED25519_PRIV_LEN, pub, COS_ED25519_PUB_LEN);

    /* Sign a σ-protocol QUERY body (without the sig slot). */
    const char *payload = "hello, mesh";
    uint8_t frame[COS_MSG_HEADER_LEN + 32];
    memset(frame, 0, sizeof frame);
    memcpy(frame + 8, "ed25519-node", 12);
    frame[4] = COS_MSG_QUERY;
    frame[5] = COS_MSG_VERSION;
    size_t body_len = COS_MSG_HEADER_LEN + (uint32_t)strlen(payload);
    memcpy(frame + COS_MSG_HEADER_LEN, payload, strlen(payload));

    uint8_t sig[COS_ED25519_SIG_LEN];
    cos_sigma_proto_ed25519_sign(frame, body_len, sk, sizeof sk, sig);

    if (!cos_sigma_proto_ed25519_verify(frame, body_len,
                                        pub, COS_ED25519_PUB_LEN, sig))
        return 300;
    frame[COS_MSG_HEADER_LEN] ^= 0x01;
    if (cos_sigma_proto_ed25519_verify(frame, body_len,
                                       pub, COS_ED25519_PUB_LEN, sig))
        return 301;
    return 0;
}

int cos_sigma_proto_ed25519_self_test(void) {
    int rc;
    if ((rc = ed25519_roundtrip()))         return rc;
    if ((rc = ed25519_bad_args()))          return rc;
    if ((rc = ed25519_proto_integration())) return rc;
    return 0;
}
