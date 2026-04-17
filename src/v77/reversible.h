/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v77 — σ-Reversible (the Landauer / Bennett plane).
 *
 * ------------------------------------------------------------------
 *  What v77 is
 * ------------------------------------------------------------------
 *
 * v60..v76 ship *forward-only* logic: every decision is a branchless
 * integer compose, but every compose *erases* information — taking N
 * input bits to fewer output bits pays the Landauer floor of
 * k_B · T · ln 2 per erased bit in principle (Landauer 1961).
 *
 * v77 σ-Reversible is the first Creation-OS kernel that enforces
 * **bit-reversibility on the hot path**.  Every primitive in this
 * plane is either self-inverse or has a declared explicit inverse
 * computed by the same C function run with `direction = -1` (Bennett
 * 1973 — "uncomputing", Toffoli 1980 — universal reversible gate,
 * Fredkin 1982 — conservative logic, Feynman 1985 — reversible
 * computation, Margolus 1990 — physics and computation, and the
 * 2024 arXiv:2402.02720 "Reversible Instruction Set Architectures"
 * + NeurIPS 2023-2025 reversible-transformers literature).
 *
 * Ten primitives:
 *
 *   1.  cos_v77_not        — single-bit NOT.           Self-inverse.
 *
 *   2.  cos_v77_cnot       — Feynman controlled-NOT.   Self-inverse.
 *                            (a, b) ↦ (a, a ⊕ b)
 *
 *   3.  cos_v77_swap       — in-place SWAP.            Self-inverse.
 *                            (a, b) ↦ (b, a)
 *
 *   4.  cos_v77_fredkin    — Fredkin CSWAP.            Self-inverse,
 *                            conservative (ones-count preserved).
 *                            (c, a, b) ↦ c ? (c, b, a) : (c, a, b)
 *
 *   5.  cos_v77_toffoli    — Toffoli CCNOT.            Self-inverse,
 *                            universal for reversible logic.
 *                            (a, b, c) ↦ (a, b, c ⊕ (a ∧ b))
 *
 *   6.  cos_v77_peres      — Peres gate.               Self-inverse.
 *                            (a, b, c) ↦ (a, a ⊕ b, c ⊕ (a ∧ b))
 *                            = Toffoli ∘ CNOT.  Quantum cost 4.
 *
 *   7.  cos_v77_maj3       — reversible majority-3.    Self-inverse.
 *                            (a, b, c) ↦ (a ⊕ b, b ⊕ c, MAJ(a,b,c))
 *
 *   8.  cos_v77_bennett    — Bennett-style forward/reverse driver.
 *                            Runs an RVL program forward, stamps a
 *                            garbage register chain, then runs it
 *                            backward to uncompute.  Self-test:
 *                            `bennett(bennett(x)) == x`.
 *
 *   9.  cos_v77_radd8      — reversible 8-bit adder built from a
 *                            chain of Peres gates.  Forward emits
 *                            `(a, a+b mod 256, carry)`; reverse
 *                            recovers `(a, b, 0)` deterministically.
 *
 *  10.  cos_v77_rvl        — RVL bytecode VM.  8 opcodes, every op
 *                            has an exact inverse, round-trip is
 *                            bit-identical across forward ∘ reverse.
 *                            Opcode table (integer-only):
 *                                HALT  = 0   (self-inverse)
 *                                NOT   = 1   (self-inverse)
 *                                CNOT  = 2   (self-inverse)
 *                                SWAP  = 3   (self-inverse)
 *                                FRED  = 4   (self-inverse)
 *                                TOFF  = 5   (self-inverse)
 *                                PERES = 6   (self-inverse)
 *                                ADD8  = 7   (inverse = SUB8 via
 *                                             same op in reverse
 *                                             direction with the
 *                                             carry register
 *                                             threaded back)
 *
 *      Cost model (reversible-work units, not joules):
 *          HALT   = 1
 *          NOT    = 1
 *          CNOT   = 1
 *          SWAP   = 1
 *          FRED   = 3   (3 × Peres decomposition)
 *          TOFF   = 5   (Peres + CNOT)
 *          PERES  = 4
 *          ADD8   = 4 × 8 = 32  (8-stage Peres chain)
 *
 *      Gate writes `v77_ok = 1` iff:
 *          - every opcode is ∈ {0..7}                (no bad opcode)
 *          - every register index is  < COS_V77_NREGS
 *          - total reversible-work    ≤ budget
 *          - round-trip:  forward(prog); reverse(prog) yields the
 *            exact initial register file (bit-equal across all
 *            4 × uint64 per register).
 *
 * ------------------------------------------------------------------
 *  Composed 17-bit branchless decision (extends v76)
 * ------------------------------------------------------------------
 *
 * v77 does *not* rewrite v60..v76.  It adds one lateral lane:
 *
 *     cos_v77_compose_decision(v76_compose_ok, v77_ok)
 *         = v76_compose_ok & v77_ok
 *
 * The verdict is still a single branchless AND.  Nothing — no tool
 * call, no sealed message, no generated artefact, no surface event,
 * no legacy-software hand-off — crosses to the human unless all
 * sixteen prior kernels ALLOW *and* the reversible plane certifies
 * the computation as bit-reversible under its declared budget.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline
 * ------------------------------------------------------------------
 *
 *   - Token:  a 256-bit register = 4 × uint64.  Matches v76.
 *   - File:   COS_V77_NREGS = 16 registers per VM state.
 *   - Arena:  every allocation via aligned_alloc(64, multiple-of-64).
 *   - No FP anywhere — not even in cost accounting.
 *   - Every primitive is branchless: conditional swaps / moves are
 *     done by mask arithmetic (mask = -(bit & 1)) so there is zero
 *     speculation, zero timing leak.
 *   - No malloc on the hot path; the VM state is stack-allocatable.
 *   - Every reverse step consumes *exactly* what its forward step
 *     produced; no hidden erasure.  1 = 1.
 */

