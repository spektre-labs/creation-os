/* SPDX-License-Identifier: AGPL-3.0-or-later
 * XOR positional bind / unbind recovery (host-dependent).
 * Measures mean normalized Hamming distance after self-inverse XOR chain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/cos_bsc.h"
#include "src/tokenizer/cos_tokenizer.h"

static void xor_hv(uint64_t *a, const uint64_t *b)
{
    for (int i = 0; i < COS_W; i++)
        a[i] ^= b[i];
}

static float mean_norm_hamming(const uint64_t *a, const uint64_t *b)
{
    return (float)cos_hv_hamming(a, b) / (float)COS_D;
}

int main(void)
{
    BpeMergeTable merges;
    bpe_default_merges(&merges);
    byte_codebook_build(0xB16D1E5515ULL);

    const int trials = 5000;
    const int max_bind = 32;
    double acc_err[max_bind + 1];
    for (int k = 0; k <= max_bind; k++)
        acc_err[k] = 0.0;

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int t = 0; t < trials; t++) {
        uint64_t base[COS_W];
        bpe_id_to_hypervector(7u, (uint64_t)t * 0x100000001ULL, base);

        for (int load = 1; load <= max_bind; load++) {
            uint64_t ctx[COS_W];
            memcpy(ctx, base, sizeof(ctx));
            uint64_t slots[32][COS_W];
            for (int i = 0; i < load; i++) {
                uint64_t th[COS_W], pos[COS_W];
                bpe_id_to_hypervector((uint32_t)(17u + (i % 5)), (uint64_t)(t ^ i), th);
                cos_hv_rotl(pos, th, i + 1);
                memcpy(slots[i], pos, sizeof(pos));
                xor_hv(ctx, pos);
            }
            uint64_t rec[COS_W];
            memcpy(rec, ctx, sizeof(rec));
            for (int i = load - 1; i >= 0; i--)
                xor_hv(rec, slots[i]);
            acc_err[load] += (double)mean_norm_hamming(rec, base);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);

    printf("binding_fidelity: trials=%d bind_depth=1..%d wall_s=%.4f\n", trials, max_bind, sec);
    for (int load = 1; load <= max_bind; load++) {
        double mean = acc_err[load] / (double)trials;
        printf("  load=%02d mean_norm_hamming(recover,base)=%.6g (ideal 0)\n", load, mean);
    }
    return 0;
}
