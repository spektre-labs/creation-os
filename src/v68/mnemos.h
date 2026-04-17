/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v68 — σ-Mnemos (the continual-learning + episodic
 *                              memory + online-adaptation kernel).
 *
 * v60..v67 close the *one-shot* deliberation loop: σ-Shield gates the
 * action, Σ-Citadel enforces the lattice, the Reasoning Fabric
 * verifies energy, σ-Cipher binds the envelope, σ-Intellect drives
 * MCTS, σ-Hypercortex carries the cortex, σ-Silicon turns thought
 * into MAC ops, σ-Noesis runs the deliberative beam with evidence
 * receipts.
 *
 * What is still missing is *time*: a system that **remembers**,
 * **evolves**, and **learns** across calls — the property every 2026
 * frontier system either bolts on (vector DB, RAG cache, replay
 * buffer) or quietly fakes (in-context window).  v68 σ-Mnemos ships
 * the memory-and-learning plane as a single dependency-free,
 * branchless, integer-only C kernel:
 *
 *   1. Titans (Google Research, 2025; arXiv:2501.00663) — neural
 *      memory module that **writes at test time only on surprise**.
 *      v68 ships this as a single branchless gate
 *      (`cos_v68_surprise_gate`) over Q0.15 prediction error: write
 *      iff `|pred − obs| ≥ thresh`.  No FP, no second pass.
 *
 *   2. TTT — Test-Time Training (Sun et al., arXiv:2407.04620) — the
 *      model adapts on every input.  v68 ships a **Hebbian Q0.15
 *      outer-product update** (`cos_v68_hebb_update`) that writes a
 *      saturating rank-1 increment to a small adapter matrix on each
 *      surprise event.  Constant per-event cost; bounded by
 *      `COS_V68_ADAPT_R · COS_V68_ADAPT_C`.
 *
 *   3. ACT-R activation decay (Anderson 1996; Anderson & Lebière 1998;
 *      ACT-R 7.27, 2026) — chunk activation falls off with retrieval
 *      latency.  v68 ships a **branchless integer linear decay**
 *      (`cos_v68_actr_decay_q15`) — `A_t = max(0, A_0 − decay · dt)`
 *      computed with `sel_i32`.  No log, no division, no float.
 *
 *   4. Hippocampal pattern separation + completion (McClelland,
 *      McNaughton, O'Reilly 1995; 2026 systems neuroscience review,
 *      Nat Rev Neurosci).  v68 ships an **8 192-bit bipolar HV
 *      episodic store** with `__builtin_popcountll` Hamming recall
 *      (pattern completion) and XOR-bundle binding (pattern
 *      separation).  D = 8 192 bits → 1 024 bytes per HV; 64-byte
 *      aligned; capacity `COS_V68_EPISODES_MAX = 256` by default.
 *
 *   5. Sleep replay / consolidation (Diekelmann & Born, Nat Rev
 *      Neurosci 2010; 2024-2026 systems extensions: targeted memory
 *      reactivation, Schapiro et al.).  v68 ships
 *      `cos_v68_consolidate` — an **offline pass** that bundles
 *      every episode whose activation ≥ keep_thresh into a single
 *      long-term bipolar HV via majority XOR.  After consolidation
 *      the long-term HV is the canonical "what we remember" — the
 *      ring buffer can be safely forgotten.
 *
 *   6. Anti-catastrophic-forgetting ratchet (Kirkpatrick EWC 2017;
 *      DeepMind ASAL 2025) — the Hebbian learning rate is bounded by
 *      a Q0.15 ratchet that **never grows** between consolidations.
 *      Forgetting is opt-in via `cos_v68_forget` and is itself
 *      gated by a forget-budget (a single branchless compare).
 *
 *   7. Hippocampus-style content-addressable recall — given a query
 *      bipolar HV, return the top-K episodes by Hamming similarity
 *      (`cos_v68_recall`).  Recall fidelity is reported as a Q0.15
 *      score; the gate compares it against a threshold with one
 *      branchless instruction.
 *
 *   8. Forgetting controller — branchless prune of episodes whose
 *      decayed activation falls below `keep_thresh`.  The same
 *      controller also reports a `forget_count` register that the
 *      MML GATE opcode can compare against `forget_budget`, so a
 *      single bytecode program can guarantee both **enough recall**
 *      and **bounded forgetting**.
 *
 *   9. Meta-controller (Soar / ACT-R / LIDA 2026 synthesis) — a
 *      single branchless 4-bit FSM (SENSE → SURPRISE → WRITE/SKIP →
 *      DECAY → CONSOLIDATE if epoch%N==0) that drives every other
 *      capability.  No data-dependent branch on episode contents;
 *      timing is independent of *what* was observed.
 *
 *  10. MML — Mnemonic Memory Language — a 10-opcode integer bytecode
 *      ISA: `HALT / SENSE / SURPRISE / STORE / RECALL / HEBB /
 *      CONSOLIDATE / FORGET / CMPGE / GATE`.  Each instruction is
 *      8 bytes.  The interpreter tracks per-instruction memory-unit
 *      cost; `GATE` writes `v68_ok = 1` iff:
 *          mem_cost      ≤ cost_budget
 *          AND reg_q15[a] ≥ imm                (recall fidelity)
 *          AND forget_count ≤ forget_budget    (no runaway forgetting)
 *          AND abstained == 0
 *      So no continual-learning program produces `v68_ok = 1`
 *      without simultaneously demonstrating recall ≥ threshold and
 *      forgetting ≤ budget — the Kirkpatrick EWC discipline as a
 *      single branchless AND.
 *
 * Composition (9-bit branchless decision):
 *
 *   v60 σ-Shield      — was the action allowed?
 *   v61 Σ-Citadel     — does the data flow honour BLP+Biba?
 *   v62 Reasoning     — is the EBT energy below budget?
 *   v63 σ-Cipher      — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect   — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex — is the thought on-manifold + under HVL cost?
 *   v66 σ-Silicon     — does the matrix path clear conformal +
 *                       MAC budget?
 *   v67 σ-Noesis      — does the deliberation close (top-1 margin
 *                       ≥ threshold) AND every NBL step write a
 *                       receipt?
 *   v68 σ-Mnemos      — does the recall fidelity ≥ threshold AND is
 *                       the forget budget honoured AND is the
 *                       Hebbian learning-rate ratchet stable?
 *
 * `cos_v68_compose_decision` is a single 9-way branchless AND.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - Every arena `aligned_alloc(64, …)`; the bipolar HV store, the
 *     adapter matrix, and the NBL state are 64-byte aligned at
 *     construction; the hot paths (recall, hebb, decay) never
 *     allocate.
 *   - All arithmetic is Q0.15 integer; no FP on any decision
 *     surface.  ACT-R log-decay is replaced by a saturating linear
 *     decay computed with branchless `sel_i32`; consolidation uses
 *     majority XOR (single popcount per word).
 *   - Recall is `__builtin_popcountll` Hamming over 128 × 64-bit
 *     words per HV — native `cnt + addv` on AArch64; no
 *     data-dependent branch on the bit distribution.
 *   - The Hebbian update is a saturating Q0.15 outer-product over a
 *     fixed `R × C` adapter (default 16 × 16 = 256 cells), capped
 *     by the rate ratchet so the kernel cannot over-write past
 *     saturation.
 *   - The MML interpreter tracks per-instruction memory-unit cost
 *     (SENSE = 1, SURPRISE = 1, STORE = 4, RECALL = 16, HEBB = 8,
 *     CONSOLIDATE = 32, FORGET = 4, CMPGE = 1, GATE = 1) and
 *     refuses on over-budget without a data-dependent branch on the
 *     cost itself.
 *
 * No dependencies beyond libc.  All operations are constant-time in
 * the *episode contents*; they are not constant-time in the *store
 * size* (that is by design — recall scales with the store, not the
 * query).
 *
 * 1 = 1.
 */

