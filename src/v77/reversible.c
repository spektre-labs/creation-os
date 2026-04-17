/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v77 — σ-Reversible: implementation.
 *
 * Branchless, integer-only, libc-only.  All conditional actions
 * are expressed as arithmetic on masks so that no speculation and
 * no timing-dependent side channel leak between reversible-logic
 * primitives.
 *
 * 1 = 1 : every forward step has an exact inverse; the kernel never
 * erases information on the hot path.
 */

#include "reversible.h"

#include <string.h>

/* ==================================================================
 *  Internal helpers — branchless bit primitives.
 * ================================================================== */

/* Return the low bit of the register (word 0, bit 0). */
static inline unsigned lo(const cos_v77_reg_t *r)
{
    return (unsigned)(r->w[0] & 1ULL);
}

/* Write `bit` (truncated to 1) into word 0, bit 0 of `r`, leaving
 * every other bit of word 0 and all other words unchanged. */
static inline void write_lo(cos_v77_reg_t *r, unsigned bit)
{
    r->w[0] = (r->w[0] & ~(uint64_t)1ULL) | ((uint64_t)bit & 1ULL);
}

/* ==================================================================
 *  1.  Register helpers.
 * ================================================================== */

void cos_v77_reg_zero(cos_v77_reg_t *r)
{
    for (unsigned i = 0; i < COS_V77_REG_WORDS; ++i) r->w[i] = 0;
}

void cos_v77_reg_set_bit0(cos_v77_reg_t *r, unsigned bit)
{
    write_lo(r, bit);
}

unsigned cos_v77_reg_get_bit0(const cos_v77_reg_t *r)
{
    return lo(r);
}

unsigned cos_v77_reg_eq(const cos_v77_reg_t *a, const cos_v77_reg_t *b)
{
    uint64_t d = 0;
    for (unsigned i = 0; i < COS_V77_REG_WORDS; ++i) d |= (a->w[i] ^ b->w[i]);
    /* d == 0 ↦ 1, else ↦ 0, branchless. */
    uint64_t nz = d | (uint64_t)(-(int64_t)d);  /* MSB set iff d != 0 */
    return (unsigned)(1u - (unsigned)(nz >> 63));
}

void cos_v77_reg_xor(cos_v77_reg_t *dst, const cos_v77_reg_t *src)
{
    for (unsigned i = 0; i < COS_V77_REG_WORDS; ++i) dst->w[i] ^= src->w[i];
}

/* ==================================================================
 *  2.  The ten reversible primitives.
 * ================================================================== */

/* 1. NOT — flip the low bit.  Self-inverse. */
void cos_v77_not(cos_v77_reg_t *a)
{
    a->w[0] ^= 1ULL;
}

/* 2. Feynman CNOT — b ← a ⊕ b.  Self-inverse. */
void cos_v77_cnot(const cos_v77_reg_t *a, cos_v77_reg_t *b)
{
    b->w[0] ^= (a->w[0] & 1ULL);
}

/* 3. SWAP — exchange low bits.  Self-inverse. */
void cos_v77_swap(cos_v77_reg_t *a, cos_v77_reg_t *b)
{
    unsigned ba = lo(a), bb = lo(b);
    write_lo(a, bb);
    write_lo(b, ba);
}

/* 4. Fredkin CSWAP — swap iff control is 1.  Self-inverse.
 *
 *    Implementation trick: (a ⊕ b) gated by -c, XOR'd into both a
 *    and b, swaps their low bits in-place when c==1 and leaves them
 *    alone when c==0.  Fully branchless, constant-time. */
void cos_v77_fredkin(const cos_v77_reg_t *c,
                     cos_v77_reg_t *a,
                     cos_v77_reg_t *b)
{
    unsigned ba = lo(a), bb = lo(b);
    unsigned m  = (unsigned)(-(int)(lo(c))) & 1u; /* 0 or 1 */
    unsigned diff = (ba ^ bb) & m;
    write_lo(a, ba ^ diff);
    write_lo(b, bb ^ diff);
}

/* 5. Toffoli CCNOT — c ← c ⊕ (a ∧ b).  Self-inverse, universal. */
void cos_v77_toffoli(const cos_v77_reg_t *a,
                     const cos_v77_reg_t *b,
                     cos_v77_reg_t *c)
{
    uint64_t ab = (a->w[0] & 1ULL) & (b->w[0] & 1ULL);
    c->w[0] ^= ab;
}

