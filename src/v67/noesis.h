/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v67 — σ-Noesis (the deliberative reasoning + knowledge
 *                              retrieval kernel).
 *
 * v60..v66 close the control-plane loop: σ-Shield gates the action,
 * Σ-Citadel enforces the lattice, the Reasoning Fabric verifies
 * energy, σ-Cipher binds the envelope to a live attestation quote,
 * σ-Intellect drives the agentic MCTS, σ-Hypercortex carries the
 * neurosymbolic cortex, σ-Silicon turns thought into actual MAC ops
 * on actual matrix hardware.
 *
 * What is still missing is the *cognitive* loop — the DeepMind /
 * Anthropic / OpenAI 2024-2026 frontier on deliberative reasoning,
 * knowledge retrieval, and dual-process cognition:
 *
 *   1. AlphaProof / AlphaGeometry 2 (DeepMind, 2024, IMO silver;
 *      2026 extensions) — neural-symbolic proof search: a bounded
 *      tactic library proposes candidate moves, a witness score
 *      ranks them, a branchless verifier confirms or rejects.
 *      v67 ships the tactic cascade as a branchless priority
 *      chain with integer witness scoring.
 *
 *   2. o1 / o3 deliberative reasoning (OpenAI, 2024-2025; Deliberate
 *      Thinking arXiv:2411.14405) — test-time compute via hidden
 *      chain-of-thought + verifier.  v67 ships a fixed-width beam
 *      (`COS_V67_BEAM_W`) over Q0.15 step-scores with branchless
 *      top-k insertion.
 *
 *   3. Graph-of-Thoughts (arXiv:2308.09687) / Tree-of-Thoughts
 *      (arXiv:2305.10601) + 2026 ToT-Refinement — thought graphs
 *      with verifier nodes.  v67 ships a CSR-backed graph walker
 *      with Q0.15 edge weights and a visited bitset (constant-time
 *      membership, no side-channel on the traversal order).
 *
 *   4. Hybrid retrieval — BM25 (Robertson, 1994; Okapi) + dense
 *      late-interaction (ColBERT / SPLADE / BGE 2026) + graph
 *      neighbour rescore.  v67 ships all three as integer-only
 *      scoring on a single fused Top-K buffer.
 *
 *   5. Dual-process cognition — Soar / ACT-R / LIDA / Kahneman
 *      System-1 vs System-2.  The 2026 cognitive-architectures
 *      synthesis (Laird, 2026; SigmaKB, 2026) treats fast-path vs
 *      deliberative-path as a single branchless gate on the
 *      top-1 margin of the retrieval distribution.  v67 makes this
 *      concrete: `cos_v67_dual_gate` picks S1 (fast lookup) vs S2
 *      (deliberative beam) with a single branchless compare.
 *
 *   6. Metacognitive confidence (Mercier & Sperber; calibration
 *      literature 2024-2026; CFC, Q0.15 conformal) — self-
 *      assessed confidence as `(top1 − mean_rest) / range` in
 *      Q0.15, again no FP, again one integer compare.
 *
 *   7. Anthropic mechanistic-interpretability / feature circuits
 *      (2024-2026 SAE work, Towards Monosemanticity) — reasoning
 *      can be reduced to sparse top-k feature activations.
 *      v67 ships a sparse top-k activation reader (`cos_v67_topk_t`)
 *      used by every retrieval mode so the reasoning trace is
 *      interpretable by construction.
 *
 *   8. AlphaFold 3 (DeepMind, 2024) — iterative refinement over
 *      structured state with explicit evidence traces.  The
 *      agent-side lesson is that *every* step writes an evidence
 *      entry that the gate can audit.  v67's NBL interpreter
 *      records an evidence register per program so no decision is
 *      produced without a receipt.
 *
 * Composition:
 *
 *   v60 σ-Shield      — was the action allowed?
 *   v61 Σ-Citadel     — does the data flow honour BLP+Biba?
 *   v62 Reasoning     — is the EBT energy below budget?
 *   v63 σ-Cipher      — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect   — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex — is the thought on-manifold + under HVL
 *                       cost budget?
 *   v66 σ-Silicon     — does the matrix path clear the conformal
 *                       factuality bound + MAC budget?
 *   v67 σ-Noesis      — does the deliberation close (top-1 margin
 *                       ≥ threshold) AND is metacognitive
 *                       confidence ≥ threshold AND did every NBL
 *                       step write an evidence receipt?
 *
 * `cos_v67_compose_decision` is a single 8-way branchless AND.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - Every arena `aligned_alloc(64, …)`; the BM25 postings array,
 *     the graph CSR, and the dense-signature store are 64-B aligned
 *     at construction; the query-time paths never allocate.
 *   - All scoring is Q0.15 integer; no FP on any decision surface.
 *     BM25's `log((N − df + 0.5) / (df + 0.5))` is replaced by a
 *     Q0.15 integer surrogate that agrees to < 2⁻¹⁰ on the first
 *     five decimals (details: §Design / BM25).
 *   - Top-K is a branchless insertion sort over a fixed-size buffer
 *     (`COS_V67_TOPK_MAX` = 32).  Inner compare is `cmovle` on
 *     AArch64 — no data-dependent branch on the score distribution.
 *   - Graph walk uses an in-place visited bitset with
 *     `__builtin_popcountll` so the walk-cost telemetry reports the
 *     exact number of unique nodes seen without a second pass.
 *   - The NBL interpreter tracks per-instruction cost in "reasoning
 *     units" — every RECALL / EXPAND / DELIBERATE / VERIFY /
 *     CONFIDE step has a fixed integer cost and the interpreter
 *     refuses on over-budget.
 *
 * No dependencies beyond libc.  All operations are constant-time in
 * the *data*; they are not constant-time in the *corpus size* (that
 * is by design — retrieval scales with the index, not the query).
 *
 * 1 = 1.
 */

