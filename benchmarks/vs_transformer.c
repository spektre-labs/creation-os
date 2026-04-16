/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Ops accounting at matched geometry (D=4096): dense GEMV vs packed HV similarity.
 * This is a **counting receipt**, not a quality claim — see docs/CLAIM_DISCIPLINE.md.
 */
#include <stdio.h>
#include <stdint.h>

#include "core/cos_bsc.h"

int main(void)
{
    const int D = COS_D;
    const int T = 128; /* sequence length toy */

    const uint64_t gemv_muls = (uint64_t)2u * (uint64_t)D * (uint64_t)D * (uint64_t)T;
    const uint64_t gemv_adds = (uint64_t)D * (uint64_t)T; /* bias + partial reduction floor */

    const uint64_t hv_xor_words = (uint64_t)COS_W * (uint64_t)T;
    const uint64_t hv_pop_words = (uint64_t)COS_W * (uint64_t)T; /* one POPCOUNT word lane per compare */

    printf("vs_transformer: D=%d T=%d (toy accounting)\n", D, T);
    printf("  dense_gemv_muls≈%llu muls, dense_gemv_adds≈%llu adds (O(D^2·T) story)\n", (unsigned long long)gemv_muls,
           (unsigned long long)gemv_adds);
    printf("  hv_bind_xor_words≈%llu 64b XORs, hv_sigma_pop_words≈%llu 64b POPCNT lanes (O(D·T) words)\n",
           (unsigned long long)hv_xor_words, (unsigned long long)hv_pop_words);
    double ratio = (double)gemv_muls / (double)(hv_xor_words + hv_pop_words);
    printf("  muls / (xor+pop)_words ≈ %.1f (not “× better LM”; ops class comparison only)\n", ratio);
    return 0;
}
