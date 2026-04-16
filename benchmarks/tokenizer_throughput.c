/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Microbench: Tier-1 encode + HV projection loop (host-dependent).
 * Build: see Makefile target `bench-tokenizer-v27`.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core/cos_bsc.h"
#include "src/tokenizer/cos_tokenizer.h"

int main(void)
{
    BpeMergeTable merges;
    bpe_default_merges(&merges);
    byte_codebook_build(0xACC1D5A5ULL);

    const char *corpus =
        "hello from creation os v27 tokenizer throughput microbench "
        "repeat repeat repeat for timing noise reduction";

    uint32_t ids[COS_TOK_MAX_IDS];
    uint64_t hv[COS_W];

    const int iters = 200000;
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < iters; i++) {
        int n = bpe_encode_greedy(corpus, ids, COS_TOK_MAX_IDS, &merges);
        for (int j = 0; j < n; j++)
            bpe_id_to_hypervector(ids[j], (uint64_t)(i ^ j), hv);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double sec = (double)(t1.tv_sec - t0.tv_sec) + 1e-9 * (double)(t1.tv_nsec - t0.tv_nsec);
    double enc_per_sec = (double)iters / sec;
    printf("tokenizer_throughput: %.1f encode+Hv loops/sec (iters=%d, corpus_len=%zu)\n", enc_per_sec, iters,
           strlen(corpus));
    return 0;
}