#ifndef COS_V77_REVERSIBLE_H
#define COS_V77_REVERSIBLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 *  0.  Primitive types & constants.
 * ================================================================== */

#define COS_V77_REG_WORDS    4u     /* 256-bit registers, same as v76 */
#define COS_V77_REG_BYTES   32u
#define COS_V77_REG_BITS   256u
#define COS_V77_NREGS       16u
#define COS_V77_PROG_MAX  1024u

typedef struct {
    uint64_t w[COS_V77_REG_WORDS];
} cos_v77_reg_t;

typedef struct {
    cos_v77_reg_t r[COS_V77_NREGS];
} cos_v77_regfile_t;

/* Opcodes — 4-bit field, 8 currently used. */
typedef enum {
    COS_V77_OP_HALT  = 0,
    COS_V77_OP_NOT   = 1,
    COS_V77_OP_CNOT  = 2,
    COS_V77_OP_SWAP  = 3,
    COS_V77_OP_FRED  = 4,
    COS_V77_OP_TOFF  = 5,
    COS_V77_OP_PERES = 6,
    COS_V77_OP_ADD8  = 7,
} cos_v77_op_t;

/* 32-bit packed instruction:
 *
 *   bits  0..3    opcode
 *   bits  4..7    ra   (destination / first operand register idx)
 *   bits  8..11   rb   (second operand register idx)
 *   bits 12..15   rc   (third operand register idx — ignored on
 *                       1-/2-register ops but must be < NREGS)
 *   bits 16..31   imm  (byte-offset inside register, 0..31, for ADD8;
 *                       ignored elsewhere)
 *
 * Only the low bit of each register word is read by NOT/CNOT/SWAP/
 * FRED/TOFF/PERES (they are logical-bit gates).  ADD8 reads the byte
 * at  (reg.w[imm / 8]  >> ((imm % 8) * 8))  &  0xffU.
 */
typedef uint32_t cos_v77_insn_t;

/* Direction flag. */
typedef enum {
    COS_V77_FORWARD =  1,
    COS_V77_REVERSE = -1,
} cos_v77_dir_t;

/* ==================================================================
 *  1.  Register helpers.
 * ================================================================== */

/* Clear a register to all-zero. */
void cos_v77_reg_zero(cos_v77_reg_t *r);

/* Set the low bit of a register (used by the single-bit gate tests). */
void cos_v77_reg_set_bit0(cos_v77_reg_t *r, unsigned bit);

/* Read the low bit of a register (0 or 1, branchless). */
unsigned cos_v77_reg_get_bit0(const cos_v77_reg_t *r);

/* Bit-equal compare.  Returns 1 if equal, 0 otherwise. Branchless. */
unsigned cos_v77_reg_eq(const cos_v77_reg_t *a, const cos_v77_reg_t *b);

/* Full 256-bit XOR into dst (used inside some gates). */
void cos_v77_reg_xor(cos_v77_reg_t *dst, const cos_v77_reg_t *src);

/* ==================================================================
 *  2.  Ten reversible primitives.
 *
 *      Each primitive operates on the low bit of the referenced
 *      register(s).  They are the canonical reversible-logic gates
 *      and together form a universal reversible basis.
 *      All are either self-inverse or have a declared inverse.
 * ================================================================== */

/* 1. NOT — self-inverse. */
void cos_v77_not(cos_v77_reg_t *a);

/* 2. Feynman CNOT — self-inverse.
 *    (a, b) ↦ (a, a ⊕ b)
 */
void cos_v77_cnot(const cos_v77_reg_t *a, cos_v77_reg_t *b);

/* 3. SWAP — self-inverse. */
void cos_v77_swap(cos_v77_reg_t *a, cos_v77_reg_t *b);

/* 4. Fredkin CSWAP — self-inverse, conservative.
 *    (c, a, b) ↦ c ? (c, b, a) : (c, a, b)
 */
void cos_v77_fredkin(const cos_v77_reg_t *c,
                     cos_v77_reg_t *a,
                     cos_v77_reg_t *b);

/* 5. Toffoli CCNOT — self-inverse, universal for reversible logic.
 *    (a, b, c) ↦ (a, b, c ⊕ (a ∧ b))
 */
