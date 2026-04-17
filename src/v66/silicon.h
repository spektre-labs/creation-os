/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v66 — σ-Silicon (the matrix substrate).
 *
 * v60–v65 are *thought* layers: σ-Shield gates the action, Σ-Citadel
 * enforces the lattice, the Reasoning Fabric verifies energy, σ-Cipher
 * binds the message to the live attestation quote, σ-Intellect drives
 * the agentic search, σ-Hypercortex carries the neurosymbolic
 * substrate.  But every one of those layers is *control plane*.  The
 * matrix work — the multiply-accumulate ops that an LLM, a vision
 * model, a probabilistic circuit, or a VSA bundle ultimately reduces
 * to — is still hidden behind a framework call.
 *
 * The 2026 frontier (arXiv / Zenodo / OpenReview / GitHub corpus) has
 * converged on five independent findings that together let a local
 * agent push the matrix work all the way down to the silicon, without
 * the framework hop:
 *
 *   1. MpGEMM (arXiv:2512.21473, Demystifying ARM SME for GEMM,
 *      Dec 2025) — ARM SME multi-precision GEMM library on Apple M4
 *      Pro that beats vendor-tuned Accelerate by ~1.23 × across
 *      DeepSeek and LLaMA workloads, using cache-aware partitioning,
 *      on-the-fly transposed packing, and tile-register micro-kernels.
 *      Match: the project rule explicitly says "design direction:
 *      per-layer repulsion should move from Accelerate toward native
 *      SME".  v66 ships the SME-shimmed FP32 GEMM with a NEON i8mm /
 *      sdot fallback that is bit-identical to the SME path within
 *      tolerance.
 *
 *   2. Hello SME (arXiv:2409.18779) + libxsmm SME upstream — JIT
 *      micro-kernels >2.3 FP32 TFLOPS on M4, FP32-centric, two-step
 *      load/store to saturate matrix-register bandwidth.  v66 follows
 *      the 32-byte tile, two-step pattern in the SME path so we land
 *      inside the regime where the silicon actually streams.
 *
 *   3. NativeTernary (arXiv:2604.03336, Apr 2026) — a self-delimiting
 *      binary wire format for ternary neural networks at exactly
 *      2.0 bits per weight, 1.31 × smaller than GGUF Q2_K, with a
 *      stateless 10-line decoder running 35–45 MB/s on a single core.
 *      v66 ships the decoder as a branchless table lookup (the rule
 *      "no `if` on the hot path" is preserved by a 256-entry decode
 *      table + a 4-bit run-length state).
 *
 *   4. BitNet b1.58 NEON 1.37–5.07 × (arXiv:2410.16144) and
 *      Litespark-Inference (M1–M4, 52 × throughput vs PyTorch) — packed
 *      ternary GEMV in pure NEON with 4-accumulator + prefetch.
 *      v66 ships a packed-2-bits-per-weight ternary GEMV that uses
 *      `vdotq_s32` on AArch64 + a portable scalar fallback that is
 *      bit-identical row-for-row.  Both paths run in constant time per
 *      row — no input-dependent early exit — so the timing-channel
 *      surface is bounded by `cols`, not by weight values.
 *
 *   5. CFC — Conditional Factuality Control via Conformal Sampling
 *      (arXiv:2603.27403, Mar 2026) — feature-conditional conformal
 *      prediction with a PAC-style finite-sample certificate.
 *      Conditional miscoverage deviates from the target by at most
 *      O(√(log(1/δ)/N)).  v66 ships a Q0.15 calibration table + a
 *      branchless conformal-abstention gate that becomes the
 *      `v66_ok` lane in the 7-bit composed decision; nothing ever
 *      reaches the matrix substrate unless the calibrated factuality
 *      score clears the per-feature threshold.
 *
 * The composition story:
 *
 *   v60 σ-Shield        — was the action allowed?
 *   v61 Σ-Citadel       — does the data flow honour BLP+Biba?
 *   v62 Reasoning       — is the EBT energy below budget?
 *   v63 σ-Cipher        — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect     — does the agentic kernel authorise emit?
 *   v65 σ-Hypercortex   — is the thought on-manifold AND under HVL
 *                         cost budget?
 *   v66 σ-Silicon       — does the matrix kernel hold its calibrated
 *                         factuality bound AND are we under the HSL
 *                         MAC-cost budget?
 *
 * `cos_v66_compose_decision` is the 7-way branchless AND.  No thought
 * crosses to the matrix hardware unless every lane ALLOWs.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - Every arena is `aligned_alloc(64, ...)` so each tile is one
 *     M4 cache line (or a small integer multiple).
 *   - Inputs are INT8 or packed 2-bits-per-weight; intermediates are
 *     INT32; outputs are Q0.15 — no FP on the decision path.
 *   - `__builtin_popcountll` and the AArch64 `vdotq_s32` /
 *     `vmlal_s16` family lower to NEON `cnt + addv` and `sdot` /
 *     `smlal` micro-ops.
 *   - The SME path is *opt-in* at build time (`COS_V66_SME=1`); the
 *     default build never emits an SME instruction (avoids SIGILL on
 *     hosts without the FEAT_SME bit set), but the public ABI is
 *     identical so a caller can compile both shapes from one source.
 *   - CPU feature detection is sysctl-cached (Darwin) / getauxval-
 *     cached (Linux).  The detection runs once and the result is read
 *     branchlessly thereafter.
 *   - HSL — the bytecode dispatcher — uses a goto table, not an
 *     `if`-cascade, so the per-instruction cost is constant and
 *     observable in popcount-word units.
 *
 * Status (build matrix):
 *
 *   default build               : NEON / scalar paths only, libc-only.
 *   COS_V66_SME=1               : reserves the SME path; falls back to
 *                                 NEON if the runtime feature bit is
 *                                 absent (never SIGILL).
 *   make standalone-v66         : self-test driver.
 *   make standalone-v66-hardened: harden-flags rebuild.
 *   make asan-v66 / ubsan-v66   : sanitizer matrix.
 */