#ifndef COS_V68_MNEMOS_H
#define COS_V68_MNEMOS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------
 *  Constants and limits.
 * -------------------------------------------------------------------- */

#define COS_V68_HV_BITS         8192u  /* hyperdimensional bipolar HV */
#define COS_V68_HV_WORDS        (COS_V68_HV_BITS / 64u)  /* = 128 */
#define COS_V68_HV_BYTES        (COS_V68_HV_BITS / 8u)   /* = 1024 */

#define COS_V68_EPISODES_MAX     256u  /* hippocampal ring capacity */
#define COS_V68_TOPK_MAX          32u  /* recall top-K */
#define COS_V68_ADAPT_R           16u  /* Hebbian adapter rows */
#define COS_V68_ADAPT_C           16u  /* Hebbian adapter cols */
#define COS_V68_NREGS             16u  /* MML register file (per kind) */

/* Default thresholds (Q0.15). */
#define COS_V68_SURPRISE_DEFAULT_Q15  ((int16_t)16384)  /* ≥ 0.5 */
#define COS_V68_KEEP_DEFAULT_Q15      ((int16_t)4096)   /* ≥ ~0.125 */
#define COS_V68_RATE_INIT_Q15         ((int16_t)6554)   /* ~0.2 */
#define COS_V68_RATE_FLOOR_Q15        ((int16_t)512)    /* ~0.0156 */

