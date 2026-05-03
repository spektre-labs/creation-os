/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
#include "../inference/sigma_quantize.h"
#include "cos_quantize_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int32_t parse_sigma01(const char *s)
{
    double x;
    if (!s || !s[0])
        return (int32_t)(0.15 * 65536.0);
    x = strtod(s, NULL);
    if (x < 0.0)
        x = 0.0;
    if (x > 1.0)
        x = 1.0;
    return (int32_t)(x * 65536.0 + 0.5);
}

static void demo_planes(const sigma_quant_config_t *cfg)
{
    int32_t i;
    printf("per-layer (synthetic plane distortion proxy, Q16)\n");
    for (i = 0; i < cfg->n_layers; i++)
        printf("  layer %2d  bits=%d  dist_q16=%d\n", (int)i,
               (int)cfg->layer_bits[i], (int)cfg->layer_distortion_q16[i]);
}

int cos_quantize_main(int argc, char **argv)
{
    int i;
    int do_auto = 0;
    int do_compare = 0;
    const char *model_path = NULL;
    int32_t target_q = (int32_t)(0.15 * 65536.0);
    int32_t w1[256];
    int32_t w2[256];
    int32_t w3[256];
    const int32_t *planes[8];
    int32_t sizes[8];
    sigma_quant_config_t cfg;
    int32_t p;

    for (i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--auto") == 0)
            do_auto = 1;
        else if (strcmp(argv[i], "--compare") == 0)
            do_compare = 1;
        else if (i + 1 < argc && strcmp(argv[i], "--model") == 0)
            model_path = argv[++i];
        else if (i + 1 < argc && strcmp(argv[i], "--target-sigma") == 0)
            target_q = parse_sigma01(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("usage: cos quantize [--auto] [--compare] "
                   "[--model PATH] [--target-sigma 0..1]\n"
                   "  Lab planner only — writes no GGUF yet; prints bit plan.\n");
            return 0;
        }
    }
    if (model_path)
        printf("quantize: model path '%s' (not loaded; synthetic planes)\n",
               model_path);

    for (p = 0; p < 256; p++) {
        w1[p] = (p * 17 - 2048) * 3;
        w2[p] = (p * 31 - 4096);
        w3[p] = (p - 128) * 100;
    }
    planes[0] = w1;
    planes[1] = w2;
    planes[2] = w3;
    planes[3] = w1;
    planes[4] = w2;
    planes[5] = w3;
    planes[6] = w1;
    planes[7] = w2;
    for (i = 0; i < 8; i++)
        sizes[i] = 256;

    if (do_compare) {
        int8_t q[256];
        int32_t d2;
        int32_t d4;
        int32_t d8;
        printf("uniform quant compare (synthetic w1[256])\n");
        sigma_quant_layer_symmetric(w1, q, 256, 2);
        d2 = sigma_quant_layer_distortion_q16(w1, q, 256, 2);
        sigma_quant_layer_symmetric(w1, q, 256, 4);
        d4 = sigma_quant_layer_distortion_q16(w1, q, 256, 4);
        sigma_quant_layer_symmetric(w1, q, 256, 8);
        d8 = sigma_quant_layer_distortion_q16(w1, q, 256, 8);
        printf("  Q2 dist_q16=%d  Q4 dist_q16=%d  Q8 dist_q16=%d\n", (int)d2,
               (int)d4, (int)d8);
    }

    if (do_auto || (!do_compare && argc <= 2)) {
        sigma_quant_auto_layers(&cfg, planes, sizes, 8, target_q);
        printf("sigma auto-quant: target_distortion_q16=%d\n", (int)target_q);
        demo_planes(&cfg);
        sigma_quant_print_summary(&cfg);
    }
    printf("quantize: no GGUF output yet — integrate with cos_inference_gguf load path.\n");
    return 0;
}
