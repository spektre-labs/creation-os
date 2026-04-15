/* sk9_fast_path — aarch64 Darwin (Apple Silicon). SNL-shaped micro-ops: σ popcount, 64-bit XOR mix.
 * Full Apple AMX is not emitted here; this is minimal register fast-path glue for SNL v3.0 bridge.
 * Build: ./build_sk9_fast_path.sh
 */
.text
.align 2
.globl _sk9_fast_popcnt32
_sk9_fast_popcnt32:
    mov     w2, wzr
.Lpc_loop:
    cbz     w0, .Lpc_end
    and     w3, w0, #1
    add     w2, w2, w3
    lsr     w0, w0, #1
    b       .Lpc_loop
.Lpc_end:
    mov     w0, w2
    ret

.globl _sk9_fast_xor_mix64
_sk9_fast_xor_mix64:
    eor     x0, x0, x1
    ret

/* Hardware 1=1 gate: (flags & required_mask) == required_mask → return 1 else 0 */
.globl _sk9_audit_lock_pass
_sk9_audit_lock_pass:
    and     w2, w0, w1
    cmp     w2, w1
    cset    w0, eq
    ret