/* Initial activation when an episode is stored (Q0.15). */
#define COS_V68_ACT_INIT_Q15          ((int16_t)32767)

/* Default linear decay per tick (Q0.15). */
#define COS_V68_DECAY_DEFAULT_Q15     ((int16_t)64)

/* --------------------------------------------------------------------
 *  Bipolar HV (8 192 bits) — pattern separation + completion.
 * -------------------------------------------------------------------- */

typedef struct {
    uint64_t w[COS_V68_HV_WORDS];
} cos_v68_hv_t;

/* Hamming distance between two HVs (0 = identical, 8192 = opposite). */
static inline uint32_t cos_v68_hv_hamming(const cos_v68_hv_t *a,
                                          const cos_v68_hv_t *b)
{
    uint32_t h = 0;
    for (uint32_t i = 0; i < COS_V68_HV_WORDS; ++i)
        h += (uint32_t)__builtin_popcountll(a->w[i] ^ b->w[i]);
    return h;
}

/* Q0.15 similarity from Hamming: sim = ((BITS − 2·H) · 32768) / BITS. */
int16_t cos_v68_hv_sim_q15(const cos_v68_hv_t *a, const cos_v68_hv_t *b);

/* In-place XOR bind (bipolar group operator). */
void cos_v68_hv_xor_inplace(cos_v68_hv_t *dst, const cos_v68_hv_t *src);

/* Permutation by 1 bit (sequence-position binding). */
void cos_v68_hv_perm1(cos_v68_hv_t *dst, const cos_v68_hv_t *src);

/* --------------------------------------------------------------------
 *  Episodic store (hippocampal ring buffer).
 * -------------------------------------------------------------------- */

typedef struct {
    cos_v68_hv_t hv;
    int16_t      surprise_q15;   /* novelty at write time */
    int16_t      activation_q15; /* current ACT-R activation */
    uint32_t     timestamp;      /* monotonic tick */
    uint32_t     valid;          /* 1 = live, 0 = forgotten/empty */
} cos_v68_episode_t;

typedef struct cos_v68_store cos_v68_store_t;

cos_v68_store_t *cos_v68_store_new(uint32_t capacity);
void             cos_v68_store_free(cos_v68_store_t *s);
void             cos_v68_store_reset(cos_v68_store_t *s);
uint32_t         cos_v68_store_capacity(const cos_v68_store_t *s);
uint32_t         cos_v68_store_count(const cos_v68_store_t *s);
uint64_t         cos_v68_store_writes(const cos_v68_store_t *s);
uint64_t         cos_v68_store_forgets(const cos_v68_store_t *s);
uint32_t         cos_v68_store_tick(const cos_v68_store_t *s);
const cos_v68_episode_t *cos_v68_store_at(const cos_v68_store_t *s,
                                          uint32_t i);

/* Test/telemetry hook: directly set an episode's activation (used by
 * sleep / forgetting simulators).  Returns 0 on success. */
int cos_v68_store_set_activation(cos_v68_store_t *s,
                                 uint32_t i,
                                 int16_t activation_q15);

/* --------------------------------------------------------------------
 *  Surprise gate (Titans 2025-style).
 *
 *  Returns 1 iff |pred_q15 − obs_q15| ≥ threshold_q15.  Branchless,
 *  constant-time in the magnitudes.
 * -------------------------------------------------------------------- */

uint8_t cos_v68_surprise_gate(int16_t pred_q15,
                              int16_t obs_q15,
                              int16_t threshold_q15,
                              int16_t *out_surprise_q15);

/* Append `hv` with the supplied `surprise_q15`.  No-op (returns 0)
 * iff `surprise_q15 < threshold_q15`; otherwise returns 1 and the
 * episode is written at the next ring slot, possibly overwriting the
 * lowest-activation slot if full.  Always increments the tick. */
