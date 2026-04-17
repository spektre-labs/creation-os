/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Commercial:    licensing@spektre-labs.com
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  src/license_kernel/license_attest.h
 *  --------------------------------------------------------------------
 *  License Attestation Kernel — reference implementation for
 *  SCSL-1.0 §11 (Cryptographic License-Bound Receipt).
 *
 *  Every Receipt emitted by Creation OS MUST carry the SHA-256 of
 *  LICENSE-SCSL-1.0.md as the `license_sha256` field. This header
 *  declares the small, dependency-free, integer-only C API that
 *  any other kernel can call to:
 *
 *    (a) compute SHA-256 over arbitrary bytes (FIPS-180-4) without
 *        OpenSSL or any external runtime;
 *    (b) emit a canonical JSON License-Bound Receipt (the §11
 *        envelope) given a kernel verdict and a build commit;
 *    (c) self-check the in-tree LICENSE-SCSL-1.0.md against the
 *        pinned LICENSE.sha256 at build- or runtime; and
 *    (d) refuse to run if either file is missing or has been
 *        tampered with.
 *
 *  Hot path: libc-only (string.h, stdint.h, stddef.h). No malloc,
 *  no heap. Branchless on the SHA-256 round itself; the only
 *  branches are I/O.
 *
 *  This header is the v75 σ-License kernel of the Creation OS
 *  composed-decision stack.
 */

#ifndef SPEKTRE_LICENSE_ATTEST_H
#define SPEKTRE_LICENSE_ATTEST_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The pinned canonical SHA-256 of LICENSE-SCSL-1.0.md, as it shipped
 * with this build. Kept in sync with LICENSE.sha256 by the Makefile
 * target `license-pin`. 64 lowercase hex characters, no NUL, no
 * trailing newline.
 *
 * Defined in license_attest.c as a string literal and as a 32-byte
 * binary. Both forms are read by the assertion below. */
extern const char     spektre_license_sha256_hex[65];
extern const uint8_t  spektre_license_sha256_bin[32];

/* The canonical SCSL-1.0 / AGPL-3.0 dual license SPDX expression. */
#define SPEKTRE_LICENSE_SPDX     "LicenseRef-SCSL-1.0 OR AGPL-3.0-only"
#define SPEKTRE_COPYRIGHT_LINE   "2024-2026 Lauri Elias Rainio \xc2\xb7 Spektre Labs Oy"
#define SPEKTRE_SOURCE_URL       "https://github.com/spektre-labs/creation-os-kernel"
#define SPEKTRE_COMMERCIAL_EMAIL "licensing@spektre-labs.com"

/* ── 1. SHA-256 (FIPS-180-4), self-contained ───────────────────── */

typedef struct {
    uint32_t  h[8];
    uint64_t  total_bits;
    uint8_t   buf[64];
    size_t    buf_len;
} spektre_sha256_ctx_t;

void  spektre_sha256_init   (spektre_sha256_ctx_t *ctx);
void  spektre_sha256_update (spektre_sha256_ctx_t *ctx,
                             const void *data, size_t len);
void  spektre_sha256_final  (spektre_sha256_ctx_t *ctx,
                             uint8_t out[32]);
void  spektre_sha256        (const void *data, size_t len,
                             uint8_t out[32]);

/* Print 32 binary bytes as 64 lowercase hex chars + NUL.
 * `out` must point to ≥ 65 bytes. */
void  spektre_hex_lower     (const uint8_t in[32], char out[65]);

/* Constant-time (branchless) memory compare. Returns 0 iff equal. */
int   spektre_ct_memcmp     (const void *a, const void *b, size_t n);

/* ── 2. License attestation ────────────────────────────────────── */

/* Verify that `license_path` (default: "./LICENSE-SCSL-1.0.md") on
 * disk hashes to `spektre_license_sha256_bin`. Returns:
 *
 *    0  OK, license matches the pinned hash
 *   -1  file missing / unreadable                 (errno set)
 *   -2  hash mismatch                             (license tampered)
 *   -3  pinned hash buffer is itself corrupt
 *
 * If `license_path` is NULL the function tries, in order:
 *    "./LICENSE-SCSL-1.0.md"
 *    "../LICENSE-SCSL-1.0.md"
 *    getenv("SPEKTRE_LICENSE_PATH")
 */
int   spektre_license_verify (const char *license_path);

/* As above, but also verifies that the companion files referenced by
 * SCSL-1.0 §0 / §11 are present (LICENSE, LICENSE-AGPL-3.0.txt,
 * NOTICE, COMMERCIAL_LICENSE.md, CLA.md, LICENSE.sha256). Returns 0
 * iff the canonical bundle is intact. */
int   spektre_license_bundle_verify (const char *repo_root);

/* ── 3. License-Bound Receipt envelope (canonical JSON) ────────── */

typedef struct {
    const char *kernel_id;     /* e.g. "v74-experience"            */
    const char *kernel_verdict;/* "ALLOW" | "ABSTAIN" | "DENY"     */
    const char *commit_sha;    /* hex git SHA of the build         */
    uint64_t    emitted_at_ns; /* RFC-3339 source; pass 0 for now  */
    /* Optional: caller-side verdict bits (e.g. v60..v74 booleans). */
    const uint8_t *kernel_bits;
    size_t         kernel_bits_n;
} spektre_receipt_t;

/* Render a canonical JSON License-Bound Receipt into `out` (size
 * `cap`). Writes the SCSL-1.0 §11 envelope:
 *
 *   {
 *     "license":        "LicenseRef-SCSL-1.0 OR AGPL-3.0-only",
 *     "license_sha256": "<64 hex>",
 *     "copyright":      "2024-2026 Lauri Elias Rainio · Spektre Labs Oy",
 *     "source":         "https://github.com/spektre-labs/creation-os-kernel",
 *     "commit":         "<commit_sha>",
 *     "emitted_at":     "<RFC3339>",
 *     "kernel": {
 *        "id":      "<kernel_id>",
 *        "verdict": "<ALLOW|ABSTAIN|DENY>",
 *        "bits":    "<hex>"
 *     }
 *   }
 *
 * Returns the number of bytes written (excluding NUL), or -1 on
 * truncation. The output is canonical (sorted keys, no extra
 * whitespace, "\n"-terminated) so callers may hash it directly. */
int   spektre_receipt_render (const spektre_receipt_t *r,
                              char *out, size_t cap);

/* Convenience: emit a receipt to stdout, return 0/-1. */
int   spektre_receipt_print  (const spektre_receipt_t *r);

/* Hash a previously rendered receipt (so it can be chained). */
void  spektre_receipt_hash   (const char *receipt_json,
                              uint8_t out[32]);

/* ── 4. Anti-tamper guard ──────────────────────────────────────── */

/* Single call wrapped in __attribute__((constructor)) (or invoked
 * from main()) that:
 *
 *    1. verifies the in-tree LICENSE-SCSL-1.0.md hashes to the
 *       pinned value (spektre_license_verify(NULL));
 *    2. on mismatch, writes a one-line breach record to stderr
 *       and aborts the process with exit code 87;
 *    3. on missing file, falls back to printing a warning if
 *       SPEKTRE_LICENSE_SOFT=1 is set in the environment, otherwise
 *       aborts.
 *
 * Returns 0 on success; on hard failure the function does not
 * return.
 */
int   spektre_license_guard  (void);

#ifdef __cplusplus
}
#endif

#endif /* SPEKTRE_LICENSE_ATTEST_H */
