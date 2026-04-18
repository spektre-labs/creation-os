/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v130 σ-Codec — CLI wrapper.
 */
#include "codec.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v130_codec --self-test\n"
        "  creation_os_v130_codec --fp4-roundtrip N        # probe RMSE on N synthetic lora values\n"
        "  creation_os_v130_codec --sigma-pack \"σ0,σ1,...\" # probe uint8 round-trip\n"
        "  creation_os_v130_codec --context-compress budget \"σ0|txt0;σ1|txt1;...\"\n"
        "                                                 # σ-aware context compression\n");
    return 2;
}

static int cmd_fp4(int n) {
    if (n < 1 || n > 65536) { fprintf(stderr, "bad N\n"); return 2; }
    float *src = (float*)calloc(n, sizeof(float));
    float *dst = (float*)calloc(n, sizeof(float));
    uint8_t *pk = (uint8_t*)calloc((n + 1) / 2, 1);
    if (!src || !dst || !pk) { fprintf(stderr, "oom\n"); return 1; }
    for (int i = 0; i < n; ++i)
        src[i] = (float)(0.05 * (sin(0.31 * i) * 0.7 + cos(0.17 * i) * 0.3));
    float scale = cos_v130_quantize_fp4(src, n, pk);
    cos_v130_dequantize_fp4(pk, n, scale, dst);
    double se = 0.0;
    for (int i = 0; i < n; ++i) {
        double d = (double)src[i] - (double)dst[i];
        se += d * d;
    }
    float rmse = (float)sqrt(se / n);
    cos_v130_fp4_stats_t s = {
        .n        = n,
        .scale    = scale,
        .rmse     = rmse,
        .rel_rmse = scale > 0 ? rmse / scale : 0,
    };
    char js[256];
    cos_v130_fp4_stats_to_json(&s, js, sizeof js);
    printf("%s\n", js);
    printf("{\"bits_per_value\":4,\"compression_vs_fp32\":8.0}\n");
    free(src); free(dst); free(pk);
    return 0;
}

static int cmd_sigma_pack(const char *csv) {
    float in[COS_V130_SIGMA_DIM] = {0};
    int n = 0;
    const char *p = csv;
    while (*p && n < COS_V130_SIGMA_DIM) {
        char *end = NULL;
        in[n++] = strtof(p, &end);
        if (end == p) break;
        p = end;
        while (*p == ',' || *p == ' ') p++;
    }
    uint8_t codes[COS_V130_SIGMA_DIM];
    float out[COS_V130_SIGMA_DIM];
    cos_v130_pack_sigma(in, codes);
    cos_v130_unpack_sigma(codes, out);
    double me = 0.0;
    for (int i = 0; i < COS_V130_SIGMA_DIM; ++i) {
        double d = fabs((double)out[i] - (double)in[i]);
        if (d > me) me = d;
    }
    printf("{\"n\":%d,\"max_err\":%.8f,\"packed_bytes\":%d}\n",
           n, me, COS_V130_SIGMA_DIM);
    return 0;
}

/* Parse "σ|txt;σ|txt" into chunks. */
static int cmd_context(size_t budget, const char *spec) {
    cos_v130_chunk_t chunks[16]; int n = 0;
    const char *p = spec;
    while (*p && n < 16) {
        char *end = NULL;
        float sig = strtof(p, &end);
        if (end == p) break;
        if (*end != '|') break;
        p = end + 1;
        int written = 0;
        while (*p && *p != ';' && written < (int)sizeof chunks[n].text - 1) {
            chunks[n].text[written++] = *p++;
        }
        chunks[n].text[written] = '\0';
        chunks[n].sigma_product = sig;
        n++;
        if (*p == ';') p++;
    }
    if (n < 1) { fprintf(stderr, "no chunks parsed\n"); return 2; }
    char out[2048];
    int w = cos_v130_compress_context(chunks, n, budget, out, sizeof out);
    if (w < 0) { fprintf(stderr, "compress failed\n"); return 1; }
    size_t total_in = 0;
    for (int i = 0; i < n; ++i) total_in += strlen(chunks[i].text);
    printf("{\"chunks\":%d,\"input_bytes\":%zu,\"budget\":%zu,\"output_bytes\":%d}\n",
           n, total_in, budget, w);
    printf("%s\n", out);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test"))     return cos_v130_self_test();
    if (!strcmp(argv[1], "--fp4-roundtrip")) {
        if (argc < 3) return usage();
        return cmd_fp4(atoi(argv[2]));
    }
    if (!strcmp(argv[1], "--sigma-pack")) {
        if (argc < 3) return usage();
        return cmd_sigma_pack(argv[2]);
    }
    if (!strcmp(argv[1], "--context-compress")) {
        if (argc < 4) return usage();
        return cmd_context((size_t)strtoull(argv[2], NULL, 10), argv[3]);
    }
    return usage();
}