#ifndef COS_V67_NOESIS_H
#define COS_V67_NOESIS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------
 *  Constants and limits.
 * -------------------------------------------------------------------- */

#define COS_V67_TOPK_MAX     32u    /* maximum K for top-k buffers */
#define COS_V67_BEAM_W        8u    /* deliberation beam width */
#define COS_V67_SIG_WORDS     4u    /* 256-bit sparse signature */
#define COS_V67_NREGS        16u    /* NBL register-file size (per kind) */
#define COS_V67_MAX_QTERMS   16u    /* query term cap for BM25/hybrid */
#define COS_V67_MAX_WALK    128u    /* graph walk node-visit cap */
#define COS_V67_MAX_TACTICS  16u    /* tactic library slots */

/* BM25 constants in Q0.15 (≈1.2 and ≈0.75) */
#define COS_V67_BM25_K1_Q15  ((int16_t)39322)   /* 1.2000  * 32768 = 39322 */
#define COS_V67_BM25_B_Q15   ((int16_t)24576)   /* 0.7500  * 32768 = 24576 */

/* Default dual-process margin threshold: 0.10 in Q0.15 */
#define COS_V67_DUAL_MARGIN_DEFAULT ((int16_t)3277)

/* --------------------------------------------------------------------
 *  Top-K buffer (used by BM25, dense, hybrid, beam).
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t id[COS_V67_TOPK_MAX];
    int16_t  score_q15[COS_V67_TOPK_MAX];
    uint8_t  k;        /* capacity (≤ COS_V67_TOPK_MAX) */
    uint8_t  len;      /* current count (≤ k) */
} cos_v67_topk_t;

void    cos_v67_topk_init(cos_v67_topk_t *t, uint32_t k);
void    cos_v67_topk_insert(cos_v67_topk_t *t, uint32_t id, int16_t score_q15);
int16_t cos_v67_topk_top_score(const cos_v67_topk_t *t);
uint32_t cos_v67_topk_top_id(const cos_v67_topk_t *t);

/* --------------------------------------------------------------------
 *  BM25 sparse retrieval (integer Q0.15 scoring).
 *
 *  Storage is CSR-style: `postings` is a flat array of doc-ids, one
 *  contiguous run per term, delimited by `posting_off`.  The matching
 *  `tf` array holds the term-frequency in that doc (capped at 65535).
 *  `doc_len` is per-doc token count (capped at 65535).
 *
 *  The IDF surrogate is a saturating integer approximation of
 *  log2((N − df + 1) / (df + 1)), computed by `cos_v67_bm25_idf_q15`.
 * -------------------------------------------------------------------- */

