/* ULTRA-3 — selective decoding on |Δσ|. */

#include "selective.h"

#include <math.h>

int cos_ultra_selective_should_decode(float sigma_t, float sigma_prev,
                                      float epsilon) {
    if (!(epsilon >= 0.0f)) epsilon = 0.0f;
    if (sigma_t != sigma_t || sigma_prev != sigma_prev) return 1;
    return fabsf(sigma_t - sigma_prev) >= epsilon ? 1 : 0;
}

int cos_ultra_selective_self_test(void) {
    if (cos_ultra_selective_should_decode(0.5f, 0.5f, 0.01f) != 0) return 1;
    if (cos_ultra_selective_should_decode(0.5f, 0.6f, 0.05f) != 1) return 2;
    if (cos_ultra_selective_should_decode(0.1f, 0.9f, 0.0f) != 1) return 3;
    return 0;
}
