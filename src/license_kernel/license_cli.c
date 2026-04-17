/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Commercial:    licensing@spektre-labs.com
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  src/license_kernel/license_cli.c
 *  --------------------------------------------------------------------
 *  Standalone driver for the License Attestation Kernel.
 *
 *    license_attest --self-test             # FIPS-180-4 + receipt KAT
 *    license_attest --hash <path>           # SHA-256 of any file
 *    license_attest --verify [<path>]       # verify LICENSE-SCSL-1.0.md
 *    license_attest --bundle [<repo_root>]  # verify full doc bundle
 *    license_attest --receipt <kid> <verdict> [<commit>] [<bits-hex>]
 *    license_attest --guard                 # exit 87 on breach
 *
 *  Used by:
 *    - `make license-attest`
 *    - cli/cos.c  (`cos license`)
 *    - scripts/v57/verify_agent.sh slot `license_attestation`
 */

#include "license_attest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_hex(const char *s, uint8_t *out, size_t cap) {
    size_t L = strlen(s);
    if (L % 2u || L / 2u > cap) return -1;
    for (size_t i = 0; i < L / 2u; ++i) {
        unsigned hi, lo;
        char a = s[i*2 + 0], b = s[i*2 + 1];
        if      (a >= '0' && a <= '9') hi = (unsigned)(a - '0');
        else if (a >= 'a' && a <= 'f') hi = (unsigned)(a - 'a' + 10);
        else if (a >= 'A' && a <= 'F') hi = (unsigned)(a - 'A' + 10);
        else return -1;
        if      (b >= '0' && b <= '9') lo = (unsigned)(b - '0');
        else if (b >= 'a' && b <= 'f') lo = (unsigned)(b - 'a' + 10);
        else if (b >= 'A' && b <= 'F') lo = (unsigned)(b - 'A' + 10);
        else return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return (int)(L / 2u);
}