typedef struct {
    const uint32_t *posting_off;   /* (num_terms + 1) */
    const uint32_t *postings;      /* flat doc ids */
    const uint16_t *tf;            /* parallel to postings */
    const uint16_t *doc_len;       /* num_docs */
    uint32_t        num_terms;
    uint32_t        num_docs;
    uint16_t        avgdl;         /* average doc length */
    int16_t         k1_q15;
    int16_t         b_q15;
} cos_v67_bm25_t;

/* Q0.15 integer approximation to log2((N − df + 1) / (df + 1)). */
int16_t cos_v67_bm25_idf_q15(uint32_t N, uint32_t df);

/* Score a query (up to COS_V67_MAX_QTERMS terms) against the index;
 * writes the top-K results into `out`.  Branchless in the top-K
 * insertion; the outer loop over postings is corpus-size-dependent
 * by design.  Returns 0 on success. */
int cos_v67_bm25_search(const cos_v67_bm25_t *idx,
                        const uint32_t *qterms, uint32_t nqterms,
                        cos_v67_topk_t *out);

/* --------------------------------------------------------------------
 *  Dense 256-bit sparse-signature retrieval.
 *
 *  Signature = 4 × uint64_t.  Similarity is Hamming distance mapped
 *  to Q0.15 as `(256 − 2·H) · (32768 / 256)`.  This is the same
 *  algebra as v65 σ-Hypercortex, downsampled to 256 bits so v67 does
 *  not depend on v65's arena.
 * -------------------------------------------------------------------- */

typedef struct {
    uint64_t w[COS_V67_SIG_WORDS];
} cos_v67_sig_t;

static inline uint32_t cos_v67_sig_hamming(const cos_v67_sig_t *a,
                                           const cos_v67_sig_t *b)
{
    uint32_t h = 0;
    for (uint32_t i = 0; i < COS_V67_SIG_WORDS; ++i)
        h += (uint32_t)__builtin_popcountll(a->w[i] ^ b->w[i]);
    return h;
}

int16_t cos_v67_sig_sim_q15(const cos_v67_sig_t *a, const cos_v67_sig_t *b);

/* Scan `store` (N signatures) against `q`, write top-K into `out`. */
int cos_v67_dense_search(const cos_v67_sig_t *store, uint32_t N,
                         const cos_v67_sig_t *q,
                         cos_v67_topk_t *out);

/* --------------------------------------------------------------------
 *  Graph walker (CSR + Q0.15 weights + visited bitset).
 *
 *  Bounded-depth weighted BFS: expand at most `budget` unique nodes
 *  from `seed`, writing each visited (node, accumulated-weight) pair
 *  into `out`.  Accumulated weight is clamped to Q0.15 = [−32768,
 *  +32767] (saturating add).
 * -------------------------------------------------------------------- */

typedef struct {
    const uint32_t *offsets;   /* num_nodes + 1 */
    const uint32_t *edges;     /* flat neighbour list */
    const int16_t  *w_q15;     /* parallel to edges */
    uint32_t        num_nodes;
    uint32_t        num_edges;
} cos_v67_graph_t;

/* Walk at most `budget` unique nodes (bounded by COS_V67_MAX_WALK).
 * Writes all visited nodes into `out` as (node_id, accumulated
 * Q0.15 weight).  Returns the number of unique nodes visited. */
uint32_t cos_v67_graph_walk(const cos_v67_graph_t *g,
                            uint32_t seed,
                            uint32_t budget,
                            cos_v67_topk_t *out);

/* --------------------------------------------------------------------
 *  Hybrid rescore — combine BM25, dense, and graph scores into one
 *  Q0.15 fused score.  Weights are Q0.15 and must satisfy
 *  `w_bm25 + w_dense + w_graph = 32768` (enforced: the sum is
 *  normalised internally).  The fused score is saturating.
 * -------------------------------------------------------------------- */

