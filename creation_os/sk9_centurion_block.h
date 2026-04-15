/* CREATION OS — Phase 100 Centurion (Genesis Block 1). 1 = 1 */
#ifndef SK9_CENTURION_BLOCK_H
#define SK9_CENTURION_BLOCK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t CENTURION_CORE[4];
void sk9_mine_centurion_block(float *vector_128);

#ifdef __cplusplus
}
#endif

#if defined(__cplusplus) && defined(__aarch64__)
#include <arm_neon.h>
float32x4_t sk9_mine_centurion_vec(float32x4_t latent);
#endif

#endif
