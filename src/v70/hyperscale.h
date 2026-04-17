/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v70 — σ-Hyperscale (the trillion-parameter / hyperscale-
 *                                 killer kernel).
 *
 * v60..v69 close the *single-fleet* loop: σ-Shield gates the action,
 * Σ-Citadel enforces the lattice, the Reasoning Fabric verifies energy,
 * σ-Cipher binds the envelope, σ-Intellect drives MCTS, σ-Hypercortex
 * carries the cortex, σ-Silicon turns thought into MAC ops, σ-Noesis
 * runs the deliberative beam with evidence receipts, σ-Mnemos remembers
 * and evolves across calls, σ-Constellation orchestrates the fleet.
 *
 * What is still missing is *substrate* — the integer floor that makes
 * a 10^12 .. 10^16-parameter model addressable from a single branchless
 * C kernel without paying the Blackwell / Stargate / Grok bill.  Every
 * 2026 hyperscale system either ships custom silicon (NVIDIA Blackwell
 * B200, AMD MI400, Google TPU v6 Trillium, Cerebras WSE-3, Groq LPU,
 * Lightmatter Envise photonic, Samsung HBM-PIM, Intel Loihi 3) or
 * stitches together an ad-hoc Python service plane over a fleet of
 * accelerators.  None ship the *primitives* of every one of those
 * systems as a single dependency-free, branchless, integer-only C
 * kernel that composes with the nine prior kernels via an 11-way
 * branchless AND.  v70 σ-Hyperscale does:
 *
 *   1. P2Q — Power-of-2 weight quantisation (ShiftAddLLM lineage,
 *      arXiv:2406.05981, NeurIPS 2024) — every weight is one of
 *      ±2^k for k ∈ [0..7] (4-bit: 1 sign + 3-bit exponent).  GEMV
 *      becomes branchless `acc += sgn ? -(a << k) : (a << k)`; the
 *      multiplier is gone.  No DSP, no matmul, no LUT — pure shift +
 *      conditional negate over `int32_t` accumulators.  Surpasses
 *      Blackwell FP4 by being *exactly* multiply-free — it is the
 *      ShiftAddLLM dispatch in C.
 *
 *   2. Mamba-2 selective SSM scan (Dao & Gu 2024 "Transformers are
 *      SSMs"; Mamba-3 arXiv:2603.15569; PyTorch SSD kernel-fusion
 *      blog 2025) — branchless Q0.15 selective state-space scan with
 *      content-dependent gates A, B, C, Δ.  Constant memory per
 *      token (no KV cache); linear in sequence length; matches
 *      Transformer perplexity at 1/state-size on Mamba-3.  The C
 *      kernel does the recurrence step in a tight branchless loop
 *      with `__builtin_prefetch` on the next state row.
 *
 *   3. RWKV-7 "Goose" delta-rule update (Peng *et al.*, 2025;
 *      arXiv:2503.14456, EleutherAI) — branchless integer time-mix
 *      + channel-mix with vector-valued gating and an in-context
 *      learning rate.  O(1) per token; can recognise all regular
 *      languages (Transformers are limited to TC^0 under standard
 *      complexity conjectures; RWKV-7 strictly exceeds that).  We
 *      ship the delta-rule update + token-shift mix as one Q0.15
 *      step; 100 % integer; no FP.
 *
 *   4. Block-sparse MoE with mmap'd disk-resident experts (DeepSeek-V3
 *      arXiv:2412.19437 lineage scaled to 10^4 experts) — branchless
 *      top-K MaxScore router over up to 10 240 experts; auxiliary-
 *      loss-free load balancing via integer bias adjustment (the
 *      DeepSeek-V3 contribution); only K=8 experts touched per
 *      token.  At 1.58 b ternary / 8192-d / 10 240-expert / 64-layer
 *      that is ≈ 200 GB cold on NVMe with ≈ 1.6 GB resident — i.e.
 *      a trillion-parameter model on a laptop.  At 10^4 experts ×
 *      10^4 layers and a quantised wire it scales toward the
 *      "10000 trillion parameter" headline as expert count rises.
 *      We expose only the integer router + bias controller; the
 *      mmap is a thin POSIX wrapper around the streaming scheduler
 *      in (9).
 *
 *   5. PIM (Processing-in-Memory) bit-serial AND-popcount surrogate
 *      (Samsung HBM-PIM, ISSCC 2021; RACAM bit-serial DRAM-PIM,
 *      arXiv:2603.09216; PAISE Samsung SDS 2026) — column-major
 *      bit-packed weights; matmul replaced by
 *      `__builtin_popcountll(act_word & weight_col)`.  This is the
 *      *exact* in-DRAM bit-serial primitive that HBM-PIM uses; we
 *      run it on the CPU as `cnt + addv` on AArch64.  RACAM
 *      reported 9 × .. 102 × speedup over GPUs end-to-end on LLM
 *      inference; v70 ships the kernel surface.
 *
 *   6. Photonic wavelength-multiplexed dot-product surrogate
 *      (Lightmatter Envise; SKYLIGHT arXiv:2602.19031; LightMat-HP
 *      arXiv:2604.12278; Single-Shot Matrix-Matrix Optical TPU
 *      arXiv:2503.24356, 4096 MAC/shot at 20 attojoules/MAC) —
 *      parallel integer accumulation lanes indexed by "wavelength
 *      channel" (default 8); branchless lane-select; demonstrates
 *      WDM-multiplexed dot product as integer SIMD.  Photonic
 *      hardware is wavelength-parallel; v70 captures that as
 *      lane-parallel `int32_t` accumulators with branchless
 *      reduction.  Same ABI shape as a real photonic engine.
 *
 *   7. Spike-coded sparse activation (Intel Loihi 3, January 2026:
 *      8 M neurons / 64 B synapses / 4 nm, graded 32-bit spikes;
 *      MatMul-free LLM on Loihi 2, arXiv:2503.18002, 3 × throughput
 *      and 2 × less energy than transformer baselines on edge GPUs;
 *      6 × more efficient than traditional hardware on text
 *      classification at 3.66 W) — branchless threshold spike
 *      trigger on Q0.15 activations; only spiking neurons
 *      accumulate; energy-proportional to activity (the whole point
 *      of neuromorphic).  We ship the integer spike-encode + sparse-
 *      readout as a single branchless pass, including the Loihi 3
 *      "graded spike" generalisation (signed Q0.15 spike payload).
 *
 *   8. Ring all-reduce (NVIDIA NCCL; Patarasuk & Yuan 2009; the
 *      Blackwell B200 NVLink-5 1.8 TB/s topology) — bandwidth-
 *      optimal branchless integer reduce-scatter + all-gather over
 *      N "ranks" arranged in a ring.  Demonstrates NVLink topology
 *      cost on a single CPU as N round-trips of (N-1) chunks; the
 *      arithmetic is `int32_t` add with saturation; the schedule
 *      is the canonical 2 (N-1) step Patarasuk-Yuan.  Same ABI as
 *      MPI_Allreduce(SUM) — but branchless and integer.
 *
 *   9. Streaming weight scheduler (Petals arXiv:2209.01188;
 *      Helix arXiv:2406.01566; Microsoft DirectStorage / NVIDIA
 *      GPUDirect Storage; the AMD Ryzen AI Max+ trillion-parameter
 *      cluster guide, Feb 2026) — N-slot branchless LRU over
 *      `(layer, expert)` keys with `__builtin_prefetch`-backed
 *      loads; eviction is a single saturating-counter compare.  The
 *      cache key is 64 bits (`layer << 32 | expert`); the slot
 *      counter is an unsigned monotone clock.  The scheduler
 *      surface is what Petals would expose if it were one C
 *      function instead of a Python service.
 *
 *  10. HSL — Hyperscale Language — 10-opcode integer bytecode ISA:
 *      `HALT / SHIFT / SCAN / TIMEMIX / ROUTEK / PIMPOP / WDM /
 *       SPIKE / RING / GATE`.  Each instruction is 8 bytes.  The
 *      interpreter tracks per-instruction silicon-unit cost
 *      (HALT = 1, SHIFT = 1, SCAN = 4, TIMEMIX = 4, ROUTEK = 8,
 *      PIMPOP = 2, WDM = 2, SPIKE = 1, RING = 8, GATE = 1) and
 *      writes `v70_ok = 1` iff:
 *          silicon_cost  ≤ silicon_budget
 *          AND reg_q15[a] ≥ imm           (throughput floor)
 *          AND topology_ok == 1           (ring closed, balanced)
 *          AND abstained == 0
 *      So no hyperscale program produces `v70_ok = 1` without
 *      simultaneously demonstrating silicon-budget compliance,
 *      throughput-floor compliance, and a balanced topology — the
 *      Patarasuk-Yuan + DeepSeek-V3 + ShiftAddLLM discipline as a
 *      single branchless AND.
 *
 * Composition (11-bit branchless decision):
 *
 *   v60 σ-Shield        — was the action allowed?
 *   v61 Σ-Citadel       — does the data flow honour BLP + Biba?
 *   v62 Reasoning       — is the EBT energy below budget?
 *   v63 σ-Cipher        — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect     — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex   — is the thought on-manifold + under HVL cost?
 *   v66 σ-Silicon       — does the matrix path clear conformal +
 *                         MAC budget?
 *   v67 σ-Noesis        — does the deliberation close + receipts?
 *   v68 σ-Mnemos        — does recall ≥ threshold AND forget ≤ budget?
 *   v69 σ-Constellation — does quorum margin ≥ threshold AND
 *                         Byzantine faults ≤ budget?
 *   v70 σ-Hyperscale    — does the silicon-substrate path clear
 *                         silicon budget AND throughput floor AND
 *                         topology balance?
 *
 * `cos_v70_compose_decision` is a single 11-way branchless AND.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - Every arena `aligned_alloc(64, …)`; the SSM state, RWKV state,
 *     MoE expert pool, PIM column-major matrix, WDM lane buffer,
 *     spike vector, ring buffers, LRU slot table, and HSL state are
 *     64-byte aligned at construction; hot paths never allocate.
 *   - All arithmetic is integer (`int32_t` accumulators, Q0.15
 *     activations); no FP on any decision surface.  No softmax,
 *     no exp, no division on the hot path.  P2Q removes the
 *     multiplier itself — pure shift + add.
 *   - PIM kernel is `__builtin_popcountll` over column-major bit-
 *     packed weights — native `cnt + addv` on AArch64; surrogate
 *     for in-DRAM compute with the same ABI.
 *   - The HSL interpreter tracks per-instruction silicon-unit cost
 *     and refuses on over-budget without a data-dependent branch on
 *     the cost itself (`reg_q15[a] >= imm` is one `int16_t` compare).
 *   - All capabilities are *constant-time in the payload contents*;
 *     they are *not* constant-time in the model size — that is the
 *     entire point: scaling with N (experts, layers, ranks) is what
 *     makes the substrate hyperscale.
 *
 * No dependencies beyond libc.  No mmap is required by the kernel
 * itself (the streaming scheduler exposes a slot-table abstraction
 * that any backing store — mmap, NVMe DirectStorage, GPUDirect
 * Storage, Petals shard server — can plug into; the kernel is
 * pure integer C).
 *
 * 1 = 1.
 */

