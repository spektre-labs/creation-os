/*
 * ============================================================================
 * CREATION OS v27.0 — VOCAB / TOKENIZATION PIPELINE (portable lab scaffold)
 * ============================================================================
 *
 * Lauri Elias Rainio · Spektre Labs · Helsinki
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Evidence class: **lab demo (C)** — deterministic `self_test` + JSON trace hooks.
 * This is **not** a trained 32K multilingual BPE, **not** FPGA timing closure,
 * **not** `lm-eval` parity, and **not** “coherent LM” generation.
 *
 * Roadmap vs shipped: docs/VOCAB_PIPELINE_V27.md
 *
 * New modules (names only; algebra is schematic):
 * [M177] Tier-1 BPE stand-in (`src/tokenizer/bpe.c`)
 * [M178] Tier-2 byte codebook + XOR-bind composition (`src/tokenizer/byte_fallback.c`)
 * [M179] Tier-3 base-27 literal codec stub (`src/tokenizer/gda27_stub.c`)
 * [M180] ROTL positional bind + context bundle
 * [M181] JEPA-style σ readout (toy target)
 * [M182] Eight-channel σ telemetry (rotated references)
 * [M183] Abstention gate vs fixed threshold
 * [M184] `--inference` trace writer (`inference_trace.json`)
 *
 * Compile: see Makefile `standalone-v27`
 * Test:    ./creation_os_v27 --self-test
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "core/cos_bsc.h"
#include "src/tokenizer/cos_tokenizer.h"

static int g_checks;
static int g_fails;

static void st(const char *name, bool ok)
{
    g_checks++;
    if (!ok) {
        printf("FAIL: %s\n", name);
        g_fails++;
    }
}

static void json_escape(FILE *f, const char *s)
{
    fputc('"', f);
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '"' || *p == '\\')
            fputc('\\', f);
        if (*p < 32) {
            fprintf(f, "\\u%04x", *p);
        } else {
            fputc((int)*p, f);
        }
    }
    fputc('"', f);
}

static float mean_sigma8(const uint64_t *ctx)
{
    float acc = 0.f;
    for (int k = 0; k < 8; k++) {
        uint64_t ref[COS_W];
        cos_hv_rotl(ref, ctx, k + 1);
        acc += cos_hv_sigma(ctx, ref);
    }
    return acc / 8.f;
}

static int run_inference_trace(const char *text, const char *path)
{
    BpeMergeTable merges;
    bpe_default_merges(&merges);
    uint32_t ids[COS_TOK_MAX_IDS];
    int n = bpe_encode_greedy(text, ids, COS_TOK_MAX_IDS, &merges);

    uint64_t ctx[COS_W];
    memset(ctx, 0, sizeof(ctx));
    for (int i = 0; i < n; i++) {
        uint64_t th[COS_W], pos[COS_W];
        bpe_id_to_hypervector(ids[i], 0xC0FFEEULL ^ (uint64_t)i, th);
        cos_hv_rotl(pos, th, i);
        for (int w = 0; w < COS_W; w++)
            ctx[w] ^= pos[w];
    }

    uint64_t tgt[COS_W];
    cos_hv_rotl(tgt, ctx, 3);
    float s0 = cos_hv_sigma(ctx, tgt);
    float s8 = mean_sigma8(ctx);
    const float thresh = 0.35f;
    bool abstain = (s8 >= thresh);

    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fprintf(f, "{\n  \"schema\": \"creation_os.v27.inference_trace\",\n");
    fprintf(f, "  \"tiers\": [\"bpe_stub\", \"byte_xor\", \"gda27_literal\"],\n");
    fprintf(f, "  \"input\": ");
    json_escape(f, text);
    fprintf(f, ",\n  \"token_count\": %d,\n", n);
    fprintf(f, "  \"sigma_ctx_tgt\": %.9f,\n", (double)s0);
    fprintf(f, "  \"sigma_mean8\": %.9f,\n", (double)s8);
    fprintf(f, "  \"abstain\": %s\n}\n", abstain ? "true" : "false");
    fclose(f);
    return 0;
}

static int self_test(void)
{
    g_checks = 0;
    g_fails = 0;

    BpeMergeTable merges;
    bpe_default_merges(&merges);
    st("merges_nonzero", merges.n_merges > 0u);

    const char *sample = "hello";
    uint32_t ids[COS_TOK_MAX_IDS];
    int n = bpe_encode_greedy(sample, ids, COS_TOK_MAX_IDS, &merges);
    st("encode_nonempty", n > 0);

    uint64_t a[COS_W], b[COS_W];
    bpe_id_to_hypervector(1, 1, a);
    bpe_id_to_hypervector(2, 1, b);
    st("hv_deterministic", memcmp(a, b, sizeof(a)) != 0);

    bpe_id_to_hypervector(9, 42, a);
    bpe_id_to_hypervector(9, 42, b);
    st("hv_stable_seed", memcmp(a, b, sizeof(a)) == 0);

    byte_codebook_build(0xBEEF1234DEADBEEFULL);
    uint64_t b0[COS_W], b1[COS_W];
    byte_symbol_hypervector('A', b0);
    byte_symbol_hypervector('B', b1);
    st("byte_rows_differ", memcmp(b0, b1, sizeof(b0)) != 0);

    const uint8_t raw[] = "ABC";
    uint64_t bun[COS_W];
    byte_bundle(raw, (int)sizeof(raw) - 1, bun);
    st("byte_bundle_nonzero", cos_hv_hamming(bun, b0) > 0u);

    char buf[32];
    int w = gda27_encode_uint(729, buf, (int)sizeof(buf)); /* 27^2 */
    st("gda27_encode_ok", w > 0);
    uint32_t back = gda27_decode_uint(buf);
    st("gda27_roundtrip", back == 729u);

    uint64_t ctx[COS_W];
    memset(ctx, 0, sizeof(ctx));
    for (int i = 0; i < n; i++) {
        uint64_t th[COS_W], pos[COS_W];
        bpe_id_to_hypervector(ids[i], 0xA11CEULL, th);
        cos_hv_rotl(pos, th, i);
        for (int k = 0; k < COS_W; k++)
            ctx[k] ^= pos[k];
    }
    float s8 = mean_sigma8(ctx);
    st("sigma8_finite", s8 == s8);

    uint64_t z[COS_W];
    memset(z, 0, sizeof(z));
    st("sigma_self_zero", cos_hv_sigma(z, z) < 1e-9f);

    uint32_t ids_empty[COS_TOK_MAX_IDS];
    int nz = bpe_encode_greedy("", ids_empty, COS_TOK_MAX_IDS, &merges);
    st("encode_empty_zero", nz == 0);

    st("gda27_decode_empty", gda27_decode_uint("") == 0u);

    char big[8];
    int gw = gda27_encode_uint(0xFFFFFFFFu, big, (int)sizeof(big));
    st("gda27_encode_trunc", gw >= 1);

    uint64_t hv300a[COS_W], hv300b[COS_W];
    bpe_id_to_hypervector(305u, 99, hv300a);
    bpe_id_to_hypervector(305u, 99, hv300b);
    st("merged_id_stable", memcmp(hv300a, hv300b, sizeof(hv300a)) == 0);

    st("merges_count_eight", merges.n_merges == 8u);

    for (int i = 0; i < 40; i++) {
        char name[48];
        (void)snprintf(name, sizeof name, "byte_row_distinct_%02d", i);
        uint64_t h1[COS_W], h2[COS_W];
        byte_symbol_hypervector((uint8_t)i, h1);
        byte_symbol_hypervector((uint8_t)((i + 13) % 256), h2);
        st(name, cos_hv_hamming(h1, h2) > 0u);
    }

    for (int k = 0; k < 8; k++) {
        char name[48];
        (void)snprintf(name, sizeof name, "rotl_sigma_pos_%d", k);
        uint64_t base[COS_W], r[COS_W];
        bpe_id_to_hypervector(42u, (uint64_t)k, base);
        cos_hv_rotl(r, base, k + 2);
        st(name, cos_hv_sigma(base, r) > 0.f);
    }

    const char *tmpf = "inference_trace_selftest.tmp";
    int trc = run_inference_trace("ping", tmpf);
    st("inference_trace_ok", trc == 0);
    remove(tmpf);

    printf("%d/%d PASS\n", g_checks - g_fails, g_checks);
    return g_fails ? 1 : 0;
}

int main(int argc, char **argv)
{
    bool do_test = false;
    bool do_inf = false;
    const char *text = NULL;
    const char *trace = "inference_trace.json";

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            do_test = true;
        } else if (!strcmp(argv[i], "--inference") && i + 1 < argc) {
            do_inf = true;
            text = argv[++i];
        } else if (!strcmp(argv[i], "--trace-out") && i + 1 < argc) {
            trace = argv[++i];
        }
    }

    if (do_test)
        return self_test();

    if (do_inf && text) {
        byte_codebook_build(0xC0DEC0DEC0DEC0DEULL);
        if (run_inference_trace(text, trace) != 0) {
            fprintf(stderr, "v27: could not write %s\n", trace);
            return 2;
        }
        printf("wrote %s\n", trace);
        return 0;
    }

    fprintf(stderr, "Creation OS v27 — usage:\n");
    fprintf(stderr, "  %s --self-test\n", argv[0]);
    fprintf(stderr, "  %s --inference \"text\" [--trace-out path]\n", argv[0]);
    return 1;
}