static int self_test(void) {
    int pass = 0, fail = 0;
    char hex[65];
    uint8_t got[32];

    /* ── 1. SHA-256 KATs (FIPS-180-2 Appendix B + RFC 6234) ─────── */
    static const struct {
        const char *msg;
        const char *expect;
    } kats[] = {
        { "",                  "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc",               "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
                               "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    };
    for (size_t i = 0; i < sizeof kats / sizeof kats[0]; ++i) {
        spektre_sha256(kats[i].msg, strlen(kats[i].msg), got);
        spektre_hex_lower(got, hex);
        if (strcmp(hex, kats[i].expect) == 0) ++pass;
        else { fprintf(stderr, "fail: KAT[%zu] got=%s want=%s\n", i, hex, kats[i].expect); ++fail; }
    }

    /* Streaming update equivalence with one-shot. */
    {
        const char *m = "The quick brown fox jumps over the lazy dog";
        spektre_sha256(m, strlen(m), got);
        spektre_hex_lower(got, hex);
        if (strcmp(hex, "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592") == 0) ++pass;
        else { fprintf(stderr, "fail: brown-fox got=%s\n", hex); ++fail; }

        spektre_sha256_ctx_t ctx;
        spektre_sha256_init(&ctx);
        spektre_sha256_update(&ctx, "The quick brown ", 16);
        spektre_sha256_update(&ctx, "fox jumps over the lazy dog", 27);
        spektre_sha256_final(&ctx, got);
        spektre_hex_lower(got, hex);
        if (strcmp(hex, "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592") == 0) ++pass;
        else { fprintf(stderr, "fail: streaming got=%s\n", hex); ++fail; }
    }

    /* Million 'a' KAT (FIPS-180-2 §B.3) — exercises ≥ 15625 blocks. */
    {
        spektre_sha256_ctx_t ctx;
        spektre_sha256_init(&ctx);
        char a_buf[1024];
        memset(a_buf, 'a', sizeof a_buf);
        for (int i = 0; i < 1000; ++i) {
            spektre_sha256_update(&ctx, a_buf, sizeof a_buf);
        }
        /* 1024*1000 = 1 048 576, want one million bytes. Trim 48 576. */
        spektre_sha256_ctx_t ctx2;
        spektre_sha256_init(&ctx2);
        for (int i = 0; i < 976; ++i) {
            spektre_sha256_update(&ctx2, a_buf, sizeof a_buf);
        }
        spektre_sha256_update(&ctx2, a_buf, 1000000 - 976 * 1024);
        spektre_sha256_final(&ctx2, got);
        spektre_hex_lower(got, hex);
        if (strcmp(hex, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0) ++pass;
        else { fprintf(stderr, "fail: million-a got=%s\n", hex); ++fail; }
    }

    /* ── 2. constant-time memcmp ────────────────────────────────── */
    {
        uint8_t a[32] = {0}, b[32] = {0};
        if (spektre_ct_memcmp(a, b, 32) == 0) ++pass; else { fprintf(stderr, "fail: ct_memcmp eq\n"); ++fail; }
        b[17] = 1;
        if (spektre_ct_memcmp(a, b, 32) != 0) ++pass; else { fprintf(stderr, "fail: ct_memcmp ne\n"); ++fail; }
    }

    /* ── 3. License pin self-consistency ────────────────────────── */
    {
        char tmp[65];
        spektre_hex_lower(spektre_license_sha256_bin, tmp);
        if (strcmp(tmp, spektre_license_sha256_hex) == 0) ++pass;
        else { fprintf(stderr, "fail: pin hex/bin mismatch\n"); ++fail; }
    }

    /* ── 4. Receipt envelope renders and is itself hashable ─────── */
    {
        spektre_receipt_t r = {
            .kernel_id = "license-self-test",
            .kernel_verdict = "ALLOW",
            .commit_sha = "deadbeefcafebabefeedfacef00dbeef00112233",
            .emitted_at_ns = 1700000000000000000ull,
        };
        char buf[1536];
        int n = spektre_receipt_render(&r, buf, sizeof buf);
        if (n > 0 && strstr(buf, "license_sha256")
                  && strstr(buf, spektre_license_sha256_hex)
                  && strstr(buf, "\"verdict\":\"ALLOW\"")) {
            ++pass;
        } else {
            fprintf(stderr, "fail: receipt render n=%d\n", n);
            ++fail;
        }
        uint8_t rh[32];
        spektre_receipt_hash(buf, rh);
        char rhex[65];
        spektre_hex_lower(rh, rhex);
        if (strlen(rhex) == 64) ++pass; else { fprintf(stderr, "fail: receipt hash\n"); ++fail; }
    }

    fprintf(stderr, "license_attest self-test: %d pass, %d fail\n", pass, fail);
    return fail == 0 ? 0 : 1;
}

static int do_hash(const char *path) {
    spektre_sha256_ctx_t ctx;
    spektre_sha256_init(&ctx);
    FILE *f = path ? fopen(path, "rb") : stdin;
    if (!f) { perror(path); return 2; }
    uint8_t buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) {
        spektre_sha256_update(&ctx, buf, n);
    }
    if (path) fclose(f);
    uint8_t out[32]; char hex[65];
    spektre_sha256_final(&ctx, out);
    spektre_hex_lower(out, hex);
    printf("%s  %s\n", hex, path ? path : "-");
    return 0;
}

static int do_verify(const char *path) {
    int rc = spektre_license_verify(path);
    if (rc == 0) {
        printf("license_attest: OK (sha256=%s)\n", spektre_license_sha256_hex);
        return 0;
    }
    fprintf(stderr,
        "license_attest: FAIL rc=%d (%s)\n",
        rc,
        rc == -1 ? "license file missing"
      : rc == -2 ? "license file tampered (SHA-256 mismatch)"
      : rc == -3 ? "pinned-hash buffer is corrupt"
      :            "unknown");
    return 1;
}

static int do_bundle(const char *root) {
    int rc = spektre_license_bundle_verify(root);
    if (rc == 0) {
        printf("license_attest: bundle OK at %s\n", root ? root : ".");
        return 0;
    }
    fprintf(stderr, "license_attest: bundle FAIL rc=%d\n", rc);
    return 1;
}

static int do_receipt(int argc, char **argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: license_attest --receipt <kid> <verdict> [<commit>] [<bits-hex>]\n");
        return 2;
    }
    spektre_receipt_t r = {
        .kernel_id      = argv[2],
        .kernel_verdict = argv[3],
        .commit_sha     = argc > 4 ? argv[4] : NULL,
    };
    uint8_t bits_buf[64];
    int bits_n = 0;
    if (argc > 5) {
        bits_n = parse_hex(argv[5], bits_buf, sizeof bits_buf);
        if (bits_n < 0) { fprintf(stderr, "license_attest: bad hex bits\n"); return 2; }
        r.kernel_bits   = bits_buf;
        r.kernel_bits_n = (size_t)bits_n;
    }
    return spektre_receipt_print(&r) == 0 ? 0 : 1;
}

static int usage(const char *p) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --hash <file>\n"
        "       %s --verify [<license-path>]\n"
        "       %s --bundle [<repo-root>]\n"
        "       %s --receipt <kernel-id> <ALLOW|ABSTAIN|DENY> [<commit>] [<bits-hex>]\n"
        "       %s --guard\n",
        p, p, p, p, p, p);
    return 2;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    const char *cmd = argv[1];
    if (!strcmp(cmd, "--self-test"))  return self_test();
    if (!strcmp(cmd, "--hash")) {
        if (argc < 3) return usage(argv[0]);
        return do_hash(argv[2]);
    }
    if (!strcmp(cmd, "--verify"))     return do_verify(argc > 2 ? argv[2] : NULL);
    if (!strcmp(cmd, "--bundle"))     return do_bundle(argc > 2 ? argv[2] : NULL);
    if (!strcmp(cmd, "--receipt"))    return do_receipt(argc, argv);
    if (!strcmp(cmd, "--guard"))      return spektre_license_guard();
    if (!strcmp(cmd, "--help"))       return usage(argv[0]);
    return usage(argv[0]);
}