#ifndef COS_V70_HYPERSCALE_H
#define COS_V70_HYPERSCALE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------
 *  Constants and limits.
 * -------------------------------------------------------------------- */

#define COS_V70_P2Q_BITS                 4u    /* 1 sign + 3 exponent */
#define COS_V70_P2Q_EXP_MAX              7u    /* shift in [0..7] */
#define COS_V70_P2Q_DIM_MAX           4096u    /* per-row P2Q dim cap */

#define COS_V70_SSM_DIM_MAX            128u    /* SSM hidden state */
#define COS_V70_SSM_TOKENS_MAX        4096u    /* SSM scan length cap */

#define COS_V70_RWKV_DIM_MAX           128u
#define COS_V70_RWKV_TOKENS_MAX       4096u

#define COS_V70_MOE_EXPERTS_MAX     10240u    /* 10 240-expert router */
#define COS_V70_MOE_TOPK_MAX           16u

#define COS_V70_PIM_LANES              64u    /* one 64-bit word per lane */
#define COS_V70_PIM_COLS_MAX         4096u    /* PIM column count cap */

#define COS_V70_WDM_LANES               8u    /* wavelength channels */

#define COS_V70_SPIKE_NEURONS_MAX    1024u    /* graded-spike vector */

#define COS_V70_RING_RANKS_MAX         32u    /* NVLink-5 ring rank cap */
#define COS_V70_RING_BUFFER_MAX      4096u    /* per-rank chunk cap */

