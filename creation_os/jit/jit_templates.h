/* CREATION OS — ARM64 JIT instruction templates (AArch64 encoding, little-endian). */
#ifndef CREATION_OS_JIT_TEMPLATES_H
#define CREATION_OS_JIT_TEMPLATES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** U32 little-endian write. */
static inline void jit_store_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

/**
 * MOVZ Xd, #imm16, LSL #shift  (shift 0,16,32,48)
 * Rd in bits [4:0], imm16 in [20:5], hw in [22:21].
 */
void jit_emit_movz_x(uint8_t **wp, unsigned rd, uint16_t imm16, unsigned shift_hw);

/** MOVK Xd, #imm16, LSL #shift — patch upper lanes of x register. */
void jit_emit_movk_x(uint8_t **wp, unsigned rd, uint16_t imm16, unsigned shift_hw);

/** RET (BR X30). */
void jit_emit_ret(uint8_t **wp);

#ifdef __cplusplus
}
#endif

#endif