uint32_t cos_v68_store_write(cos_v68_store_t *s,
                             const cos_v68_hv_t *hv,
                             int16_t surprise_q15,
                             int16_t threshold_q15);

/* --------------------------------------------------------------------
 *  ACT-R activation decay (branchless integer linear).
 *
 *  A_t = max(0, A_0 − decay_q15 · dt).  Saturates at 0 and at
 *  COS_V68_ACT_INIT_Q15 on the upper bound.
 * -------------------------------------------------------------------- */

int16_t cos_v68_actr_decay_q15(int16_t init_q15,
                               int16_t decay_q15,
                               uint32_t dt);

/* Apply linear decay to every live episode in `s` using the supplied
 * decay rate.  Returns the number of episodes whose activation
 * reached 0 after the decay (for telemetry; no episode is forgotten
 * by this call).  Constant time per episode. */
uint32_t cos_v68_store_decay(cos_v68_store_t *s, int16_t decay_q15);

/* --------------------------------------------------------------------
 *  Recall (pattern completion).
 *
 *  Top-K nearest episodes (by Hamming → Q0.15 similarity) to `q`.
 *  `out` must have capacity ≥ k.  `fidelity_q15` (if non-NULL)
 *  receives the top-1 similarity.  Updates accessed episodes'
 *  activations to ACT_INIT (rehearsal effect).
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t id[COS_V68_TOPK_MAX];
    int16_t  sim_q15[COS_V68_TOPK_MAX];
    uint8_t  k;
    uint8_t  len;
} cos_v68_recall_t;

void cos_v68_recall_init(cos_v68_recall_t *r, uint32_t k);

uint32_t cos_v68_recall(cos_v68_store_t *s,
                        const cos_v68_hv_t *q,
                        cos_v68_recall_t *out,
                        int16_t *fidelity_q15);

/* --------------------------------------------------------------------
 *  Hebbian online adapter (TTT-style).
 *
 *  W is an `R × C` matrix of int16 (Q0.15).  Update:
 *      W[i,j] ← sat( W[i,j] + (eta · pre[i] · post[j]) >> 15 )
 *
 *  `eta_q15` is bounded by an internal ratchet (`rate_q15`) that
 *  never grows; consolidation may reset it back to RATE_INIT.
 * -------------------------------------------------------------------- */

typedef struct cos_v68_adapter cos_v68_adapter_t;

cos_v68_adapter_t *cos_v68_adapter_new(uint32_t r, uint32_t c);
void               cos_v68_adapter_free(cos_v68_adapter_t *a);
void               cos_v68_adapter_reset(cos_v68_adapter_t *a);
uint32_t           cos_v68_adapter_rows(const cos_v68_adapter_t *a);
uint32_t           cos_v68_adapter_cols(const cos_v68_adapter_t *a);
int16_t            cos_v68_adapter_w(const cos_v68_adapter_t *a,
                                     uint32_t i, uint32_t j);
int16_t            cos_v68_adapter_rate_q15(const cos_v68_adapter_t *a);
uint64_t           cos_v68_adapter_updates(const cos_v68_adapter_t *a);

/* Apply one Hebbian update: W += (eta · outer(pre, post)) >> 15.
 * `eta_q15` is clamped to the adapter's rate ratchet.  Branchless,
 * saturating per-cell.  Returns the actual eta used. */
int16_t cos_v68_hebb_update(cos_v68_adapter_t *a,
                            const int16_t *pre,
                            const int16_t *post,
                            int16_t eta_q15);

/* Ratchet the adapter's learning rate down by a Q0.15 factor.
 * `factor_q15` ∈ [0, 32768]; the new rate is
 * `max(RATE_FLOOR, (rate · factor) >> 15)`.  Anti-catastrophic-
 * forgetting discipline. */
void cos_v68_adapter_ratchet(cos_v68_adapter_t *a, int16_t factor_q15);

/* --------------------------------------------------------------------
 *  Sleep consolidation (offline replay).
 *
 *  Bundle every episode whose decayed activation ≥ keep_thresh_q15
 *  into the long-term HV `lt_out` via majority XOR.  Returns the
 *  number of episodes consolidated.  `lt_out` is overwritten.
 *  Also resets the adapter's learning-rate ratchet to RATE_INIT
 *  (the sleep-cycle hand-off).
 * -------------------------------------------------------------------- */