#define COS_V70_STREAM_SLOTS           64u    /* LRU slot table */

#define COS_V70_NREGS                  16u    /* HSL register file */
#define COS_V70_PROG_MAX             1024u    /* HSL program cap */

/* Default thresholds (Q0.15). */
#define COS_V70_THROUGHPUT_DEFAULT_Q15  ((int16_t)6554)   /* ~0.20 floor */
#define COS_V70_SPIKE_THRESH_DEFAULT_Q15 ((int16_t)8192)  /* ~0.25 fire */
#define COS_V70_RWKV_GATE_DEFAULT_Q15   ((int16_t)16384)  /* ~0.50 mix  */

/* --------------------------------------------------------------------
 *  1. P2Q — Power-of-2 weight quantisation (ShiftAddLLM lineage).
 *
 *  A `cos_v70_p2q_w_t` packs one weight into 4 bits:
 *      bit  3   : sign     (1 = negative, 0 = non-negative)
 *      bits 2-0 : exponent (shift amount k ∈ [0..7])
 *  The reconstructed weight is `(sign ? -1 : +1) << k`, so the
 *  multiply `w * a` becomes a branchless `(a << k)` with a
 *  conditional sign flip — exactly the ShiftAddLLM dispatch.  We
 *  pack two weights per byte (column-major).
 * -------------------------------------------------------------------- */