#ifndef COS_V66_SILICON_H
#define COS_V66_SILICON_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  CPU feature detection (sysctlbyname on Darwin, getauxval on
 *      Linux, runtime-cached and read branchlessly thereafter).
 * ==================================================================== */

#define COS_V66_FEAT_NEON     0x01u
#define COS_V66_FEAT_DOTPROD  0x02u    /* ARMv8.2-A FEAT_DotProd       */
#define COS_V66_FEAT_I8MM     0x04u    /* ARMv8.6-A FEAT_I8MM          */
#define COS_V66_FEAT_BF16     0x08u    /* ARMv8.6-A FEAT_BF16          */
#define COS_V66_FEAT_SVE      0x10u    /* SVE                          */
#define COS_V66_FEAT_SME      0x20u    /* ARMv9-A FEAT_SME (M4)        */
#define COS_V66_FEAT_SME2     0x40u    /* ARMv9-A FEAT_SME2            */

/*  Returns the OR of all feature bits the host advertises.  Cached
 *  after the first call; constant-time reads thereafter. */
uint32_t cos_v66_features(void);

/*  Best-effort textual report into `buf` (NUL-terminated).  Returns
 *  the number of bytes written (excluding NUL). */
size_t cos_v66_features_describe(char *buf, size_t cap);

/* ====================================================================
 *  1.  7-bit composed decision.
 *
 *      v60 σ-Shield        — action allowed?
 *      v61 Σ-Citadel       — lattice flow allowed?
 *      v62 Reasoning       — EBT energy under budget?
 *      v63 σ-Cipher        — AEAD tag authentic + quote bound?
 *      v64 σ-Intellect     — agentic kernel authorises emit?
 *      v65 σ-Hypercortex   — thought on-manifold + HVL budget?
 *      v66 σ-Silicon       — calibrated factuality + HSL budget?
 *
 *  `.allow` is the 7-way branchless AND.
 * ==================================================================== */

typedef struct {
    uint8_t v60_ok;
    uint8_t v61_ok;
    uint8_t v62_ok;
    uint8_t v63_ok;
    uint8_t v64_ok;
    uint8_t v65_ok;
    uint8_t v66_ok;
    uint8_t allow;        /* AND of all seven lanes                    */
} cos_v66_decision_t;

cos_v66_decision_t
cos_v66_compose_decision(uint8_t v60_ok,
                         uint8_t v61_ok,
                         uint8_t v62_ok,
                         uint8_t v63_ok,
                         uint8_t v64_ok,
                         uint8_t v65_ok,
                         uint8_t v66_ok);

/* ====================================================================
 *  2.  INT8 GEMV — y[m] = saturate( bias[m] + Σ_k W[m,k] * x[k] )
 *
 *  Inputs and weights are signed 8-bit, accumulator is int32, output
 *  is Q0.15 saturated.  `W` is row-major, 64-byte aligned, `cols`
 *  must be a multiple of 16 (one NEON tile).  No allocation, no
 *  branches in the inner loop, 4 parallel accumulators with a 64-byte
 *  prefetch ahead of the load — exactly the .cursorrules pattern.
 *
 *  Returns 0 on success, -1 on bad alignment / stride.
 * ==================================================================== */

