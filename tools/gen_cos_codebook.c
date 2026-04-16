/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Deterministic COSB codebook generator (Tier-1 table artifact builder).
 *
 * Usage: gen_cos_codebook [n_vocab] [output.bin]
 * Defaults: n_vocab=4096 output=.build/cos_codebook_4k.bin (fast dev artifact).
 * Example full Tier-1 table: `gen_cos_codebook 32768 .build/cos_codebook_32k.bin` (~16 MiB + header).
 *
 * Memory: n_vocab * 512 bytes (+16 byte header). Example: 32K -> ~16 MiB.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "src/tokenizer/cos_codebook_mmap.h"

int main(int argc, char **argv)
{
    uint32_t n = 4096u;
    if (argc >= 2) {
        unsigned long v = strtoul(argv[1], NULL, 10);
        if (v == 0ul || v > 1048576ul) {
            fprintf(stderr, "gen_cos_codebook: n_vocab out of range (1..1048576)\n");
            return 2;
        }
        n = (uint32_t)v;
    }
    const char *out = (argc >= 3) ? argv[2] : ".build/cos_codebook_4k.bin";
    if (cos_codebook_write_file(out, n) != 0) {
        perror("gen_cos_codebook");
        fprintf(stderr, "gen_cos_codebook: failed to write %s\n", out);
        return 1;
    }
    printf("wrote %s (n_vocab=%u, bytes≈%llu)\n", out, n,
           (unsigned long long)(sizeof(CosCodebookHeader) + (uint64_t)n * (uint64_t)(COS_W * sizeof(uint64_t))));
    return 0;
}
