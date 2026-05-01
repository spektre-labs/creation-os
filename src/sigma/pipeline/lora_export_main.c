/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-export demo: train a tiny adapter, serialise it to a `.cos`
 * file, peek + read it back, tamper with the payload, and report
 * whether the MAC check catches the edit — all through a
 * deterministic JSON envelope.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "lora.h"
#include "lora_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

static int weights_equal(const cos_lora_adapter_t *x,
                         const cos_lora_adapter_t *y) {
    size_t nA = (size_t)x->rank    * (size_t)x->in_dim;
    size_t nB = (size_t)x->out_dim * (size_t)x->rank;
    for (size_t i = 0; i < nA; i++) if (x->A[i] != y->A[i]) return 0;
    for (size_t i = 0; i < nB; i++) if (x->B[i] != y->B[i]) return 0;
    return 1;
}

int main(void) {
    int st = cos_lora_export_self_test();

    cos_lora_adapter_t src;
    cos_lora_init(&src, "factual_v1", 16, 8, 8, 16.0f,
                  0xFAC7FAC7FAC7FAC7ULL);
    float x[16], t[8], zero[8] = {0};
    for (int i = 0; i < 16; i++) x[i] = -0.5f + 0.07f * (float)i;
    for (int i = 0;  i < 8;  i++) t[i] =  0.2f  - 0.05f * (float)i;
    for (int s = 0; s < 300; s++) cos_lora_train_step(&src, x, t, zero, 0.05f, NULL);

    char path[] = "/tmp/cos-lora-export-demo-XXXXXX.cos";
    int fd = mkstemps(path, 4);
    if (fd < 0) { perror("mkstemps"); return 2; }
    close(fd);

    cos_lora_export_meta_t meta = {
        .description = "Curated factual adapter",
        .trained_by  = "spektre-labs",
        .benchmark_sigma = 0.14f,
        .created_unix    = 1700000000ULL,   /* fixed for determinism */
    };
    int rc_write = cos_lora_export_write(&src, &meta, path);

    cos_lora_export_info_t info;
    int rc_peek = cos_lora_export_peek(path, &info);

    cos_lora_adapter_t dst;
    int rc_read = cos_lora_export_read(path, &dst, &info);
    int equal   = (rc_read == COS_LORA_EXPORT_OK) && weights_equal(&src, &dst);

    /* Tamper: flip a weight byte and try to read again. */
    FILE *fp = fopen(path, "r+b");
    fseek(fp, 256 + 8, SEEK_SET);
    uint8_t b; fread(&b, 1, 1, fp); b ^= 0xFFu;
    fseek(fp, 256 + 8, SEEK_SET); fwrite(&b, 1, 1, fp);
    fclose(fp);

    cos_lora_adapter_t tamper;
    int rc_tampered = cos_lora_export_read(path, &tamper, NULL);

    /* Header MAC bytes (first 8 hex) — for the envelope so CI can
     * assert stability across runs. */
    char mac_hex[17] = {0};
    for (int i = 0; i < 8; i++) {
        snprintf(mac_hex + i*2, 3, "%02x", info.mac[i]);
    }

    printf("{\n");
    printf("  \"kernel\": \"sigma_lora_export\",\n");
    printf("  \"self_test\": %d,\n", st);
    printf("  \"write_rc\": %d,\n", rc_write);
    printf("  \"peek_rc\": %d,\n",  rc_peek);
    printf("  \"read_rc\": %d,\n",  rc_read);
    printf("  \"weights_equal\": %s,\n", equal ? "true" : "false");
    printf("  \"tamper_rc\": %d,\n", rc_tampered);
    printf("  \"info\": {\n");
    printf("    \"name\": \"%s\",\n", info.name);
    printf("    \"description\": \"%s\",\n", info.description);
    printf("    \"trained_by\": \"%s\",\n", info.trained_by);
    printf("    \"in_dim\": %d, \"out_dim\": %d, \"rank\": %d,\n",
           info.in_dim, info.out_dim, info.rank);
    printf("    \"alpha\": %.4f, \"benchmark_sigma\": %.4f,\n",
           (double)info.alpha, (double)info.benchmark_sigma);
    printf("    \"seed\": %llu, \"created_unix\": %llu,\n",
           (unsigned long long)info.seed,
           (unsigned long long)info.created_unix);
    printf("    \"bytes_weights\": %zu,\n", info.bytes_weights);
    printf("    \"mac_prefix_hex\": \"%s\"\n", mac_hex);
    printf("  }\n");
    printf("}\n");

    cos_lora_free(&src);
    if (rc_read == COS_LORA_EXPORT_OK) cos_lora_free(&dst);
    remove(path);
    return st == 0 ? 0 : 1;
}