typedef uint8_t cos_v70_p2q_byte_t;   /* two 4-bit weights packed */

/* Pack a signed integer-shifted scalar (sign in bit 3, exp in bits 2..0)
 * into a P2Q nibble.  Saturating in `exp` ∈ [0..7]. */
uint8_t cos_v70_p2q_pack(int8_t sign, uint8_t exp);

/* Unpack to (sign ∈ {-1,+1}, exp ∈ [0..7]). */
void cos_v70_p2q_unpack(uint8_t nib, int8_t *sign, uint8_t *exp);

/* Multiply-free GEMV:  out[r] = sum_c (sign(W[r,c]) << exp(W[r,c])) * x[c]
 *   x:    in[c]  Q0.15 (`int16_t`)                                       *
 *   W:    `weights` packed, two nibbles per byte, row-major;             *
 *         `cols` columns per row, `rows` rows total.                     *
 *   out:  `int32_t` accumulator per row (caller saturates).              *
 *
 * Branchless: sign flip is `(acc ^ mask) - mask` with `mask = -sign_bit`.
 * No multiply; pure `int32_t` shift + add. */
void cos_v70_p2q_gemv(const cos_v70_p2q_byte_t *weights,
                      const int16_t            *x_q15,
                      uint32_t                  rows,
                      uint32_t                  cols,
                      int32_t                  *out);

