#include "spektre_sk8_shadow_sim.h"

#include <stdlib.h>
#include <string.h>

/*
 * Shadow kernel = Judge Decoding layer (same weights as target path; separate malloc shadow state).
 * Stricter acceptance bar is applied in spektre.cpp via SPEKTRE_SHADOW_JUDGE_SIGMA_GE or 0.8× main.
 */

int spektre_sk8_shadow_commit_sigma(const sk8_kernel_t *baseline, const char *piece_utf8, int *sigma_after) {
    if (!baseline || !sigma_after) {
        return -1;
    }
    sk8_kernel_t *tmp = (sk8_kernel_t *)malloc(sizeof(sk8_kernel_t));
    if (!tmp) {
        return -1;
    }
    memcpy(tmp, baseline, sizeof(sk8_kernel_t));
    sk8_commit_token(tmp, piece_utf8 && piece_utf8[0] ? piece_utf8 : " ");
    *sigma_after = tmp->sigma;
    free(tmp);
    return 0;
}