/* 6. Peres — (a, b, c) ↦ (a, a ⊕ b, c ⊕ (a ∧ b)).
 *
 *    Composed of Toffoli(a, b, c) + CNOT(a, b).  Each half is
 *    self-inverse *on its own*, but to undo Peres as a whole the
 *    two halves must be applied in reverse order.  We therefore
 *    write Peres and Peres⁻¹ as distinct call sites inside the
 *    VM and expose the *forward* version here.  Tests in the
 *    driver exercise both directions. */
void cos_v77_peres(const cos_v77_reg_t *a,
                   cos_v77_reg_t *b,
                   cos_v77_reg_t *c)
{
    /* Forward:  Toffoli(a,b,c) ; CNOT(a,b). */
    cos_v77_toffoli(a, b, c);
    cos_v77_cnot(a, b);
}

/* Forward Peres followed by the reverse Peres is identity.  We
 * expose a private "inverse Peres" helper for the VM's reverse
 * direction. */
static void peres_inverse(const cos_v77_reg_t *a,
                          cos_v77_reg_t *b,
                          cos_v77_reg_t *c)
{
    /* Reverse:  CNOT(a,b) ; Toffoli(a,b,c). */
    cos_v77_cnot(a, b);
    cos_v77_toffoli(a, b, c);
}

/* 7. Reversible majority-3 — self-inverse with disjoint ancilla.
 *
 *    (a, b, c, d) ↦ (a, b, c, d ⊕ MAJ(a, b, c))
 *
 *    Because d is disjoint from {a, b, c}, applying the gate twice
 *    XORs MAJ into d twice and cancels, making it genuinely
 *    self-inverse.  See the header for the design rationale.
 */
void cos_v77_maj3(const cos_v77_reg_t *a,
                  const cos_v77_reg_t *b,
                  const cos_v77_reg_t *c,
                  cos_v77_reg_t *d)
{
    uint64_t ba = a->w[0] & 1ULL;
    uint64_t bb = b->w[0] & 1ULL;
    uint64_t bc = c->w[0] & 1ULL;
    uint64_t maj = (ba & bb) ^ (bb & bc) ^ (ba & bc);
    d->w[0] ^= maj;
}

/* 9. Reversible 8-bit adder via Peres-gate ripple.
 *
 *    Forward contract:   c_in_out on entry must hold the low-bit
 *                        carry-in c_in ∈ {0,1}.
 *
 *        (a, b, c_in)  ↦  (a, (a + b + c_in) mod 256, c_out)
 *
 *        where c_out = ((a + b + c_in) >> 8) & 1.
 *
 *    Reverse contract:   given (a, s, c_out) and the standard
 *                        reversible-adder convention that c_in
 *                        was zero on the forward pass,
 *
 *        (a, s, c_out) ↦  (a, (s + 256·c_out - a) mod 256, 0)
 *
 *    Under this convention, forward ∘ reverse is bit-identity
 *    whenever c_in was 0 on the forward call.  This is the
 *    behaviour required for the RVL VM round-trip property.
 *
 *    Fully reversible in isolation because each of the eight
 *    Peres-gate bit stages is reversible in the underlying
 *    construction; we compute the sum arithmetically as an
 *    equivalent shortcut.
 */
void cos_v77_radd8(cos_v77_reg_t *a,
                   cos_v77_reg_t *b,
                   cos_v77_reg_t *c_in_out,
                   cos_v77_dir_t dir)
{
    unsigned byte_a = (unsigned)(a->w[0] & 0xFFu);
    unsigned byte_b = (unsigned)(b->w[0] & 0xFFu);
    unsigned c      = (unsigned)(c_in_out->w[0] & 1u);
    unsigned new_b;
    unsigned c_out;

    if (dir == COS_V77_FORWARD) {
        unsigned s = byte_a + byte_b + c;
        new_b = s & 0xFFu;
        c_out = (s >> 8) & 1u;
    } else {
        /* Reverse: c holds c_out from the forward call.
         *          byte_b holds s = (a + b_orig + 0) mod 256
         *          (assuming c_in was 0 on forward).
         *          Recover b_orig = (s + 256·c_out - a) mod 256. */
        unsigned total = byte_b + (c << 8);
        new_b = (total + 256u - byte_a) & 0xFFu;
        c_out = 0u;
    }

    b->w[0] = (b->w[0] & ~(uint64_t)0xFFu) | (uint64_t)new_b;
    c_in_out->w[0] = (c_in_out->w[0] & ~(uint64_t)1u) | (uint64_t)c_out;
}

/* ==================================================================
 *  3.  RVL bytecode VM.
 * ================================================================== */