/* --------------------------------------------------------------------
 *  2. Mamba-2 selective SSM scan (linear-time, no KV cache).
 *
 *  Per-token recurrence (Q0.15):
 *      h[t] = sat( a_q15[t] * h[t-1] >> 15 + b_q15[t] * x[t] >> 15 )
 *      y[t] = sat( c_q15[t] * h[t]   >> 15 )
 *  This is the 1-D scalar form of the SSM recurrence; the full
 *  multi-head form is a tile of these.  Constant memory per token;
 *  linear in `n_tokens`.  Branchless saturating arithmetic.
 * -------------------------------------------------------------------- */

void cos_v70_ssm_scan(const int16_t *a_q15,    /* size n_tokens */
                      const int16_t *b_q15,
                      const int16_t *c_q15,
                      const int16_t *x_q15,
                      uint32_t       n_tokens,
                      int16_t        h0_q15,
                      int16_t       *h_out,    /* size n_tokens */
                      int16_t       *y_out);   /* size n_tokens */

/* --------------------------------------------------------------------
 *  3. RWKV-7 "Goose" delta-rule update (O(1) per token).
 *
 *  Per-channel update (Q0.15):
 *      gate    = sat( w_q15 * x_q15 >> 15 + u_q15 )         (vector gate)
 *      decay   = sat( decay_q15 * state_q15 >> 15 )         (delta-rule decay)
 *      v_eff   = sat( v_q15 - state_q15 )                   (relaxed value rep)
 *      state'  = sat( decay + sat( gate * v_eff >> 15 ) )
 *      y       = sat( r_q15 * state' >> 15 )
 *  Channel-local; branchless saturating arithmetic; no FP, no exp.
 * -------------------------------------------------------------------- */

void cos_v70_rwkv7_step(const int16_t *r_q15,
                        const int16_t *w_q15,
                        const int16_t *u_q15,
                        const int16_t *v_q15,
                        const int16_t *decay_q15,
                        const int16_t *x_q15,
                        int16_t       *state_q15,    /* in/out, size dim */
                        int16_t       *y_out,        /* size dim */
                        uint32_t       dim);

/* --------------------------------------------------------------------
 *  4. Block-sparse MoE with auxiliary-loss-free load balancing
 *     (DeepSeek-V3 lineage scaled to 10 240 experts).
 *
 *  - `scores_q15[e]` is the per-expert raw routing score.
 *  - `bias_q15[e]`   is the auxiliary-loss-free load bias (DeepSeek-V3
 *    contribution): added to `scores_q15` before top-K selection;
 *    updated by the controller after each call to push under-utilised
 *    experts up and over-utilised ones down — *no auxiliary loss* on
 *    the training side.
 *  - `selected[k]` receives the top-K expert indices (branchless
 *    bubble; not stable, deterministic).
 *  - `load_counter[e]` is incremented for every selected expert.
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t selected[COS_V70_MOE_TOPK_MAX];
    int16_t  selected_score_q15[COS_V70_MOE_TOPK_MAX];
    uint8_t  k;
    uint8_t  len;
} cos_v70_moe_route_t;

void cos_v70_moe_route_topk(const int16_t      *scores_q15,
                            const int16_t      *bias_q15,    /* optional */
                            uint32_t            n_experts,
                            uint32_t            k,
                            cos_v70_moe_route_t *out,
                            uint32_t           *load_counter); /* size n_experts, optional */

/* DeepSeek-V3 auxiliary-loss-free balance update:
 *      bias[e] += step_q15 * sign(load_avg − load[e])
 * Branchless `sign()`. */
void cos_v70_moe_bias_update(int16_t        *bias_q15,
                             const uint32_t *load_counter,
                             uint32_t        n_experts,
                             int16_t         step_q15);

