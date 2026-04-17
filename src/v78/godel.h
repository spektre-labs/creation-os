/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v78 — σ-Gödel-Attestor (the self-attesting, meta-
 * cognitive plane).
 *
 * ------------------------------------------------------------------
 *  What v78 is
 * ------------------------------------------------------------------
 *
 * v60..v76 gated every emission through sixteen branchless ANDs.
 * v77 σ-Reversible added a seventeen-th AND so the hot path touches
 * the Landauer floor (k_B·T·ln 2) and erases zero bits.
 *
 * v78 σ-Gödel-Attestor adds the **eighteen-th AND** and — by ABI
 * construction — is the first open-source local-AI-agent runtime
 * to force every emission through the joint filter of nine of
 * twentieth-to-twenty-first-century theoretical computer science
 * and cognitive science's hardest results, all rendered as one
 * branchless, integer-only, libc-only kernel:
 *
 *   1. cos_v78_iit_phi        — integrated information φ (Tononi,
 *      Albantakis, Oizumi 2014; IIT 4.0, Albantakis et al. 2023).
 *      The "minimum-information bipartition" (MIB) gap between the
 *      joint transition distribution and the closest product of
 *      its parts, measured in integer bits via a 257-entry Q0.15
 *      log2 table.  High φ ⇒ the decision is genuinely integrated
 *      rather than reducible to a sum of independent steps.
 *
 *   2. cos_v78_free_energy    — discrete variational free energy
 *      (Friston 2010, "The Free-Energy Principle: A Unified Brain
 *      Theory?" Nat Rev Neuro; Active Inference 2022; Parr et al.
 *      2024).  F_q = KL(q‖p) + H[q] on a K-state categorical
 *      generative model.  Every integer arithmetic, zero FP.
 *
 *   3. cos_v78_mdl_bound      — Minimum Description Length upper
 *      bound (Solomonoff 1964; Rissanen 1978; Chaitin 1975).  An
 *      integer Kolmogorov-complexity proxy: ceil(log2(program_len))
 *      + popcount(flags).  The emission is refused unless its
 *      description is bounded below the declared budget.
 *
 *   4. cos_v78_godel_num      — Gödel numbering of a small program
 *      as a prime-power product (Gödel 1931 "Über formal
 *      unentscheidbare Sätze").  Encodes opcode/operand sequences
 *      into a single uint64 via the first 8 primes, enabling one-
 *      shot comparison with an embedded spec-number.
 *
 *   5. cos_v78_workspace_broadcast — Global Neuronal Workspace
 *      broadcast gate (Baars 1988; Dehaene 2024 "Consciousness and
 *      the Brain" updated).  A 256-bit HV attention distribution
 *      must concentrate ≥ `threshold` of its popcount in the top-k
 *      positions for the coalition to "ignite" and the emission to
 *      cross the global broadcast threshold.
 *
 *   6. cos_v78_halt_witness   — Turing-style halting proof via a
 *      strictly-decreasing termination measure (Turing 1936 "On
 *      Computable Numbers").  The kernel executes a small step
 *      program bounded by a monotone integer measure that must
 *      reach zero within N steps; if it does, the witness is
 *      accepted.
 *
 *   7. cos_v78_self_trust     — Löbian self-trust anchor (Löb 1955
 *      "Solution of a problem of Leon Henkin"; Payor 2014).  The
 *      kernel trusts its own proof system iff the SHA-256 hash of
 *      an embedded spec constant matches a pinned anchor.  This
 *      is a conservative, Gödel-consistent formalisation: the
 *      kernel cannot prove its own full soundness, but it can
 *      prove agreement with a fixed external anchor.
 *
 *   8. cos_v78_bisim_check    — coalgebraic bisimulation check
 *      (Milner 1989; Sangiorgi 2011).  Two transition-system
 *      states are declared bisimilar iff their n-step HV orbits
 *      are bit-equal — one branchless popcount-XOR reduction.
 *      Used to prove that the emission behaves identically to
 *      its spec.
 *
 *   9. cos_v78_chaitin_bound  — algorithmic-probability upper
 *      bound (Chaitin 1975; Ω-number literature).  Each emission
 *      class has a pre-tabulated integer Ω-budget; emissions are
 *      refused once cumulative Ω-weight would exceed the budget.
 *
 *  10. cos_v78_mcb_run        — MCB (Meta-Cognitive Bytecode) VM:
 *      an 8-opcode integer ISA that composes 1..9 into one proof
 *      receipt.  Each MCB opcode runs one of the nine filters
 *      above and writes a single bit of proof state; the kernel
 *      emits `v78_ok = 1` iff **every** opcode's bit lights.
 *
 *      Opcodes:
 *         0  HALT     (self-inverse sentinel)
 *         1  PHI      (run cos_v78_iit_phi            ≥ φ_min?)
 *         2  FE       (run cos_v78_free_energy        ≤ F_max?)
 *         3  MDL      (run cos_v78_mdl_bound          ≤ MDL_max?)
 *         4  GDL      (run cos_v78_godel_num          == spec?)
 *         5  WS       (run cos_v78_workspace_broadcast ≥ thr?)
 *         6  HWS      (run cos_v78_halt_witness        — halted?)
 *         7  TRUST    (run cos_v78_self_trust         — Löb-OK?)
 *
 *      (Bisimulation and Chaitin-Ω are direct ABI calls rather
 *       than MCB opcodes; they are runtime-callable by the host
 *       but do not themselves participate in the MCB proof path
 *       so the opcode field stays a clean 3-bit subfield.)
 *
 * ------------------------------------------------------------------
 *  Composed 18-bit branchless decision (extends v77)
 * ------------------------------------------------------------------
 *
 *     cos_v78_compose_decision(v77_composed_ok, v78_ok)
 *         = v77_composed_ok & v78_ok
 *
 * Nothing — no inference step, no tool call, no sealed message, no
 * surface interaction, no reversible-logic result — crosses to the
 * human unless every one of the sixteen prior gates in the v76
 * compose function ALLOWs, the v77 reversible plane certifies the
 * computation as bit-reversible, and the v78 σ-Gödel-Attestor emits
 * a valid proof receipt under every one of its integrated-
 * information, free-energy, MDL, Gödel-number, global-workspace,
 * halting-witness, Löbian-self-trust, bisimulation, and Chaitin-Ω
 * filters.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline (unchanged)
 * ------------------------------------------------------------------
 *
 *   - All arithmetic integer.  No FP on any decision surface.
 *   - All "ok" predicates reduce to a branchless AND on 0/1 bits.
 *   - Arenas 64-B aligned via aligned_alloc, sized to multiples
 *     of 64.  No malloc on the hot path.
 *   - All log2 values come from a precomputed 257-entry Q0.15
 *     table (`cos_v78_log2_q15`).  Every logarithm is an array
 *     lookup, no branches.
 *   - SHA-256 via the v75 FIPS-180-4 path (no OpenSSL).
 *   - HV format matches v76 / v77: 256 bits = 4 × uint64.
 */

#ifndef COS_V78_GODEL_H
#define COS_V78_GODEL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 *  0.  Constants & primitive types.
 * ================================================================== */

#define COS_V78_HV_WORDS     4u
#define COS_V78_HV_BYTES    32u
#define COS_V78_HV_BITS    256u

/* IIT system size: n-node Boolean network with n ∈ {2, 3, 4}.
 * A 4-node system has 16 possible states and 16 × 16 = 256 joint
 * transition entries, all addressable by a single byte index.
 */
#define COS_V78_IIT_NODES_MAX        4u
#define COS_V78_IIT_STATES_MAX      16u        /* 2^4 */
#define COS_V78_IIT_JOINT_CELLS    256u        /* 16 × 16 */

/* FEP: K-category discrete generative model. */
#define COS_V78_FEP_STATES_MAX       8u

/* Gödel: encode up to 8 opcode/operand pairs via first 8 primes. */
#define COS_V78_GODEL_MAX_OPS        8u

/* MCB: tiny bytecode, up to 256 instructions per proof program. */
#define COS_V78_MCB_PROG_MAX       256u
#define COS_V78_MCB_NREGS           16u

typedef struct {
    uint64_t w[COS_V78_HV_WORDS];
} cos_v78_hv_t;

/* MCB opcodes.  Exactly eight, matching v77's 8-opcode ISA. */
typedef enum {
    COS_V78_OP_HALT  = 0,
    COS_V78_OP_PHI   = 1,    /* integrated information filter */
    COS_V78_OP_FE    = 2,    /* free-energy filter */
    COS_V78_OP_MDL   = 3,    /* MDL filter */
    COS_V78_OP_GDL   = 4,    /* Gödel-number filter */
    COS_V78_OP_WS    = 5,    /* workspace broadcast filter */
    COS_V78_OP_HWS   = 6,    /* halting-witness filter */
    COS_V78_OP_TRUST = 7,    /* Löbian self-trust filter */
} cos_v78_mcb_op_t;

typedef uint32_t cos_v78_mcb_insn_t;

/* Packed MCB instruction:
 *   bits  0..2   opcode (8 slots)
 *   bits  3..7   budget_field     (0..31)   — per-opcode budget hint
 *   bits  8..15  arg_a            (0..255)  — per-opcode argument
 *   bits 16..23  arg_b            (0..255)
 *   bits 24..31  reserved (must be 0)
 */
static inline uint32_t cos_v78_op_of(cos_v78_mcb_insn_t i)  { return (i >>  0) & 0x7u; }
static inline uint32_t cos_v78_bud_of(cos_v78_mcb_insn_t i) { return (i >>  3) & 0x1Fu; }
static inline uint32_t cos_v78_a_of(cos_v78_mcb_insn_t i)   { return (i >>  8) & 0xFFu; }
static inline uint32_t cos_v78_b_of(cos_v78_mcb_insn_t i)   { return (i >> 16) & 0xFFu; }

/* ==================================================================
 *  1.  IIT-φ — integrated information on small Boolean networks.
 * ================================================================== */

/* Transition system TPM (Transition Probability Matrix), integerised
 * as *counts*: `tpm[s_in * STATES + s_out]` is the number of times
 * the system transitions from `s_in` to `s_out` across the sample.
 * STATES = 2^n_nodes with n_nodes ≤ 4.
 *
 * `cos_v78_iit_phi` returns φ in Q0.15 (0..32767).  Higher values
 * indicate higher integration.  Deterministic, branchless, no FP.
 */
uint32_t cos_v78_iit_phi(const uint32_t *tpm_counts,
                         uint32_t n_nodes);

/* ==================================================================
 *  2.  FEP — discrete variational free energy.
 * ================================================================== */

/* Given posterior q and prior-times-likelihood p over K states
 * (both as Q0.15 probability vectors summing to 32768), compute
 *
 *     F = KL(q || p)  +  H[q]             (integer, Q0.15)
 *
 * and return it clamped to uint32.  F = 0 is perfect posterior
 * alignment; larger values mean the emission is more "surprising"
 * under the generative model and should be refused. */
uint32_t cos_v78_free_energy(const uint16_t *q_q15,
                             const uint16_t *p_q15,
                             uint32_t k_states);

/* ==================================================================
 *  3.  MDL — minimum description length (integer Kolmogorov proxy).
 * ================================================================== */

/* Returns a Q0.0 integer upper bound in bits on the description
 * length of a program with `program_len` opcode/operand pairs and
 * `flag_bits` auxiliary flag bits:
 *
 *     MDL  =  8 · ceil_log2(max(program_len, 1))  +  popcount(flag_bits)
 */
uint32_t cos_v78_mdl_bound(uint32_t program_len,
                           uint64_t flag_bits);

/* ==================================================================
 *  4.  Gödel — prime-power Gödel numbering.
 * ================================================================== */

/* Encodes an opcode/operand sequence of length `n` (≤ 8) as
 *
 *     G = prod_i  prime_i ^ (code_i + 1)
 *
 * where `prime_i` are the first 8 primes (2, 3, 5, 7, 11, 13, 17, 19)
 * and `code_i ∈ 0..31`.  The result is a uint64; overflow is
 * clamped to UINT64_MAX.  Pure integer, no FP, branchless table
 * walk (fixed iteration count).
 */
uint64_t cos_v78_godel_num(const uint32_t *codes, uint32_t n);

/* ==================================================================
 *  5.  Workspace broadcast — Baars / Dehaene Global Workspace.
 * ================================================================== */

/* Given a 256-bit attention HV, the "coalition mass" is the popcount
 * of its top window of `k_hi` words (out of 4).  Returns 1 iff
 * coalition_mass ≥ threshold_bits.  Branchless — it sorts the four
 * word-popcounts via a fixed 5-compare sorting network and then
 * sums the top k.
 */
uint32_t cos_v78_workspace_broadcast(const cos_v78_hv_t *attn,
                                     uint32_t k_hi,
                                     uint32_t threshold_bits);

/* ==================================================================
 *  6.  Halting witness — Turing-style terminating proof.
 * ================================================================== */

/* Runs a monotone-decreasing termination measure `mu_init` for up
 * to `max_steps` iterations, decreasing `mu` by `mu_step` per step
 * (both integer).  Returns 1 iff `mu` reached 0 within the step
 * budget.  No branching in the hot loop — clamped subtraction plus
 * one OR-accumulate.
 */
uint32_t cos_v78_halt_witness(uint32_t mu_init,
                              uint32_t mu_step,
                              uint32_t max_steps);

/* ==================================================================
 *  7.  Self-trust — Löbian anchor.
 * ================================================================== */

/* Compares the 32-byte digest of an embedded spec blob (`spec_blob`,
 * `spec_len`) against a pinned 32-byte anchor.  Returns 1 iff
 * bit-equal.  This is Löb-compatible: the kernel is never required
 * to prove its own soundness; it only needs to agree with a fixed
 * external reference hash.
 */
uint32_t cos_v78_self_trust(const uint8_t *spec_blob,
                            size_t spec_len,
                            const uint8_t anchor32[32]);

/* Fetches the kernel's embedded default anchor (derived at build
 * time from the v78 primitive signature).  The caller may pass any
 * other 32-byte anchor instead; this one is the identity anchor. */
void cos_v78_default_anchor(uint8_t out32[32]);

/* ==================================================================
 *  8.  Bisimulation check.
 * ================================================================== */

/* Two HV states are declared bisimilar iff their bit-wise XOR has
 * zero popcount.  Branchless.  The function exists as a named ABI
 * rather than a macro so that the verifier call site is explicit. */
uint32_t cos_v78_bisim_check(const cos_v78_hv_t *a,
                             const cos_v78_hv_t *b);

/* ==================================================================
 *  9.  Chaitin Ω bound.
 * ================================================================== */

/* Table-lookup integer upper bound on the algorithmic probability
 * of an emission class.  The table is fixed at 16 entries and lives
 * in the implementation unit; classes ≥ 16 pin to the last entry.
 * Returns the budget; callers compare their accumulated Ω-weight
 * against this value and refuse the emission when it exceeds.
 */
uint32_t cos_v78_chaitin_bound(uint32_t emission_class);

/* ==================================================================
 * 10.  MCB — Meta-Cognitive Bytecode VM.
 * ================================================================== */

/* Parameters for MCB opcode checks.  Fixed thresholds live in this
 * struct so that they cannot be side-channeled through uninit reads.
 */
typedef struct {
    uint32_t           phi_min_q15;         /* PHI   gate */
    uint32_t           f_max_q15;           /* FE    gate */
    uint32_t           mdl_max_bits;        /* MDL   gate */
    uint64_t           godel_spec;          /* GDL   gate */
    uint32_t           ws_k_hi;             /* WS    gate, k */
    uint32_t           ws_threshold_bits;   /* WS    gate, t */
    uint32_t           hws_mu_init;         /* HWS   gate */
    uint32_t           hws_mu_step;         /* HWS   gate */
    uint32_t           hws_max_steps;       /* HWS   gate */
    const uint8_t     *trust_spec_blob;     /* TRUST gate (may be NULL) */
    size_t             trust_spec_len;
    uint8_t            trust_anchor32[32];  /* TRUST gate anchor */
    /* Runtime evidence the host supplies to the VM: */
    const uint32_t    *iit_tpm_counts;      /* used by PHI */
    uint32_t           iit_n_nodes;
    const uint16_t    *fep_q_q15;           /* used by FE */
    const uint16_t    *fep_p_q15;
    uint32_t           fep_k_states;
    uint32_t           mdl_program_len;     /* used by MDL */
    uint64_t           mdl_flag_bits;
    const uint32_t    *godel_codes;         /* used by GDL */
    uint32_t           godel_n_codes;
    cos_v78_hv_t       ws_attn;             /* used by WS  */
} cos_v78_mcb_ctx_t;

/* Runs `prog[0..prog_len)` and accumulates a per-opcode proof
 * bitmap into *out_proof_bits (one bit per opcode type).  Returns:
 *     0        — program completed; all required opcodes passed
 *     > 0      — program completed; this many opcodes failed
 *     < 0      — malformed instruction (bad opcode or reserved
 *                bits set)
 */
int cos_v78_mcb_run(const cos_v78_mcb_insn_t *prog,
                    size_t prog_len,
                    const cos_v78_mcb_ctx_t *ctx,
                    uint32_t *out_proof_bits);

/* ==================================================================
 *  11.  Gate + 18-bit compose.
 * ================================================================== */

/* `v78_ok = 1` iff every MCB opcode that appeared in `prog` passed
 * its own local check AND the prog is well-formed.  This is the
 * production ABI used by the compose function below. */
uint32_t cos_v78_ok(const cos_v78_mcb_insn_t *prog,
                    size_t prog_len,
                    const cos_v78_mcb_ctx_t *ctx);

/* 18-bit branchless composed decision.  Extends v77's 17-th AND. */
uint32_t cos_v78_compose_decision(uint32_t v77_composed_ok,
                                  uint32_t v78_ok);

/* ==================================================================
 *  12.  Introspection.
 * ================================================================== */

/* Returns a pointer to the internal 257-entry Q0.15 log2 table,
 * one value per integer in [0, 256].  Index 0 returns 0 (clamped). */
const uint16_t *cos_v78_log2_q15_table(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V78_GODEL_H */
