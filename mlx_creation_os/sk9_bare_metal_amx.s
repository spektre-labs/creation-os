/* Creation OS v8 — bare AMX instruction surface (Darwin aarch64 GAS).
 * AMX SET/CLR + one vector FMA32 (operand word in X0, 1<<63). No fake MSR.
 * Encoding: corsix/amx aarch64.md. M4/M-series only; illegal under Rosetta.
 */
.text
.align 2

.globl _sk9_bare_amx_enter
_sk9_bare_amx_enter:
    nop
    nop
    nop
    .word (0x201000 + (17 << 5) + 0)
    ret

.globl _sk9_bare_amx_exit
_sk9_bare_amx_exit:
    nop
    nop
    nop
    .word (0x201000 + (17 << 5) + 1)
    ret

/* FMA32 vector mode, Z row 0; operand word must sit in X0 (caller sets). */
.globl _sk9_bare_amx_fma32_run
_sk9_bare_amx_fma32_run:
    .word (0x201000 + (12 << 5) + 0)
    ret

/* Fixed pulse: x*y+z with vector bit, Z row 0 — operand = 1<<63 */
.globl _sk9_bare_amx_fma32_vec_z0_pulse
_sk9_bare_amx_fma32_vec_z0_pulse:
    movz    x0, #0x8000, lsl #48
    .word (0x201000 + (12 << 5) + 0)
    ret
