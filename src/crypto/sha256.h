/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * FIPS-180-4 SHA-256 — standalone C, no OpenSSL.
 * Algorithm reference: public-domain implementations (e.g. B-Con/crypto-algorithms
 * sha256.c) and FIPS-180-4.
 */
#ifndef COS_CRYPTO_SHA256_H
#define COS_CRYPTO_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t  h[8];
    uint64_t  total_bits;
    uint8_t   buf[64];
    size_t    buf_len;
} cos_sha256_ctx_t;

void cos_sha256_init(cos_sha256_ctx_t *ctx);
void cos_sha256_update(cos_sha256_ctx_t *ctx, const void *data, size_t len);
void cos_sha256_final(cos_sha256_ctx_t *ctx, uint8_t out[32]);

void cos_sha256(const uint8_t *data, size_t len, uint8_t out[32]);

/** Lowercase hex digest + NUL (out must be ≥ 65 bytes). */
void cos_sha256_hex(const uint8_t *data, size_t len, char out[65]);

#ifdef __cplusplus
}
#endif
#endif /* COS_CRYPTO_SHA256_H */