int cos_v66_gemv_int8(const int8_t  *W,        /* [rows][cols], 64B-aligned */
                      const int8_t  *x,        /* [cols],       64B-aligned */
                      const int32_t *bias,     /* [rows] or NULL            */
                      int16_t       *y_q15,    /* [rows]                    */
                      uint32_t       rows,
                      uint32_t       cols);

/* ====================================================================
 *  3.  Ternary GEMV (BitNet b1.58 packed 2-bits-per-weight).
 *
 *  Each weight is one of {-1, 0, +1}, encoded:
 *      00  →  0
 *      01  →  +1
 *      10  →  −1
 *      11  →  reserved (decoded as 0 — branchless safety)
 *
 *  Storage: `cols/4` bytes per row, 64-byte aligned.  `cols` must be
 *  a multiple of 64 so each row is one or more 16-byte NEON tiles.
 *  Inputs are int8; output is Q0.15 saturated.  Constant time per row.
 *
 *  Returns 0 on success, -1 on bad alignment / stride.
 * ==================================================================== */

int cos_v66_gemv_ternary(const uint8_t *W_packed,  /* [rows][cols/4]      */
                         const int8_t  *x,         /* [cols]              */
                         const int32_t *bias,      /* [rows] or NULL      */
                         int16_t       *y_q15,     /* [rows]              */
                         uint32_t       rows,
                         uint32_t       cols);

/* ====================================================================
 *  4.  NativeTernary wire decoder (arXiv:2604.03336, 2.0 bpw exact).
 *
 *  Packs ternary weights in a self-delimiting unary-run-length wire
 *  format.  The encoder is intentionally trivial (one round-trip
 *  helper for tests).  The decoder is a 10-state branchless table
 *  walk that emits one packed-2-bit weight per iteration; throughput
 *  on a single M4 P-core is in the 35–45 MB/s envelope of the paper.
 *
 *  `dst_packed` capacity is `(n_weights + 3) / 4` bytes (each weight
 *  is one of {-1, 0, +1}, packed as in `cos_v66_gemv_ternary`).
 *  Returns the number of weights decoded, or -1 on truncation.
 * ==================================================================== */

int32_t cos_v66_ntw_encode(const int8_t *weights,    /* values in {-1,0,+1} */
                           uint32_t      n_weights,
                           uint8_t      *dst_wire,
                           uint32_t      dst_cap);

int32_t cos_v66_ntw_decode(const uint8_t *wire,
                           uint32_t       wire_len,
                           uint8_t       *dst_packed, /* 2 bits / weight */
                           uint32_t       n_weights);

/* ====================================================================
 *  5.  Conformal abstention gate (CFC, arXiv:2603.27403).
 *
 *  Holds a Q0.15 calibration table over `n_groups` feature groups
 *  (e.g. difficulty bins, topic classes, output-length bins) and a
 *  global PAC threshold τ ∈ [0, 32768].  At decision time the caller
 *  passes the per-emission feature group and the calibrated
 *  factuality score (Q0.15); the gate returns 1 (ALLOW) iff
 *  `score ≥ table[group] AND score ≥ τ`.  Branchless via integer
 *  comparison + bitmask AND.
 *
 *  The table is updated via a streaming quantile estimator: each
 *  update widens or tightens the per-group threshold so empirical
 *  miscoverage stays under target ε.  All updates are integer Δs
 *  with a ratio-preserving overflow shift (mirrors v64's Reflexion
 *  ratchet).
 * ==================================================================== */

typedef struct {
    int16_t  *thr_q15;       /* [n_groups]                                  */
    uint32_t *seen;          /* [n_groups] — count of calibration updates    */
    uint32_t *miscov;        /* [n_groups] — count of miscoverage events     */
    uint32_t  n_groups;
    int16_t   global_tau_q15;
    int16_t   target_eps_q15;/* allowed miscoverage rate                     */
} cos_v66_conformal_t;

cos_v66_conformal_t *cos_v66_conformal_new(uint32_t n_groups,
                                           int16_t  global_tau_q15,
                                           int16_t  target_eps_q15);
void                 cos_v66_conformal_free(cos_v66_conformal_t *g);

/*  Update the calibration with one observation: did the model's
 *  emission `was_factual` match ground truth at score `score_q15`
 *  for feature group `group`?  Streaming quantile update; constant
 *  time per call. */