/* --------------------------------------------------------------------
 *  5. PIM (Processing-in-Memory) bit-serial AND-popcount surrogate.
 *
 *  Inputs are bit-packed:
 *      - `act_word`: a single 64-bit word of activations (one
 *        bit-serial slice).
 *      - `weights[c]`: column-major; one 64-bit word per column,
 *        `n_cols` columns.
 *  Output `out[c] += __builtin_popcountll(act_word & weights[c])`.
 *  This is the *exact* in-DRAM bit-serial primitive used by HBM-PIM
 *  / RACAM; we run it on the CPU as `cnt + addv` on AArch64.
 * -------------------------------------------------------------------- */

void cos_v70_pim_and_popcount(uint64_t        act_word,
                              const uint64_t *weights_col,
                              uint32_t        n_cols,
                              uint32_t       *out);

/* --------------------------------------------------------------------
 *  6. Photonic wavelength-multiplexed dot product (WDM lane SIMD).
 *
 *  Inputs `q[n][lanes]` and `k[n][lanes]` are interleaved by lane.
 *  Output `lane_out[lane]` is the lane-local Q0.15 dot product.  A
 *  branchless lane-select reduction folds them into a single scalar
 *  (sum across lanes).  Same ABI shape as a real WDM photonic engine
 *  (each wavelength channel is one independent dot product, then
 *  optical fan-out adds them).
 * -------------------------------------------------------------------- */

int32_t cos_v70_wdm_dot(const int16_t *q_q15,
                        const int16_t *k_q15,
                        uint32_t       n_per_lane,
                        uint32_t       lanes,           /* ≤ COS_V70_WDM_LANES */
                        int32_t       *lane_out);       /* size lanes, optional */

/* --------------------------------------------------------------------
 *  7. Spike-coded sparse activation (Loihi 3 / TrueNorth lineage).
 *
 *  Loihi 3 ships graded spikes (signed 32-bit payload).  We model
 *  this as Q0.15 activations:
 *      spike[i] = sat(a_q15[i] - thresh_q15)   (Q0.15 graded spike)
 *      fire[i]  = (a_q15[i] >= thresh_q15)     (sparse mask)
 *  Returns the number of firing neurons; `spikes_q15[i]` is the
 *  graded spike payload (0 if below threshold).  Branchless mask
 *  via `-(int16_t)(a >= thresh)`.
 * -------------------------------------------------------------------- */

uint32_t cos_v70_spike_encode(const int16_t *a_q15,
                              uint32_t       n,
                              int16_t        thresh_q15,
                              int16_t       *spikes_q15,    /* size n, optional */
                              uint8_t       *fire_mask);    /* size n, optional */

/* Sparse readout: y[r] = sum over firing i of (spike[i] * w[r,i]) >> 15.
 * Skips the multiply for non-firing neurons via mask AND. */
void cos_v70_spike_readout(const int16_t *spikes_q15,
                           const uint8_t *fire_mask,
                           const int16_t *w_q15,            /* row-major rows×n */
                           uint32_t       rows,
                           uint32_t       n,
                           int32_t       *y_out);

/* --------------------------------------------------------------------
 *  8. Ring all-reduce (NVIDIA NCCL / Patarasuk-Yuan 2009 lineage).
 *
 *  N ranks, each holding a chunk of `chunk_len` `int32_t` values.
 *  `ranks` is a logical view: `ranks[r]` points to rank r's buffer
 *  (size N * chunk_len; we slice it into N chunks of length
 *  chunk_len).  The schedule is the canonical 2(N-1) step
 *  reduce-scatter + all-gather; arithmetic is `int32_t` saturating
 *  add.
 *
 *  Returns 1 if every rank ends with the same `int32_t` sum, 0
 *  otherwise (topology imbalance / ring-broken).
 * -------------------------------------------------------------------- */

uint8_t cos_v70_ring_allreduce(int32_t **rank_buffers,
                               uint32_t  n_ranks,
                               uint32_t  chunk_len);

