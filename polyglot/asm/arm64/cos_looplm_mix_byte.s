/* SPDX-License-Identifier: AGPL-3.0-or-later */
/* cos_looplm_mix_byte.s — AArch64: one LoopLM toy mix step on bytes (matches C drum idea) */
/*   out = (3*h + scale) >> 2   (unsigned bytes, same as rtl/cos_looplm_drum.sv mix_line) */
/* Build: clang -c -arch arm64 asm/arm64/cos_looplm_mix_byte.s -o cos_mix.o */
/* C ABI: uint8_t cos_looplm_mix_byte(uint8_t h, uint8_t scale); */

.text
.align 4
.globl _cos_looplm_mix_byte
_cos_looplm_mix_byte:
    /* w0 = h, w1 = scale */
    mov     w2, w0
    add     w2, w2, w0, lsl #1    /* w2 = 3*h */
    add     w0, w2, w1
    lsr     w0, w0, #2
    ret