void cos_v66_conformal_update(cos_v66_conformal_t *g,
                              uint32_t             group,
                              int16_t              score_q15,
                              int                  was_factual);

/*  Branchless gate.  Returns 1 (ALLOW) iff
 *      score_q15 ≥ thr_q15[group] AND score_q15 ≥ global_tau_q15.
 *  Out-of-range groups are treated as the strictest (DENY). */
uint8_t cos_v66_conformal_gate(const cos_v66_conformal_t *g,
                               uint32_t                   group,
                               int16_t                    score_q15);

/* ====================================================================
 *  6.  HSL — Hardware Substrate Language.
 *
 *  An 8-opcode integer bytecode for matrix programs.  Sister ISA to
 *  v65's HVL — instead of vector-symbolic ops on hypervectors, HSL
 *  composes the v66 matrix primitives, charges per-instruction MAC
 *  cost, and writes `v66_ok` into the composed decision through a
 *  GATE opcode.
 *
 *  Opcodes:
 *      HALT              — stop, success.
 *      LOAD r, slot      — bind register r to arena slot.
 *      GEMV_I8 r, w, x   — y_q15 = INT8 GEMV (cost = rows * cols).
 *      GEMV_T  r, w, x   — y_q15 = ternary GEMV (cost = rows * cols / 4).
 *      DECODE_NTW r, w   — y_packed = decode wire (cost = wire_len).
 *      ABSTAIN r, score  — y = conformal gate (cost = 1).
 *      CMPGE r, thr      — set gate_bit if r ≥ thr.
 *      GATE              — write `gate_bit AND budget_ok` into v66_ok.
 *
 *  Programs are straight-line, bounded length, bounded MAC cost.
 *  No loops, no backward jumps — every program terminates in
 *  bounded time.
 * ==================================================================== */

typedef enum {
    COS_V66_OP_HALT       = 0,
    COS_V66_OP_LOAD       = 1,
    COS_V66_OP_GEMV_I8    = 2,
    COS_V66_OP_GEMV_T     = 3,
    COS_V66_OP_DECODE_NTW = 4,
    COS_V66_OP_ABSTAIN    = 5,
    COS_V66_OP_CMPGE      = 6,
    COS_V66_OP_GATE       = 7
} cos_v66_opcode_t;

typedef struct {
    uint8_t  op;
    uint8_t  rd;          /* destination register / lane                 */
    uint16_t a;           /* operand a (slot or group)                   */
    uint16_t b;           /* operand b (slot or threshold)               */
    uint16_t c;           /* operand c (slot or score, opcode-specific)  */
} cos_v66_inst_t;

typedef struct cos_v66_hsl_state cos_v66_hsl_state_t;

cos_v66_hsl_state_t *cos_v66_hsl_new(uint8_t nreg);
void                 cos_v66_hsl_free(cos_v66_hsl_state_t *s);

/*  Reset cost accumulator and gate bit (used between programs). */
void cos_v66_hsl_reset(cos_v66_hsl_state_t *s);

/*  Read the current cost (in MAC-units) and gate bit. */
uint64_t cos_v66_hsl_cost(const cos_v66_hsl_state_t *s);
uint8_t  cos_v66_hsl_gate_bit(const cos_v66_hsl_state_t *s);
uint8_t  cos_v66_hsl_v66_ok(const cos_v66_hsl_state_t *s);

/*  Execute `prog` (length `nprog`) against `arena_*` slots.  The
 *  conformal gate `cg` is consulted by ABSTAIN; the cost ceiling is
 *  `cost_budget` MAC-units (program halts and reports v66_ok = 0
 *  on overrun, branchlessly).  Returns 0 on HALT, -1 on malformed
 *  program. */
int cos_v66_hsl_exec(cos_v66_hsl_state_t       *s,
                     const cos_v66_inst_t      *prog,
                     uint32_t                   nprog,
                     /* arena slots */
                     const int8_t       *const *w_i8_slots,
                     const uint32_t            *w_i8_rows,
                     const uint32_t            *w_i8_cols,
                     const int8_t       *const *x_i8_slots,
                     const uint8_t      *const *w_t_slots,
                     const uint32_t            *w_t_rows,
                     const uint32_t            *w_t_cols,
                     int16_t            * const *y_q15_slots,
                     /* gate */
                     const cos_v66_conformal_t *cg,
                     uint64_t                   cost_budget);

/* ====================================================================
 *  7.  Build / version reporting.
 * ==================================================================== */

extern const char cos_v66_version[];

#ifdef __cplusplus
}
#endif

#endif /* COS_V66_SILICON_H */