uint32_t cos_v68_consolidate(cos_v68_store_t *s,
                             cos_v68_adapter_t *adapter,
                             int16_t keep_thresh_q15,
                             cos_v68_hv_t *lt_out);

/* --------------------------------------------------------------------
 *  Forgetting controller (branchless prune).
 *
 *  Mark every episode with activation < keep_thresh_q15 as invalid.
 *  `forget_budget` caps the number that may be forgotten in one
 *  call; if more would qualify, only the lowest-activation
 *  `forget_budget` are dropped.  Returns the number forgotten.
 * -------------------------------------------------------------------- */

uint32_t cos_v68_forget(cos_v68_store_t *s,
                        int16_t keep_thresh_q15,
                        uint32_t forget_budget);

/* --------------------------------------------------------------------
 *  MML — Mnemonic Memory Language (10 opcodes).
 *
 *  Cost model (memory units):
 *      HALT        = 1
 *      SENSE       = 1
 *      SURPRISE    = 1
 *      STORE       = 4
 *      RECALL      = 16
 *      HEBB        = 8
 *      CONSOLIDATE = 32
 *      FORGET      = 4
 *      CMPGE       = 1
 *      GATE        = 1
 *
 *  GATE writes `v68_ok = 1` iff:
 *      cost ≤ cost_budget
 *      AND reg_q15[a] ≥ imm
 *      AND forget_count ≤ forget_budget
 *      AND abstained == 0
 * -------------------------------------------------------------------- */

typedef enum {
    COS_V68_OP_HALT        = 0,
    COS_V68_OP_SENSE       = 1,
    COS_V68_OP_SURPRISE    = 2,
    COS_V68_OP_STORE       = 3,
    COS_V68_OP_RECALL      = 4,
    COS_V68_OP_HEBB        = 5,
    COS_V68_OP_CONSOLIDATE = 6,
    COS_V68_OP_FORGET      = 7,
    COS_V68_OP_CMPGE       = 8,
    COS_V68_OP_GATE        = 9,
} cos_v68_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v68_inst_t;

typedef struct cos_v68_mml_state cos_v68_mml_state_t;

cos_v68_mml_state_t *cos_v68_mml_new(void);
void                 cos_v68_mml_free(cos_v68_mml_state_t *s);
void                 cos_v68_mml_reset(cos_v68_mml_state_t *s);

uint64_t cos_v68_mml_cost(const cos_v68_mml_state_t *s);
int16_t  cos_v68_mml_reg_q15(const cos_v68_mml_state_t *s, uint32_t i);
uint32_t cos_v68_mml_reg_ptr(const cos_v68_mml_state_t *s, uint32_t i);
uint32_t cos_v68_mml_forget_count(const cos_v68_mml_state_t *s);
uint8_t  cos_v68_mml_abstained(const cos_v68_mml_state_t *s);
uint8_t  cos_v68_mml_v68_ok(const cos_v68_mml_state_t *s);

typedef struct {
    cos_v68_store_t   *store;
    cos_v68_adapter_t *adapter;
    cos_v68_recall_t  *scratch_recall;  /* caller-owned */
    const cos_v68_hv_t *sense_hv;       /* current observation HV */
    int16_t             pred_q15;        /* SENSE: predicted scalar */
    int16_t             obs_q15;         /* SENSE: observed scalar */
    int16_t             surprise_thresh_q15;
    int16_t             keep_thresh_q15;
    int16_t             decay_q15;
    int16_t            *adapter_pre;     /* COS_V68_ADAPT_R, optional */
    int16_t            *adapter_post;    /* COS_V68_ADAPT_C, optional */
    cos_v68_hv_t       *lt_out;          /* CONSOLIDATE target, optional */
    uint32_t            forget_budget;
} cos_v68_mml_ctx_t;

int cos_v68_mml_exec(cos_v68_mml_state_t *s,
                     const cos_v68_inst_t *prog,
                     uint32_t n,
                     cos_v68_mml_ctx_t *ctx,
                     uint64_t cost_budget);

/* --------------------------------------------------------------------
 *  Composed 9-bit branchless decision.
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
    uint8_t allow;
} cos_v68_decision_t;

cos_v68_decision_t cos_v68_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok);

/* --------------------------------------------------------------------
 *  Version string.
 * -------------------------------------------------------------------- */

extern const char cos_v68_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V68_MNEMOS_H */