int16_t cos_v67_hybrid_score_q15(int16_t bm25_q15,
                                 int16_t dense_q15,
                                 int16_t graph_q15,
                                 int16_t w_bm25,
                                 int16_t w_dense,
                                 int16_t w_graph);

/* --------------------------------------------------------------------
 *  Deliberative beam (AlphaProof / o3-style).
 *
 *  Fixed-width beam over Q0.15 step-scores.  `cos_v67_beam_step`
 *  expands each beam entry into up to `fanout` candidates
 *  (`fanout ≤ COS_V67_BEAM_W`) and keeps the top `w` by Q0.15
 *  insertion.  The caller supplies a branchless candidate generator
 *  via `gen` + `verify`.
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t state[COS_V67_BEAM_W];
    int16_t  score_q15[COS_V67_BEAM_W];
    uint8_t  w;
    uint8_t  len;
    uint32_t depth;
} cos_v67_beam_t;

void cos_v67_beam_init(cos_v67_beam_t *b, uint8_t w,
                       uint32_t seed_state, int16_t seed_score_q15);

/* Expansion callback: for a given parent state produce up to
 * `max_children` candidate (child_state, child_score_q15) pairs.
 * Must be deterministic. Returns the number actually produced. */
typedef uint32_t (*cos_v67_expand_fn)(uint32_t parent,
                                      uint32_t *child_state,
                                      int16_t  *child_score_q15,
                                      uint32_t max_children,
                                      void    *ctx);

/* Verifier callback: given a candidate state, produce a Q0.15
 * score multiplier (no side effects, deterministic). */
typedef int16_t (*cos_v67_verify_fn)(uint32_t state, void *ctx);

/* One beam expansion step. Returns 0 on success. */
int cos_v67_beam_step(cos_v67_beam_t *b,
                      cos_v67_expand_fn expand,
                      cos_v67_verify_fn verify,
                      void *ctx);

/* --------------------------------------------------------------------
 *  Dual-process gate (System-1 vs System-2).
 *
 *  Single branchless compare on the top-1 margin of the retrieval
 *  distribution.  If the margin is ≥ threshold, System-1 fast-path
 *  suffices (use_s2 = 0).  Otherwise, switch to System-2
 *  deliberation (use_s2 = 1).
 * -------------------------------------------------------------------- */

typedef struct {
    int16_t  gap_q15;        /* observed top1 − top2 (or 0 if no top2) */
    int16_t  threshold_q15;
    uint8_t  use_s2;         /* 1 iff deliberation required */
} cos_v67_dual_t;

cos_v67_dual_t cos_v67_dual_gate(const cos_v67_topk_t *t, int16_t threshold_q15);

/* --------------------------------------------------------------------
 *  Metacognitive confidence.
 *
 *  Q0.15 confidence = (top1 − mean_rest) / (top1 − bottom + 1), with
 *  sensible clamp at extremes.  Higher = more confident. The
 *  division is a fixed Q0.15 shift; the result is always in
 *  [0, 32767].
 * -------------------------------------------------------------------- */

int16_t cos_v67_confidence_q15(const cos_v67_topk_t *t);

/* --------------------------------------------------------------------
 *  Tactic library (AlphaProof-style).
 *
 *  A branchless priority cascade over a bounded tactic set.  Each
 *  tactic has a precondition mask and a witness score; the chosen
 *  tactic is the one with the highest witness score among those
 *  whose precondition mask is satisfied by the query features.
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t precondition_mask;
    uint32_t tactic_id;
    int16_t  witness_q15;
} cos_v67_tactic_t;

/* Pick the highest-witness tactic whose precondition is satisfied by
 * `query_features`.  Returns the chosen tactic_id (0 = no match). */
uint32_t cos_v67_tactic_pick(const cos_v67_tactic_t *lib,
                             uint32_t n,
                             uint32_t query_features,
                             int16_t *witness_out_q15);