void cos_v77_toffoli(const cos_v77_reg_t *a,
                     const cos_v77_reg_t *b,
                     cos_v77_reg_t *c);

/* 6. Peres — self-inverse.
 *    (a, b, c) ↦ (a, a ⊕ b, c ⊕ (a ∧ b))
 *
 *    Composed of Toffoli(a, b, c) followed by CNOT(a, b).  The
 *    reverse is the same operation applied in opposite sequence,
 *    and because both halves are self-inverse Peres itself is
 *    self-inverse.
 */
void cos_v77_peres(const cos_v77_reg_t *a,
                   cos_v77_reg_t *b,
                   cos_v77_reg_t *c);

/* 7. Reversible majority-3 — self-inverse.
 *
 *    (a, b, c, d) ↦ (a, b, c, d ⊕ MAJ(a, b, c))
 *
 *    Operates on four registers: the three majority inputs pass
 *    through untouched, and the majority value is XOR'd into a
 *    dedicated ancilla register d.  Because d is disjoint from
 *    {a, b, c}, applying the gate twice cancels the XOR.  This is
 *    structurally analogous to Toffoli with the AND replaced by
 *    the MAJ function and is genuinely self-inverse.
 */
void cos_v77_maj3(const cos_v77_reg_t *a,
                  const cos_v77_reg_t *b,
                  const cos_v77_reg_t *c,
                  cos_v77_reg_t *d);

/* 8. Bennett forward/reverse driver.
 *
 *    Runs `prog` (length `prog_len`) on `rf` in direction `dir`.
 *    Returns  0 on success, <0 on malformed program.
 *
 *    If called twice in opposite directions on the same program
 *    with the same register file, the register file is bit-equal
 *    restored.  This is verified by the self-test.
 */
int cos_v77_bennett(cos_v77_regfile_t *rf,
                    const cos_v77_insn_t *prog,
                    size_t prog_len,
                    cos_v77_dir_t dir);

/* 9. Reversible 8-bit adder via Peres-gate chain.
 *
 *    dir = COS_V77_FORWARD:
 *        (a, b, c_in) ↦ (a, (a + b + c_in) mod 256, c_out)
 *
 *    dir = COS_V77_REVERSE:
 *        (a, s, c_in)  ↦ (a, (s - a - c_in) mod 256, 0)
 *
 *    Reads the low byte of the indexed register.  Works bit-by-bit
 *    so the entire add is reversible; the only non-identity mapping
 *    is on the operand register.
 */
void cos_v77_radd8(cos_v77_reg_t *a,
                   cos_v77_reg_t *b,
                   cos_v77_reg_t *c_in_out,
                   cos_v77_dir_t dir);

/* 10. RVL bytecode VM — all eight opcodes in one branchless switch
 *     table.  Returns  0 on success, <0 on malformed instruction.
 *
 *     If dir = COS_V77_REVERSE, the program is executed in reverse
 *     order with each opcode's own inverse.
 *
 *     The cost budget is measured in reversible-work units (see the
 *     table at the top of this file).  If the executed work exceeds
 *     `budget_units`, the VM still completes (so forward∘reverse
 *     remains sound) but returns a non-zero positive value equal to
 *     the overflow count.  Negative return codes indicate true
 *     errors (bad opcode, out-of-range register).
 */
int cos_v77_rvl_run(cos_v77_regfile_t *rf,
                    const cos_v77_insn_t *prog,
                    size_t prog_len,
                    uint32_t budget_units,
                    cos_v77_dir_t dir,
                    uint32_t *out_work_units);

/* ==================================================================
 *  3.  Gate + 17-bit compose.
 * ================================================================== */

/* v77 "OK" bit.  Branchless AND over:
 *   - every opcode in {0..7}
 *   - every register idx < NREGS
 *   - total reversible work ≤ budget
 *   - forward(prog) ∘ reverse(prog) yields the original rf
 */
uint32_t cos_v77_ok(const cos_v77_insn_t *prog,
                    size_t prog_len,
                    uint32_t budget_units);

/* 17-bit branchless composed decision.  Extends v76's 16-bit AND.
 *
 *    cos_v77_compose_decision(v76_ok, v77_ok)  = v76_ok & v77_ok
 *
 * Written as a function so callers cannot accidentally bypass it
 * and so future kernels can layer another AND on top without
 * touching v76's ABI.
 */
uint32_t cos_v77_compose_decision(uint32_t v76_compose_ok,
                                  uint32_t v77_ok);

/* ==================================================================
 *  4.  Introspection helpers — no decision state, just data.
 * ================================================================== */

/* Returns the reversible-work cost of a single opcode.  0 on
 * unknown opcode.  Branchless lookup. */
uint32_t cos_v77_op_cost(cos_v77_op_t op);

/* Returns 1 if the opcode is self-inverse (its own reverse), else 0. */
uint32_t cos_v77_op_is_self_inverse(cos_v77_op_t op);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif  /* COS_V77_REVERSIBLE_H */