/* --------------------------------------------------------------------
 *  9. Streaming weight scheduler — N-slot LRU over (layer, expert).
 *
 *  64-bit key:  (uint64_t)layer << 32 | expert.
 *  Slot table is fixed at COS_V70_STREAM_SLOTS entries.  `tick` is
 *  a monotone clock; the LRU victim is the slot with the smallest
 *  `last_used`.  Branchless: victim selection is a `sel` reduction.
 *
 *  The kernel does NOT mmap on its own — it exposes the slot table
 *  abstraction; the caller plugs in an NVMe / mmap / DirectStorage /
 *  GPUDirect / Petals backing.  `prefetch_hint` invokes
 *  `__builtin_prefetch` on `addr` if the platform supports it.
 * -------------------------------------------------------------------- */

typedef struct {
    uint64_t key;          /* (layer << 32) | expert */
    uint64_t last_used;    /* monotone clock */
    void    *addr;         /* caller-owned backing buffer pointer */
    uint8_t  valid;
} cos_v70_stream_slot_t;

typedef struct {
    cos_v70_stream_slot_t slots[COS_V70_STREAM_SLOTS];
    uint64_t              tick;
    uint64_t              hits;
    uint64_t              misses;
    uint64_t              evictions;
} cos_v70_stream_cache_t;

void  cos_v70_stream_init(cos_v70_stream_cache_t *c);
void *cos_v70_stream_lookup(cos_v70_stream_cache_t *c,
                            uint32_t layer,
                            uint32_t expert);
/* Insert (layer,expert,addr); returns the chosen slot index.  If the
 * cache is full, evicts the LRU slot.  `prefetch_hint != 0` issues a
 * `__builtin_prefetch` on `addr`. */
uint32_t cos_v70_stream_insert(cos_v70_stream_cache_t *c,
                               uint32_t layer,
                               uint32_t expert,
                               void    *addr,
                               int      prefetch_hint);

/* --------------------------------------------------------------------
 * 10. HSL — Hyperscale Language (10-opcode integer ISA).
 *
 *  Cost model (silicon units):
 *      HALT    = 1
 *      SHIFT   = 1   (P2Q step)
 *      SCAN    = 4   (one SSM step)
 *      TIMEMIX = 4   (one RWKV-7 step)
 *      ROUTEK  = 8   (top-K MoE)
 *      PIMPOP  = 2
 *      WDM     = 2
 *      SPIKE   = 1
 *      RING    = 8
 *      GATE    = 1
 *
 *  GATE writes `v70_ok = 1` iff:
 *      cost          ≤ cost_budget
 *      AND reg_q15[a] ≥ imm           (throughput floor)
 *      AND topology_ok == 1           (ring closed, balanced)
 *      AND abstained == 0
 * -------------------------------------------------------------------- */

typedef enum {
    COS_V70_OP_HALT    = 0,
    COS_V70_OP_SHIFT   = 1,
    COS_V70_OP_SCAN    = 2,
    COS_V70_OP_TIMEMIX = 3,
    COS_V70_OP_ROUTEK  = 4,
    COS_V70_OP_PIMPOP  = 5,
    COS_V70_OP_WDM     = 6,
    COS_V70_OP_SPIKE   = 7,
    COS_V70_OP_RING    = 8,
    COS_V70_OP_GATE    = 9,
} cos_v70_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v70_inst_t;

typedef struct cos_v70_hsl_state cos_v70_hsl_state_t;

cos_v70_hsl_state_t *cos_v70_hsl_new(void);
void                 cos_v70_hsl_free(cos_v70_hsl_state_t *s);
void                 cos_v70_hsl_reset(cos_v70_hsl_state_t *s);

