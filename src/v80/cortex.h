/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v80 — σ-Cortex (the hypervector-space neocortical
 * reasoning plane).
 *
 * ------------------------------------------------------------------
 *  What v80 is
 * ------------------------------------------------------------------
 *
 * The composed-decision stack up to v79 decides *whether* to emit,
 * and gives the agent a simulation substrate for *what gets simulated*
 * before emission.  v80 adds the substrate for *how the agent
 * reasons* — a branchless, integer-only, libc-only hardware kernel
 * that collapses ten of the most impactful 2023–2025 sequence-model,
 * attention, routing and test-time-compute results into one
 * hypervector-space neocortex, compiled directly to hardware.
 *
 * Ten primitives, each grounded in a real paper:
 *
 *   1. cos_v80_ssm_step       — Selective State Space Model step
 *      (Mamba, Gu & Dao 2023, arXiv:2312.00752; Mamba-2, Dao & Gu
 *      2024, arXiv:2405.21060).  Diagonal linear recurrence
 *         h_t = A ⊙ h_{t-1} + B ⊙ u_t
 *         y_t = C · h_t
 *      in Q16.16 fixed-point, packed into a 4-lane hypervector.
 *      Linear-time, parallel-scan-friendly, subquadratic in seq len.
 *
 *   2. cos_v80_rope_rotate    — Rotary Position Embedding (Su et al.
 *      2021, "RoFormer: Enhanced Transformer with Rotary Position
 *      Embedding", arXiv:2104.09864).  Rotates pairs of Q16.16
 *      values by an integer sin/cos LUT — gives attention its
 *      relative-position inductive bias without any learnable
 *      parameters.
 *
 *   3. cos_v80_attn_window    — sliding-window / ring attention
 *      (Beltagy et al. 2020 Longformer arXiv:2004.05150; Mistral
 *      7B sliding window 2023; Liu et al. 2023 Ring Attention
 *      arXiv:2310.01889).  Branchless popcount-argmax over a
 *      256-bit window — turns "who attends to whom" into a pure
 *      bit-population-count decision.
 *
 *   4. cos_v80_kv_commit      — paged KV-cache commit (Kwon et al.
 *      2023 vLLM / PagedAttention arXiv:2309.06180).  Slot-indexed
 *      ring buffer with integer page tags — the same idea that
 *      pushed LLM inference throughput by > 2× in 2023.
 *
 *   5. cos_v80_spec_verify    — speculative decoding verify step
 *      (Leviathan, Kalman, Matias 2023, arXiv:2211.17192; Chen et
 *      al. DeepMind 2023 "Accelerating LLM inference with
 *      speculative sampling").  Integer accept/reject predicate for
 *      a draft sequence of up to 32 tokens — monotone in the
 *      popcount of agreeing positions.
 *
 *   6. cos_v80_free_energy    — variational free energy / active
 *      inference (Friston 2010 "The free-energy principle: a unified
 *      brain theory?" Nat. Rev. Neurosci.).  Integer upper bound on
 *      F = D_KL(q‖p) + H(p, likelihood)
 *      with Q16.16 log-sum-exp approximation.
 *
 *   7. cos_v80_kan_edge       — Kolmogorov-Arnold Network edge
 *      activation (Liu et al. 2024, "KAN: Kolmogorov-Arnold
 *      Networks", arXiv:2404.19756; Kolmogorov 1957 superposition
 *      theorem).  Integer cumulative spline via Q16.16 LUT on an
 *      edge instead of a node — 1-D learnable activation per
 *      connection, branchless.
 *
 *   8. cos_v80_ctm_tick       — Continuous Thought Machine oscillator
 *      tick (Sakana AI 2025, "Continuous Thought Machines").
 *      Phase-coupled integer Kuramoto update on a 256-bit HV
 *      oscillator bank:
 *         φ_i ← φ_i + ω_i + (K/N) Σ_j sin(φ_j − φ_i)
 *      using an 8-bit integer sin LUT — gives the agent a
 *      continuous-time neural fabric instead of a fixed-depth stack.
 *
 *   9. cos_v80_moe_route      — Mixture-of-Experts top-k expert
 *      routing (Shazeer et al. 2017 Outrageously Large Neural
 *      Networks arXiv:1701.06538; DeepSeek-MoE 2024 arXiv:2401.06066;
 *      Mixtral 2024).  Branchless selection sort over the expert
 *      gate logits; guarantees popcount(routed) == k.
 *
 *  10. cos_v80_ttc_run        — TTC (Test-Time Compute) VM, a
 *      16-opcode integer ISA that composes 1..9 into one reasoning
 *      program.  Test-time scaling in the o1 / DeepSeek-R1 2024–2025
 *      sense — the agent can spend more inference-time compute
 *      before emitting a token when the kernel decides it must.
 *      Opcodes:
 *          0  HALT   (sentinel)
 *          1  SSM    (SSM step)
 *          2  RPE    (RoPE rotate)
 *          3  ATT    (windowed attention)
 *          4  KVC    (KV-cache commit)
 *          5  SPV    (speculative verify)
 *          6  FEN    (free-energy update)
 *          7  KAN    (KAN edge activation)
 *          8  CTM    (CTM oscillator tick)
 *          9  MOE    (MoE top-k route)
 *         10  FOLD   (fold cortex state into receipt)
 *      11..15 reserved (must be zero).
 *
 * ------------------------------------------------------------------
 *  Composed 20-bit branchless decision (extends v79)
 * ------------------------------------------------------------------
 *
 *     cos_v80_compose_decision(v79_composed_ok, v80_ok)
 *         = v79_composed_ok & v80_ok
 *
 * `v80_ok = 1` iff, for the TTC program just executed:
 *   - the SSM hidden state stayed within the declared integer norm
 *     budget (no overflow/drift),
 *   - every KV-cache commit landed at a legal page slot and kept
 *     the ring-buffer sentinel intact,
 *   - every speculative-verify decision was monotone in the popcount
 *     of agreeing draft positions,
 *   - every MoE top-k route returned exactly k experts (no more,
 *     no fewer),
 *   - no TTC instruction was malformed.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline (unchanged)
 * ------------------------------------------------------------------
 *
 *   - All arithmetic integer; hidden state / Q/K/V in Q16.16
 *     fixed-point.
 *   - Branchless hot path; masks instead of `if`.
 *   - 64-byte aligned layouts; no malloc on the hot path.
 *   - libc-only (string.h + stdint.h).
 *   - HV format: 256 bits = 4 × uint64 (matches v65 / v76 / v77 /
 *     v78 / v79).
 */

#ifndef COS_V80_CORTEX_H
#define COS_V80_CORTEX_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 *  0.  Constants & primitive types.
 * ================================================================== */

#define COS_V80_HV_WORDS      4u
#define COS_V80_HV_BITS     256u

/* SSM hidden-state dimension (per-lane; we have 4 lanes). */
#define COS_V80_SSM_D        16u

/* RoPE: number of (q, k) pairs we rotate at a time. */
#define COS_V80_ROPE_PAIRS    8u

/* Sliding-window attention: window size in HV bits. */
#define COS_V80_ATTN_WIN     64u

/* KV cache: total pages, slots per page. */
#define COS_V80_KV_PAGES      8u
#define COS_V80_KV_SLOTS      8u

/* Maximum draft length for speculative decoding. */
#define COS_V80_SPEC_MAX_DRAFT  32u

/* Free-energy history length. */
#define COS_V80_FE_HISTORY    16u

/* KAN spline: number of LUT control points (power of 2). */
#define COS_V80_KAN_CP        16u

/* CTM: number of oscillators (must be ≤ 64 so phases fit in one
 * uint64 lane, though we use Q16.16 for precision). */
#define COS_V80_CTM_N          8u

/* MoE: total experts and top-k selection. */
#define COS_V80_MOE_EXPERTS   16u
#define COS_V80_MOE_TOPK       2u

/* TTC program max length. */
#define COS_V80_TTC_PROG_MAX 256u

/* Q16.16 fixed-point typedef (same as v79). */
typedef int32_t cos_v80_q16_t;

#define COS_V80_Q16_ONE    (1 << 16)
#define COS_V80_Q16_HALF   (1 << 15)
#define COS_V80_Q16_PI     205887     /* π · 2^16, rounded */
#define COS_V80_Q16_TWOPI  411775     /* 2π · 2^16 */

typedef struct {
    uint64_t w[COS_V80_HV_WORDS];
} cos_v80_hv_t;

/* ------------------------------------------------------------------
 *  1.  SSM (Mamba-style selective state space)
 * ------------------------------------------------------------------
 *  Diagonal recurrence.  `a[i]` is the per-dim pole (stable when
 *  |a[i]| < 1 in Q16.16).  `b[i]`, `c[i]` are the per-dim input and
 *  output projections.  `h[i]` is the hidden state.
 */
typedef struct {
    cos_v80_q16_t a[COS_V80_SSM_D];   /* Q16.16 diagonal A */
    cos_v80_q16_t b[COS_V80_SSM_D];   /* Q16.16 B          */
    cos_v80_q16_t c[COS_V80_SSM_D];   /* Q16.16 C          */
    cos_v80_q16_t h[COS_V80_SSM_D];   /* Q16.16 hidden     */
    cos_v80_q16_t norm_budget;        /* Q16.16 max |h_i|  */
    uint32_t      overflow;           /* 0 = ok, 1 = out of budget */
} cos_v80_ssm_t;

/* ------------------------------------------------------------------
 *  2.  RoPE (rotary position embedding)
 * ------------------------------------------------------------------
 *  Q, K laid out as interleaved pairs.  Position `pos` applied via
 *  integer sin/cos LUT of size COS_V80_ROPE_PAIRS.
 */
typedef struct {
    cos_v80_q16_t q[2u * COS_V80_ROPE_PAIRS];   /* even = q_x, odd = q_y */
    cos_v80_q16_t k[2u * COS_V80_ROPE_PAIRS];
    uint32_t      pos;
} cos_v80_rope_t;

/* ------------------------------------------------------------------
 *  3.  Sliding-window attention
 * ------------------------------------------------------------------
 *  `past` is a ring of 64 tokens each stored as a 64-bit hash.  The
 *  "attention" for the current query hash `q` is: for every slot i in
 *  the window, compute popcount(q ^ past[i]); the attended slot is
 *  the argmin.  Branchless because we use a selection-sort-style
 *  reduction with bitmask selects.
 */
typedef struct {
    uint64_t  past[COS_V80_ATTN_WIN];
    uint32_t  head;
    uint32_t  last_best_idx;
    uint32_t  last_best_dist;
} cos_v80_attn_t;

/* ------------------------------------------------------------------
 *  4.  Paged KV cache (vLLM style)
 * ------------------------------------------------------------------
 *  `pages` is a flat [pages][slots] uint64 array of KV hashes.
 *  `page_tag[p]` is a 32-bit page tag (0 = free).  `slot_head[p]` is
 *  the write-head into page p.  Sentinels at the end of each page
 *  must always read as 0xCAFEBABE.
 */
typedef struct {
    uint64_t  pages[COS_V80_KV_PAGES * COS_V80_KV_SLOTS];
    uint32_t  page_tag[COS_V80_KV_PAGES];
    uint32_t  slot_head[COS_V80_KV_PAGES];
    uint32_t  next_free_page;
    uint32_t  sentinel;     /* invariant: 0xCAFEBABEu */
    uint32_t  invariant_violations;
} cos_v80_kv_t;

/* ------------------------------------------------------------------
 *  5.  Speculative decode verify
 * ------------------------------------------------------------------
 *  The draft model produced `draft_len` tokens each packed as 32-bit
 *  hashes; the target model produced `target_len` token hashes.
 *  Verify: count the longest common prefix; accept that many.
 */
typedef struct {
    uint32_t  draft[COS_V80_SPEC_MAX_DRAFT];
    uint32_t  target[COS_V80_SPEC_MAX_DRAFT];
    uint32_t  draft_len;
    uint32_t  target_len;
    uint32_t  last_accepted;
    uint32_t  last_nonmonotone;   /* must stay 0 */
} cos_v80_spec_t;

/* ------------------------------------------------------------------
 *  6.  Free energy (active inference)
 * ------------------------------------------------------------------
 *  Track a small Q16.16 history of surprise; variational free energy
 *  bound is mean + variance proxy.
 */
typedef struct {
    cos_v80_q16_t surprise[COS_V80_FE_HISTORY];
    uint32_t      head;
    cos_v80_q16_t last_f;    /* last free-energy bound, Q16.16 */
} cos_v80_fe_t;

/* ------------------------------------------------------------------
 *  7.  KAN edge
 * ------------------------------------------------------------------
 *  A 1-D cumulative spline.  Input in Q16.16; output = LUT-lookup +
 *  linear interpolation between two integer control points.
 */
typedef struct {
    cos_v80_q16_t cp[COS_V80_KAN_CP];   /* Q16.16 control points */
    cos_v80_q16_t last_out;
} cos_v80_kan_t;

/* ------------------------------------------------------------------
 *  8.  CTM (Continuous Thought Machine) oscillator bank
 * ------------------------------------------------------------------
 *  Each oscillator has a Q16.16 phase and a per-oscillator natural
 *  frequency in Q16.16.  Coupling K is Q16.16.
 */
typedef struct {
    cos_v80_q16_t phi[COS_V80_CTM_N];
    cos_v80_q16_t omega[COS_V80_CTM_N];
    cos_v80_q16_t K;
    uint32_t      step_count;
} cos_v80_ctm_t;

/* ------------------------------------------------------------------
 *  9.  MoE top-k router
 * ------------------------------------------------------------------
 *  `gate[i]` is the Q16.16 gate logit for expert i.  After routing,
 *  `routed` is a bitmask of selected experts and must satisfy
 *  popcount(routed) == k.
 */
typedef struct {
    cos_v80_q16_t gate[COS_V80_MOE_EXPERTS];
    uint32_t      routed;       /* bitmask */
    uint32_t      k;            /* exact count (must equal COS_V80_MOE_TOPK after route) */
    uint32_t      invariant_violations;
} cos_v80_moe_t;

/* ------------------------------------------------------------------
 *  10.  Cortex receipt (folded, v72-compatible)
 * ------------------------------------------------------------------ */
typedef struct {
    cos_v80_hv_t  receipt;
    uint32_t      step_count;
    cos_v80_q16_t last_energy;
} cos_v80_receipt_t;

/* ------------------------------------------------------------------
 *  One complete cortex.
 * ------------------------------------------------------------------ */
typedef struct {
    cos_v80_ssm_t      ssm;
    cos_v80_rope_t     rope;
    cos_v80_attn_t     attn;
    cos_v80_kv_t       kv;
    cos_v80_spec_t     spec;
    cos_v80_fe_t       fe;
    cos_v80_kan_t      kan;
    cos_v80_ctm_t      ctm;
    cos_v80_moe_t      moe;
    cos_v80_receipt_t  receipt;
} cos_v80_cortex_t;

/* ------------------------------------------------------------------
 *  TTC opcodes — packed into uint32:
 *    bits  0..3   opcode                (16 slots)
 *    bits  4..11  param_a               (0..255)
 *    bits 12..19  param_b               (0..255)
 *    bits 20..27  param_c               (0..255)
 *    bits 28..31  reserved (must be 0)
 * ------------------------------------------------------------------ */
typedef enum {
    COS_V80_OP_HALT = 0,
    COS_V80_OP_SSM  = 1,
    COS_V80_OP_RPE  = 2,
    COS_V80_OP_ATT  = 3,
    COS_V80_OP_KVC  = 4,
    COS_V80_OP_SPV  = 5,
    COS_V80_OP_FEN  = 6,
    COS_V80_OP_KAN  = 7,
    COS_V80_OP_CTM  = 8,
    COS_V80_OP_MOE  = 9,
    COS_V80_OP_FOLD = 10,
} cos_v80_op_t;

typedef uint32_t cos_v80_ttc_insn_t;

static inline uint32_t cos_v80_op_of(cos_v80_ttc_insn_t i) { return (i >> 0)  & 0xFu; }
static inline uint32_t cos_v80_a_of (cos_v80_ttc_insn_t i) { return (i >> 4)  & 0xFFu; }
static inline uint32_t cos_v80_b_of (cos_v80_ttc_insn_t i) { return (i >> 12) & 0xFFu; }
static inline uint32_t cos_v80_c_of (cos_v80_ttc_insn_t i) { return (i >> 20) & 0xFFu; }

/* ==================================================================
 *  API.
 * ================================================================== */

/* ---------- 1. SSM ----------------------------------------------- */
void cos_v80_ssm_init(cos_v80_ssm_t *s,
                      cos_v80_q16_t pole,
                      cos_v80_q16_t b,
                      cos_v80_q16_t c,
                      cos_v80_q16_t norm_budget);
void cos_v80_ssm_step(cos_v80_ssm_t *s, cos_v80_q16_t u);
cos_v80_q16_t cos_v80_ssm_readout(const cos_v80_ssm_t *s);

/* ---------- 2. RoPE ---------------------------------------------- */
void cos_v80_rope_init(cos_v80_rope_t *r);
void cos_v80_rope_rotate(cos_v80_rope_t *r, uint32_t pos);

/* ---------- 3. Sliding-window attention -------------------------- */
void cos_v80_attn_init(cos_v80_attn_t *a);
void cos_v80_attn_window(cos_v80_attn_t *a, uint64_t query_hash);

/* ---------- 4. Paged KV commit ----------------------------------- */
void cos_v80_kv_init(cos_v80_kv_t *kv);
/* Commit `hash` under `page_tag`; allocates a fresh page if needed.
 * Returns the page index used, or -1 on invariant violation. */
int  cos_v80_kv_commit(cos_v80_kv_t *kv, uint32_t page_tag, uint64_t hash);
uint32_t cos_v80_kv_invariant_ok(const cos_v80_kv_t *kv);

/* ---------- 5. Speculative verify -------------------------------- */
void cos_v80_spec_init(cos_v80_spec_t *sp);
void cos_v80_spec_set_draft(cos_v80_spec_t *sp,
                            const uint32_t *draft,
                            uint32_t draft_len);
void cos_v80_spec_set_target(cos_v80_spec_t *sp,
                             const uint32_t *target,
                             uint32_t target_len);
/* Returns the number of accepted tokens (0..min(draft,target)). */
uint32_t cos_v80_spec_verify(cos_v80_spec_t *sp);
uint32_t cos_v80_spec_invariant_ok(const cos_v80_spec_t *sp);

/* ---------- 6. Free energy --------------------------------------- */
void cos_v80_fe_init(cos_v80_fe_t *fe);
/* Feed a single surprise sample; updates history and last_f. */
void cos_v80_free_energy(cos_v80_fe_t *fe, cos_v80_q16_t surprise_q16);

/* ---------- 7. KAN edge ------------------------------------------ */
void cos_v80_kan_init(cos_v80_kan_t *kan, uint64_t seed);
cos_v80_q16_t cos_v80_kan_edge(cos_v80_kan_t *kan, cos_v80_q16_t x);

/* ---------- 8. CTM oscillator tick ------------------------------- */
void cos_v80_ctm_init(cos_v80_ctm_t *c);
void cos_v80_ctm_tick(cos_v80_ctm_t *c, cos_v80_q16_t dt);

/* ---------- 9. MoE top-k route ----------------------------------- */
void cos_v80_moe_init(cos_v80_moe_t *m);
void cos_v80_moe_set_gate(cos_v80_moe_t *m,
                          const cos_v80_q16_t *gate,
                          uint32_t len);
void cos_v80_moe_route(cos_v80_moe_t *m);
uint32_t cos_v80_moe_invariant_ok(const cos_v80_moe_t *m);

/* ---------- 10. TTC VM ------------------------------------------- */
int cos_v80_ttc_run(cos_v80_cortex_t *cortex,
                    const cos_v80_ttc_insn_t *prog,
                    size_t prog_len);

/* ---------- 11. Receipt ------------------------------------------ */
void cos_v80_receipt_update(cos_v80_receipt_t *r,
                            const cos_v80_hv_t *state_hv);

/* ---------- 12. Gate + 20-bit compose ---------------------------- */
uint32_t cos_v80_ok(cos_v80_cortex_t *cortex,
                    const cos_v80_ttc_insn_t *prog,
                    size_t prog_len);

/* 20-bit branchless composed decision. */
uint32_t cos_v80_compose_decision(uint32_t v79_composed_ok,
                                  uint32_t v80_ok);

#ifdef __cplusplus
}
#endif

#endif /* COS_V80_CORTEX_H */