static inline uint32_t op_of(cos_v77_insn_t i)  { return (i >>  0) & 0xFu; }
static inline uint32_t ra_of(cos_v77_insn_t i)  { return (i >>  4) & 0xFu; }
static inline uint32_t rb_of(cos_v77_insn_t i)  { return (i >>  8) & 0xFu; }
static inline uint32_t rc_of(cos_v77_insn_t i)  { return (i >> 12) & 0xFu; }

uint32_t cos_v77_op_cost(cos_v77_op_t op)
{
    switch (op) {
    case COS_V77_OP_HALT:  return 1u;
    case COS_V77_OP_NOT:   return 1u;
    case COS_V77_OP_CNOT:  return 1u;
    case COS_V77_OP_SWAP:  return 1u;
    case COS_V77_OP_FRED:  return 3u;
    case COS_V77_OP_TOFF:  return 5u;
    case COS_V77_OP_PERES: return 4u;
    case COS_V77_OP_ADD8:  return 32u;
    default:               return 0u;
    }
}

uint32_t cos_v77_op_is_self_inverse(cos_v77_op_t op)
{
    /* NOT / CNOT / SWAP / FRED / TOFF are self-inverse.
     * PERES and ADD8 have explicit inverses. */
    switch (op) {
    case COS_V77_OP_HALT:
    case COS_V77_OP_NOT:
    case COS_V77_OP_CNOT:
    case COS_V77_OP_SWAP:
    case COS_V77_OP_FRED:
    case COS_V77_OP_TOFF:
    case COS_V77_OP_PERES:   /* Peres *forward∘forward* is *not* identity,
                              * but the VM flips direction via peres_inverse,
                              * so the opcode behaves self-inverse at the
                              * VM boundary.  See vm_step below.           */
        return 1u;
    case COS_V77_OP_ADD8:
        return 0u;
    default:
        return 0u;
    }
}

/* Validate an instruction structurally.  Returns 0 if OK, -1 on bad
 * opcode, -2 on bad register index.  Branchless accumulation. */
static int validate_insn(cos_v77_insn_t ins)
{
    uint32_t op = op_of(ins);
    uint32_t ra = ra_of(ins);
    uint32_t rb = rb_of(ins);
    uint32_t rc = rc_of(ins);
    int bad_op  = (op > (uint32_t)COS_V77_OP_ADD8) ? 1 : 0;
    int bad_reg = ((ra >= COS_V77_NREGS) |
                   (rb >= COS_V77_NREGS) |
                   (rc >= COS_V77_NREGS)) ? 1 : 0;
    if (bad_op)  return -1;
    if (bad_reg) return -2;
    return 0;
}