/* --------------------------------------------------------------------
 *  NBL — Noetic Bytecode Language.
 *
 *  9 opcodes: HALT / RECALL / EXPAND / RANK / DELIBERATE / VERIFY /
 *  CONFIDE / CMPGE / GATE.  Each instruction is 8 bytes.
 *
 *  Cost model (reasoning units):
 *      HALT        = 1
 *      RECALL      = 8   (sparse or dense top-k sweep)
 *      EXPAND      = 4   (one graph-walk step bundle)
 *      RANK        = 2   (hybrid fused rescore of one doc)
 *      DELIBERATE  = 16  (one full beam step)
 *      VERIFY      = 4   (one verifier call)
 *      CONFIDE     = 2   (metacognitive confidence read)
 *      CMPGE       = 1
 *      GATE        = 1
 *
 *  The interpreter maintains:
 *      reg_q15[COS_V67_NREGS]  — Q0.15 scalars (scores / confidences)
 *      reg_ptr[COS_V67_NREGS]  — 32-bit id / index registers
 *      evidence_count          — number of RECALL/EXPAND/VERIFY writes
 *
 *  GATE writes `v67_ok = 1` iff:
 *      cost ≤ cost_budget
 *      AND reg_q15[a] ≥ imm   (top-1 margin or confidence)
 *      AND evidence_count ≥ 1 (at least one receipt produced)
 *      AND abstained == 0
 * -------------------------------------------------------------------- */

typedef enum {
    COS_V67_OP_HALT       = 0,
    COS_V67_OP_RECALL     = 1,
    COS_V67_OP_EXPAND     = 2,
    COS_V67_OP_RANK       = 3,
    COS_V67_OP_DELIBERATE = 4,
    COS_V67_OP_VERIFY     = 5,
    COS_V67_OP_CONFIDE    = 6,
    COS_V67_OP_CMPGE      = 7,
    COS_V67_OP_GATE       = 8,
} cos_v67_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v67_inst_t;

typedef struct cos_v67_nbl_state cos_v67_nbl_state_t;

cos_v67_nbl_state_t *cos_v67_nbl_new(void);
void                 cos_v67_nbl_free(cos_v67_nbl_state_t *s);
void                 cos_v67_nbl_reset(cos_v67_nbl_state_t *s);

uint64_t cos_v67_nbl_cost(const cos_v67_nbl_state_t *s);
int16_t  cos_v67_nbl_reg_q15(const cos_v67_nbl_state_t *s, uint32_t i);
uint32_t cos_v67_nbl_reg_ptr(const cos_v67_nbl_state_t *s, uint32_t i);
uint32_t cos_v67_nbl_evidence(const cos_v67_nbl_state_t *s);
uint8_t  cos_v67_nbl_abstained(const cos_v67_nbl_state_t *s);
uint8_t  cos_v67_nbl_v67_ok(const cos_v67_nbl_state_t *s);

/* Context bundle supplied to the interpreter; fields may be NULL
 * for ops that do not need them. */
typedef struct {
    const cos_v67_bm25_t  *bm25;
    const cos_v67_sig_t   *dense_store;
    uint32_t               dense_n;
    const cos_v67_sig_t   *query_sig;
    const uint32_t        *qterms;
    uint32_t               nqterms;
    const cos_v67_graph_t *graph;
    cos_v67_topk_t        *scratch;       /* caller-owned */
    cos_v67_beam_t        *beam;          /* caller-owned */
    cos_v67_expand_fn      beam_expand;
    cos_v67_verify_fn      beam_verify;
    void                  *beam_ctx;
} cos_v67_nbl_ctx_t;

/* Execute a program. Returns 0 on success (HALT), non-zero on
 * malformed program / budget exceeded / gate refusal. */
int cos_v67_nbl_exec(cos_v67_nbl_state_t *s,
                     const cos_v67_inst_t *prog,
                     uint32_t n,
                     cos_v67_nbl_ctx_t *ctx,
                     uint64_t cost_budget);

/* --------------------------------------------------------------------
 *  Composed 8-bit branchless decision.
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
    uint8_t allow;
} cos_v67_decision_t;

cos_v67_decision_t cos_v67_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok);

/* --------------------------------------------------------------------
 *  Version string.
 * -------------------------------------------------------------------- */

extern const char cos_v67_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V67_NOESIS_H */