uint64_t cos_v70_hsl_cost(const cos_v70_hsl_state_t *s);
int16_t  cos_v70_hsl_reg_q15(const cos_v70_hsl_state_t *s, uint32_t i);
uint32_t cos_v70_hsl_reg_u32(const cos_v70_hsl_state_t *s, uint32_t i);
uint8_t  cos_v70_hsl_topology_ok(const cos_v70_hsl_state_t *s);
uint8_t  cos_v70_hsl_abstained(const cos_v70_hsl_state_t *s);
uint8_t  cos_v70_hsl_v70_ok(const cos_v70_hsl_state_t *s);

typedef struct {
    /* SHIFT (P2Q GEMV) */
    const cos_v70_p2q_byte_t *p2q_weights;
    const int16_t            *p2q_x_q15;
    uint32_t                  p2q_rows;
    uint32_t                  p2q_cols;
    int32_t                  *p2q_out;     /* size rows */

    /* SCAN (SSM scan) */
    const int16_t *ssm_a;
    const int16_t *ssm_b;
    const int16_t *ssm_c;
    const int16_t *ssm_x;
    uint32_t       ssm_n;
    int16_t        ssm_h0;
    int16_t       *ssm_h_out;
    int16_t       *ssm_y_out;

    /* TIMEMIX (RWKV-7 step) */
    const int16_t *rwkv_r;
    const int16_t *rwkv_w;
    const int16_t *rwkv_u;
    const int16_t *rwkv_v;
    const int16_t *rwkv_decay;
    const int16_t *rwkv_x;
    int16_t       *rwkv_state;             /* in/out */
    int16_t       *rwkv_y_out;
    uint32_t       rwkv_dim;

    /* ROUTEK (MoE top-K) */
    const int16_t        *moe_scores;
    const int16_t        *moe_bias;
    uint32_t              moe_n_experts;
    uint32_t              moe_k;
    cos_v70_moe_route_t  *moe_out;
    uint32_t             *moe_load_counter;

    /* PIMPOP (PIM AND-popcount) */
    uint64_t        pim_act_word;
    const uint64_t *pim_weights_col;
    uint32_t        pim_n_cols;
    uint32_t       *pim_out;

    /* WDM (photonic dot) */
    const int16_t *wdm_q;
    const int16_t *wdm_k;
    uint32_t       wdm_n_per_lane;
    uint32_t       wdm_lanes;
    int32_t       *wdm_lane_out;

    /* SPIKE (encode + readout) */
    const int16_t *spike_a_q15;
    uint32_t       spike_n;
    int16_t        spike_thresh_q15;
    int16_t       *spike_spikes_q15;       /* size spike_n */
    uint8_t       *spike_fire_mask;        /* size spike_n */
    const int16_t *spike_w_q15;            /* readout weights, rows×n */
    uint32_t       spike_rows;
    int32_t       *spike_y_out;            /* readout, size rows */

    /* RING (all-reduce) */
    int32_t **ring_buffers;
    uint32_t  ring_n_ranks;
    uint32_t  ring_chunk_len;
} cos_v70_hsl_ctx_t;

int cos_v70_hsl_exec(cos_v70_hsl_state_t *s,
                     const cos_v70_inst_t *prog,
                     uint32_t n,
                     cos_v70_hsl_ctx_t *ctx,
                     uint64_t cost_budget);

/* --------------------------------------------------------------------
 *  Composed 11-bit branchless decision.
 * -------------------------------------------------------------------- */

typedef struct {
    uint8_t v60_ok;
    uint8_t v61_ok;
    uint8_t v62_ok;
    uint8_t v63_ok;
    uint8_t v64_ok;
    uint8_t v65_ok;
    uint8_t v66_ok;
    uint8_t v67_ok;
    uint8_t v68_ok;
    uint8_t v69_ok;
    uint8_t v70_ok;
    uint8_t allow;
} cos_v70_decision_t;

cos_v70_decision_t cos_v70_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok);

/* --------------------------------------------------------------------
 *  Version string.
 * -------------------------------------------------------------------- */

extern const char cos_v70_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V70_HYPERSCALE_H */