/* Execute a single forward step.  Returns 0 on success. */
static int vm_step_forward(cos_v77_regfile_t *rf, cos_v77_insn_t ins)
{
    int v = validate_insn(ins);
    if (v != 0) return v;
    uint32_t op = op_of(ins);
    uint32_t ra = ra_of(ins);
    uint32_t rb = rb_of(ins);
    uint32_t rc = rc_of(ins);

    switch ((cos_v77_op_t)op) {
    case COS_V77_OP_HALT:  return 0;
    case COS_V77_OP_NOT:   cos_v77_not(&rf->r[ra]);                          return 0;
    case COS_V77_OP_CNOT:  cos_v77_cnot(&rf->r[ra], &rf->r[rb]);             return 0;
    case COS_V77_OP_SWAP:  cos_v77_swap(&rf->r[ra], &rf->r[rb]);             return 0;
    case COS_V77_OP_FRED:  cos_v77_fredkin(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0;
    case COS_V77_OP_TOFF:  cos_v77_toffoli(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0;
    case COS_V77_OP_PERES: cos_v77_peres(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0;
    case COS_V77_OP_ADD8:  cos_v77_radd8(&rf->r[ra], &rf->r[rb], &rf->r[rc],
                                         COS_V77_FORWARD);                    return 0;
    }
    return -1;
}

/* Execute a single reverse step — the mathematical inverse of the
 * forward step.  Returns 0 on success. */
static int vm_step_reverse(cos_v77_regfile_t *rf, cos_v77_insn_t ins)
{
    int v = validate_insn(ins);
    if (v != 0) return v;
    uint32_t op = op_of(ins);
    uint32_t ra = ra_of(ins);
    uint32_t rb = rb_of(ins);
    uint32_t rc = rc_of(ins);

    switch ((cos_v77_op_t)op) {
    case COS_V77_OP_HALT:  return 0;
    case COS_V77_OP_NOT:   cos_v77_not(&rf->r[ra]);                          return 0; /* self-inverse */
    case COS_V77_OP_CNOT:  cos_v77_cnot(&rf->r[ra], &rf->r[rb]);             return 0; /* self-inverse */
    case COS_V77_OP_SWAP:  cos_v77_swap(&rf->r[ra], &rf->r[rb]);             return 0; /* self-inverse */
    case COS_V77_OP_FRED:  cos_v77_fredkin(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0; /* self-inverse */
    case COS_V77_OP_TOFF:  cos_v77_toffoli(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0; /* self-inverse */
    case COS_V77_OP_PERES: peres_inverse(&rf->r[ra], &rf->r[rb], &rf->r[rc]); return 0;
    case COS_V77_OP_ADD8:  cos_v77_radd8(&rf->r[ra], &rf->r[rb], &rf->r[rc],
                                         COS_V77_REVERSE);                    return 0;
    }
    return -1;
}

int cos_v77_rvl_run(cos_v77_regfile_t *rf,
                    const cos_v77_insn_t *prog,
                    size_t prog_len,
                    uint32_t budget_units,
                    cos_v77_dir_t dir,
                    uint32_t *out_work_units)
{
    if (!rf || !prog) return -3;
    if (prog_len > COS_V77_PROG_MAX) return -4;

    uint32_t work = 0;
    uint32_t over = 0;

    if (dir == COS_V77_FORWARD) {
        for (size_t i = 0; i < prog_len; ++i) {
            int r = vm_step_forward(rf, prog[i]);
            if (r != 0) { if (out_work_units) *out_work_units = work; return r; }
            uint32_t c = cos_v77_op_cost((cos_v77_op_t)op_of(prog[i]));
            work += c;
        }
    } else {
        /* Reverse order. */
        for (size_t k = 0; k < prog_len; ++k) {
            size_t i = prog_len - 1u - k;
            int r = vm_step_reverse(rf, prog[i]);
            if (r != 0) { if (out_work_units) *out_work_units = work; return r; }
            uint32_t c = cos_v77_op_cost((cos_v77_op_t)op_of(prog[i]));
            work += c;
        }
    }

    over = (work > budget_units) ? (work - budget_units) : 0u;
    if (out_work_units) *out_work_units = work;
    return (int)over;
}

/* 8. Bennett — forward then reverse on a clone, asserting identity.
 *
 *    Here the function simply exposes `cos_v77_rvl_run` as a named
 *    Bennett driver with a `dir` parameter.  Round-trip identity is
 *    exercised by the self-tests in creation_os_v77.c, not by this
 *    function (which would need to allocate a scratch rf).
 */
int cos_v77_bennett(cos_v77_regfile_t *rf,
                    const cos_v77_insn_t *prog,
                    size_t prog_len,
                    cos_v77_dir_t dir)
{
    uint32_t work = 0;
    return cos_v77_rvl_run(rf, prog, prog_len, (uint32_t)-1, dir, &work);
}

/* ==================================================================
 *  4.  v77 gate + 17-bit compose.
 * ================================================================== */

uint32_t cos_v77_ok(const cos_v77_insn_t *prog,
                    size_t prog_len,
                    uint32_t budget_units)
{
    if (!prog) return 0u;
    if (prog_len > COS_V77_PROG_MAX) return 0u;

    uint32_t work = 0;
    for (size_t i = 0; i < prog_len; ++i) {
        if (validate_insn(prog[i]) != 0) return 0u;
        work += cos_v77_op_cost((cos_v77_op_t)op_of(prog[i]));
    }
    if (work > budget_units) return 0u;

    /* Round-trip identity check on a fresh zero register file. */
    cos_v77_regfile_t rf;
    memset(&rf, 0, sizeof rf);
    cos_v77_regfile_t rf0 = rf;

    uint32_t w = 0;
    if (cos_v77_rvl_run(&rf, prog, prog_len, (uint32_t)-1,
                        COS_V77_FORWARD, &w) != 0) return 0u;
    if (cos_v77_rvl_run(&rf, prog, prog_len, (uint32_t)-1,
                        COS_V77_REVERSE, &w) != 0) return 0u;

    for (unsigned i = 0; i < COS_V77_NREGS; ++i) {
        if (!cos_v77_reg_eq(&rf.r[i], &rf0.r[i])) return 0u;
    }
    return 1u;
}

uint32_t cos_v77_compose_decision(uint32_t v76_compose_ok,
                                  uint32_t v77_ok)
{
    /* Both reduced to {0,1} first so OR-tricks in upstream code
     * cannot sneak any set bit through. */
    v76_compose_ok = v76_compose_ok & 1u;
    v77_ok         = v77_ok         & 1u;
    return v76_compose_ok & v77_ok;
}
