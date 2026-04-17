/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v69 — σ-Constellation (the distributed-orchestration +
 *                                    parallel-decoding + multi-agent
 *                                    consensus kernel).
 *
 * v60..v68 close the *single-node* deliberation + memory loop:
 * σ-Shield gates the action, Σ-Citadel enforces the lattice, the
 * Reasoning Fabric verifies energy, σ-Cipher binds the envelope,
 * σ-Intellect drives MCTS, σ-Hypercortex carries the cortex,
 * σ-Silicon turns thought into MAC ops, σ-Noesis runs the
 * deliberative beam with evidence receipts, σ-Mnemos remembers and
 * evolves across calls.
 *
 * What is still missing is *scale*: a system that coordinates many
 * inference paths, many experts, many agents, many nodes — and does
 * so with a formal, integer, branchless floor that surpasses the
 * ad-hoc infrastructure of Microsoft (MAI orchestration), Anthropic
 * (Claude cluster), and OpenAI (Stargate pipeline).  v69 σ-Constellation
 * ships the orchestration plane as a single dependency-free,
 * branchless, integer-only C kernel:
 *
 *   1. Tree speculative decoding (EAGLE-3; Li et al., 2024-2026;
 *      Hierarchical Speculative Decoding, arXiv:2601.05724) —
 *      `cos_v69_draft_tree_verify` accepts the longest branchless
 *      integer prefix over a depth-limited draft tree; verification
 *      is a single XOR match mask per level, `__builtin_popcountll`
 *      for the acceptance count, and a single compare against the
 *      per-level acceptance budget.
 *
 *   2. Multi-agent debate with score-based consensus (Council Mode,
 *      arXiv:2604.02923v1; FREE-MAD, 2026) — N agents produce
 *      Q0.15-scored outputs; `cos_v69_debate_score` aggregates them
 *      with an anti-conformity bonus that prevents majority collapse
 *      and an abstain-if-disagreement default (the FREE-MAD
 *      safety-first variant).  Branchless top-K insertion.
 *
 *   3. Byzantine-safe integer vote (PBFT / HotStuff; 2f+1 quorum) —
 *      `cos_v69_byzantine_vote` counts matching bits over N
 *      participants; the gate compares against a `2f+1` quorum with
 *      one branchless compare.  No data-dependent branch on which
 *      participants are faulty.
 *
 *   4. MoE top-K routing (MaxScore routing, arXiv:2508.12801) —
 *      `cos_v69_route_topk` scores N experts in Q0.15, selects the
 *      top-K via branchless bubble, and accumulates an integer
 *      load-balance counter per expert.  No softmax, no FP.
 *
 *   5. Draft tree expansion / prune — depth-limited integer tree
 *      with per-node acceptance probability Q0.15 and a branchless
 *      cumulative-cost compare.  Tree layout is flat (parent-index
 *      array) so cache behaviour is predictable.
 *
 *   6. Gossip / vector-clock causal ordering (Lamport 1978; Fidge
 *      1988; 2026 federated learning over gossip) —
 *      `cos_v69_gossip_merge` computes the element-wise max of two
 *      integer vector clocks; no branch on which node is ahead.
 *
 *   7. Chunked flash-style attention score — `cos_v69_chunked_dot`
 *      computes an integer Q0.15 chunked inner product with an
 *      online integer max-tracker (softmax-free integer normalisation).
 *      O(N) memory, constant time per chunk.
 *
 *   8. Self-play Elo with UCB selection (AlphaZero lineage, 2025-2026
 *      open-ended self-play) — `cos_v69_elo_update` applies a
 *      saturating Q0.15 Elo update with a fixed K-factor;
 *      `cos_v69_ucb_select` picks the arm with the highest branchless
 *      integer UCB score (mean + exploration bonus).
 *
 *   9. KV-cache deduplication via popcount — `cos_v69_dedup_insert`
 *      hashes a key signature into a bipolar 512-bit sketch and
 *      collapses neighbours under a Hamming-distance threshold
 *      (reuses the v68 popcount discipline).  Dedup is branchless
 *      and saturating.
 *
 *  10. CL — Constellation Language — 10-opcode integer bytecode ISA:
 *      `HALT / DRAFT / VERIFY / DEBATE / VOTE / ROUTE / GOSSIP /
 *       ELO / DEDUP / GATE`.  Each instruction is 8 bytes.  The
 *      interpreter tracks per-instruction orchestration-unit cost;
 *      `GATE` writes `v69_ok = 1` iff:
 *          orch_cost          ≤ cost_budget
 *          AND reg_q15[a]     ≥ imm                 (vote margin)
 *          AND byz_fail_count ≤ byzantine_budget     (no over-fault)
 *          AND abstained      == 0
 *      So no orchestration program produces `v69_ok = 1` without
 *      simultaneously demonstrating quorum margin ≥ threshold and
 *      Byzantine faults ≤ budget — PBFT discipline as a single
 *      branchless AND.
 *
 * Composition (10-bit branchless decision):
 *
 *   v60 σ-Shield      — was the action allowed?
 *   v61 Σ-Citadel     — does the data flow honour BLP + Biba?
 *   v62 Reasoning     — is the EBT energy below budget?
 *   v63 σ-Cipher      — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect   — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex — is the thought on-manifold + under HVL cost?
 *   v66 σ-Silicon     — does the matrix path clear conformal +
 *                       MAC budget?
 *   v67 σ-Noesis      — does the deliberation close (top-1 margin
 *                       ≥ threshold) AND every NBL step write a
 *                       receipt?
 *   v68 σ-Mnemos      — does recall ≥ threshold AND forget ≤ budget?
 *   v69 σ-Constellation — does the multi-agent quorum margin ≥
 *                       threshold AND Byzantine faults ≤ budget AND
 *                       the speculative tree verify?
 *
 * `cos_v69_compose_decision` is a single 10-way branchless AND.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - Every arena `aligned_alloc(64, …)`; the draft tree, the vote
 *     grid, the dedup sketch, the Elo rating table, and the CL
 *     state are 64-byte aligned at construction; hot paths (verify,
 *     vote, route, dedup) never allocate.
 *   - All arithmetic is Q0.15 integer; no FP on any decision
 *     surface.  No softmax anywhere (the flash-style normaliser is
 *     a pure integer max-tracker).  No division on the hot path.
 *   - Byzantine vote is `__builtin_popcount` over N ≤ 64 lanes —
 *     native `cnt + addv` on AArch64; no branch on which lane is
 *     faulty.  Scales to N > 64 via a simple word loop.
 *   - The CL interpreter tracks per-instruction orchestration-unit
 *     cost (HALT = 1, DRAFT = 4, VERIFY = 8, DEBATE = 16, VOTE = 4,
 *     ROUTE = 4, GOSSIP = 2, ELO = 1, DEDUP = 2, GATE = 1) and
 *     refuses on over-budget without a data-dependent branch on the
 *     cost itself.
 *
 * No dependencies beyond libc.  All operations are constant-time in
 * the *payload contents*; they are not constant-time in the *fleet
 * size* (that is by design — scaling with N is the whole point).
 *
 * 1 = 1.
 */

#ifndef COS_V69_CONSTELLATION_H
#define COS_V69_CONSTELLATION_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------
 *  Constants and limits.
 * -------------------------------------------------------------------- */

#define COS_V69_AGENTS_MAX        64u   /* debate/BFT fleet size */
#define COS_V69_EXPERTS_MAX       64u   /* MoE expert count      */
#define COS_V69_TOPK_MAX          16u   /* route / debate top-K  */
#define COS_V69_TREE_MAX         256u   /* speculative draft tree nodes */
#define COS_V69_TREE_DEPTH_MAX    16u   /* speculative tree depth       */
#define COS_V69_CLOCK_MAX         32u   /* vector clock width (nodes)   */
#define COS_V69_DEDUP_BITS       512u   /* bipolar dedup sketch bits    */
#define COS_V69_DEDUP_WORDS       (COS_V69_DEDUP_BITS / 64u)  /* = 8 */
#define COS_V69_DEDUP_BYTES       (COS_V69_DEDUP_BITS / 8u)   /* = 64 */
#define COS_V69_DEDUP_MAX         64u   /* dedup table entries */
#define COS_V69_ARMS_MAX          64u   /* self-play arm count */
#define COS_V69_NREGS             16u   /* CL register file (per kind) */

/* Default thresholds (Q0.15). */
#define COS_V69_VOTE_MARGIN_DEFAULT_Q15  ((int16_t)6554)  /* ~0.20 margin */
#define COS_V69_ACCEPT_DEFAULT_Q15       ((int16_t)16384) /* ≥ 0.5 accept */
#define COS_V69_ELO_INIT_Q15             ((int16_t)16384) /* ~0.5 winrate */
#define COS_V69_ELO_K_Q15                ((int16_t)1024)  /* ~0.031 K     */
#define COS_V69_UCB_C_Q15                ((int16_t)8192)  /* ~0.25 explore */
#define COS_V69_DEDUP_HAMMING_DEFAULT     64u  /* collapse if < 64 bits */

/* --------------------------------------------------------------------
 *  1. Speculative tree decoding (EAGLE-3 / HSD inspired).
 *
 *  A draft tree is a flat array of nodes; each node has a parent index
 *  (0 for root, parent < self for children), an integer token id, and
 *  a Q0.15 acceptance probability.  Verification is a single XOR match
 *  per node against the target's realised prefix; popcount gives the
 *  accepted-length for free.
 * -------------------------------------------------------------------- */

typedef struct {
    uint16_t parent;   /* parent index; self for root */
    uint16_t depth;    /* 0 for root */
    int32_t  token;    /* integer token id (any range) */
    int16_t  accept_q15; /* per-node acceptance Q0.15 */
    int16_t  pad;
} cos_v69_tree_node_t;

typedef struct {
    cos_v69_tree_node_t nodes[COS_V69_TREE_MAX];
    uint32_t            n;
    uint32_t            root;      /* root index (usually 0) */
    uint16_t            depth_max; /* achieved depth */
    uint16_t            pad;
} cos_v69_draft_tree_t;

/* Reset tree to 0 nodes. */
void cos_v69_tree_init(cos_v69_draft_tree_t *t);

/* Append a node; returns its index, or UINT32_MAX on overflow. */
uint32_t cos_v69_tree_push(cos_v69_draft_tree_t *t,
                           uint32_t parent,
                           int32_t token,
                           int16_t accept_q15);

/* Verify a draft tree against the realised `target_tokens[0..n)`;
 * returns the length of the longest accepted branch (number of tokens
 * accepted, including the root).  Also records the chosen branch in
 * `accepted_path[0..accepted_len)` if `accepted_path` is non-NULL.
 * Branchless per-node compare; accept iff
 *     node.token == target_tokens[node.depth] AND
 *     node.accept_q15 >= min_accept_q15.
 * Longest path selection uses branchless `sel`. */
uint32_t cos_v69_draft_tree_verify(const cos_v69_draft_tree_t *t,
                                   const int32_t *target_tokens,
                                   uint32_t target_len,
                                   int16_t min_accept_q15,
                                   uint32_t *accepted_path);

/* --------------------------------------------------------------------
 *  2. Multi-agent debate (Council Mode + FREE-MAD safety default).
 *
 *  Each agent emits an integer proposal id and a Q0.15 confidence
 *  score.  Aggregation computes per-proposal score sums with an
 *  anti-conformity bonus (Council Mode 2026: penalise proposals that
 *  everyone agrees on without evidence) and returns the top-K
 *  proposals by adjusted score.  FREE-MAD default: if no proposal
 *  achieves `min_margin_q15` margin over second place, the verdict
 *  abstains.
 * -------------------------------------------------------------------- */

typedef struct {
    int32_t proposal;       /* integer id (any range)            */
    int16_t confidence_q15; /* per-agent confidence Q0.15        */
    int16_t pad;
} cos_v69_agent_vote_t;

typedef struct {
    int32_t proposal;
    int32_t score_q15;    /* aggregated Q0.15 score (may exceed 32767 pre-clip) */
    uint16_t n_votes;
    uint16_t pad;
} cos_v69_debate_rank_t;

typedef struct {
    cos_v69_debate_rank_t rank[COS_V69_TOPK_MAX];
    uint8_t  len;
    uint8_t  k;
    uint8_t  abstained;   /* 1 if margin < min_margin_q15 */
    int16_t  margin_q15;  /* top-1 − top-2 Q0.15 */
} cos_v69_debate_result_t;

/* Aggregate `n` agent votes; writes top-`k` proposals (by Q0.15 sum)
 * into `out->rank`.  Sets `abstained = 1` if fewer than two proposals
 * exist or the top-1 margin is below `min_margin_q15`.  Branchless
 * top-K insertion. */
void cos_v69_debate_score(const cos_v69_agent_vote_t *votes,
                          uint32_t n,
                          uint32_t k,
                          int16_t min_margin_q15,
                          int16_t anti_conformity_q15,
                          cos_v69_debate_result_t *out);

/* --------------------------------------------------------------------
 *  3. Byzantine-safe vote (2f+1 quorum).
 *
 *  `votes[i]` is the agent i's binary vote (0 or 1).  Returns the
 *  matching count (popcount-style, any N).  Gate is:
 *      allow = (count ≥ 2*f + 1)
 *  which holds iff at most f of the N = 3f+1 agents are Byzantine.
 *  Branchless; constant-time in the agent identities.
 * -------------------------------------------------------------------- */

uint32_t cos_v69_byzantine_vote(const uint8_t *votes,
                                uint32_t n,
                                uint32_t f,
                                uint8_t  *allow_out);

/* --------------------------------------------------------------------
 *  4. MoE top-K routing (MaxScore-style).
 *
 *  Given `n` expert Q0.15 scores, picks the top-`k` experts (indices
 *  into the score array).  Updates the per-expert load-balance counter
 *  so downstream calls can shift the scores in a balanced direction
 *  (the counter is returned; the caller owns any actual shift).
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t expert[COS_V69_TOPK_MAX];
    int16_t  score_q15[COS_V69_TOPK_MAX];
    uint8_t  k;
    uint8_t  len;
} cos_v69_route_t;

void cos_v69_route_init(cos_v69_route_t *r, uint32_t k);
void cos_v69_route_topk(const int16_t *scores_q15,
                        uint32_t n,
                        cos_v69_route_t *out,
                        uint32_t *load_counter); /* size n, optional */

/* --------------------------------------------------------------------
 *  5. Tree expansion / prune (helper on top of the tree in (1)).
 *
 *  Prune any node whose cumulative Q0.15 path acceptance falls below
 *  `min_path_q15`.  Pruning is physical (the node's `accept_q15` is
 *  set to 0 and the node is marked with depth = 0xFFFF); no memory
 *  shuffling.  Returns the number of pruned nodes.
 * -------------------------------------------------------------------- */

uint32_t cos_v69_tree_prune(cos_v69_draft_tree_t *t, int16_t min_path_q15);

/* --------------------------------------------------------------------
 *  6. Gossip / vector clock.
 *
 *  `cos_v69_clock_tick(c, i)` bumps the local component for node `i`;
 *  `cos_v69_clock_merge(dst, src)` takes the element-wise max.  All
 *  branchless sel via `(a ^ ((a ^ b) & -(a < b)))`.
 * -------------------------------------------------------------------- */

typedef struct {
    uint32_t c[COS_V69_CLOCK_MAX];
    uint32_t width;   /* number of nodes */
} cos_v69_clock_t;

void cos_v69_clock_init(cos_v69_clock_t *c, uint32_t width);
void cos_v69_clock_tick(cos_v69_clock_t *c, uint32_t node);
void cos_v69_clock_merge(cos_v69_clock_t *dst, const cos_v69_clock_t *src);

/* Returns 1 iff `a` strictly happened-before `b` (a ≤ b component-wise
 * AND a ≠ b). */
uint8_t cos_v69_clock_happens_before(const cos_v69_clock_t *a,
                                     const cos_v69_clock_t *b);

/* --------------------------------------------------------------------
 *  7. Chunked flash-style attention score (softmax-free integer).
 *
 *  Computes the Q0.15 inner product `sum_i q[i] * k[i] >> 15` in
 *  `chunk` elements at a time while tracking the running integer max
 *  (online-softmax surrogate).  Returns the saturated Q0.15 score;
 *  `out_max_q15` (if non-NULL) receives the running max.
 * -------------------------------------------------------------------- */

int16_t cos_v69_chunked_dot(const int16_t *q,
                            const int16_t *k,
                            uint32_t n,
                            uint32_t chunk,
                            int16_t *out_max_q15);

/* --------------------------------------------------------------------
 *  8. Self-play Elo + UCB arm selection (AlphaZero lineage).
 *
 *  Elo is Q0.15 ∈ [0, 1] (scaled winrate).  Update:
 *      elo' = sat( elo + K * (outcome − elo) )  (outcome ∈ {0, 1})
 *  UCB: arm with the highest `mean + c * sqrt( N / (1 + n_arm) )` —
 *  implemented with an integer surrogate (`mean + c * N / (1 + n_arm)`)
 *  to avoid FP; the caller supplies `c_q15`.  Branchless top-1.
 * -------------------------------------------------------------------- */

typedef struct {
    int16_t  elo_q15;
    uint32_t pulls;
} cos_v69_arm_t;

typedef struct {
    cos_v69_arm_t arms[COS_V69_ARMS_MAX];
    uint32_t      n;
    uint64_t      total_pulls;
} cos_v69_bandit_t;

void     cos_v69_bandit_init(cos_v69_bandit_t *b, uint32_t n);
void     cos_v69_elo_update(cos_v69_bandit_t *b,
                            uint32_t arm,
                            uint8_t outcome,
                            int16_t k_q15);
uint32_t cos_v69_ucb_select(const cos_v69_bandit_t *b, int16_t c_q15);

/* --------------------------------------------------------------------
 *  9. KV-cache deduplication (bipolar popcount sketch).
 *
 *  Each entry is a 512-bit bipolar sketch.  `cos_v69_dedup_insert`
 *  inserts a sketch and collapses any existing entry whose Hamming
 *  distance is below `hamming_thresh`; the older entry wins (lower
 *  index).  Returns the slot chosen (existing collapsed or new).
 * -------------------------------------------------------------------- */

typedef struct {
    uint64_t w[COS_V69_DEDUP_WORDS];
} cos_v69_sketch_t;

typedef struct {
    cos_v69_sketch_t sk[COS_V69_DEDUP_MAX];
    uint8_t          valid[COS_V69_DEDUP_MAX];
    uint32_t         len;
    uint32_t         collapses;
} cos_v69_dedup_t;

void     cos_v69_dedup_init(cos_v69_dedup_t *d);
uint32_t cos_v69_dedup_insert(cos_v69_dedup_t *d,
                              const cos_v69_sketch_t *sk,
                              uint32_t hamming_thresh,
                              uint8_t *collapsed_out);
uint32_t cos_v69_dedup_count(const cos_v69_dedup_t *d);
uint32_t cos_v69_dedup_collapses(const cos_v69_dedup_t *d);

/* Convenience: Hamming distance between two sketches (0..512). */
static inline uint32_t cos_v69_sketch_hamming(const cos_v69_sketch_t *a,
                                              const cos_v69_sketch_t *b)
{
    uint32_t h = 0;
    for (uint32_t i = 0; i < COS_V69_DEDUP_WORDS; ++i)
        h += (uint32_t)__builtin_popcountll(a->w[i] ^ b->w[i]);
    return h;
}

/* --------------------------------------------------------------------
 * 10. CL — Constellation Language (10-opcode integer ISA).
 *
 *  Cost model (orchestration units):
 *      HALT    = 1
 *      DRAFT   = 4
 *      VERIFY  = 8
 *      DEBATE  = 16
 *      VOTE    = 4
 *      ROUTE   = 4
 *      GOSSIP  = 2
 *      ELO     = 1
 *      DEDUP   = 2
 *      GATE    = 1
 *
 *  GATE writes `v69_ok = 1` iff:
 *      cost          ≤ cost_budget
 *      AND reg_q15[a] ≥ imm              (vote margin)
 *      AND byz_fail  ≤ byzantine_budget   (no over-fault)
 *      AND abstained == 0
 * -------------------------------------------------------------------- */

typedef enum {
    COS_V69_OP_HALT   = 0,
    COS_V69_OP_DRAFT  = 1,
    COS_V69_OP_VERIFY = 2,
    COS_V69_OP_DEBATE = 3,
    COS_V69_OP_VOTE   = 4,
    COS_V69_OP_ROUTE  = 5,
    COS_V69_OP_GOSSIP = 6,
    COS_V69_OP_ELO    = 7,
    COS_V69_OP_DEDUP  = 8,
    COS_V69_OP_GATE   = 9,
} cos_v69_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v69_inst_t;

typedef struct cos_v69_cl_state cos_v69_cl_state_t;

cos_v69_cl_state_t *cos_v69_cl_new(void);
void                cos_v69_cl_free(cos_v69_cl_state_t *s);
void                cos_v69_cl_reset(cos_v69_cl_state_t *s);

uint64_t cos_v69_cl_cost(const cos_v69_cl_state_t *s);
int16_t  cos_v69_cl_reg_q15(const cos_v69_cl_state_t *s, uint32_t i);
uint32_t cos_v69_cl_reg_ptr(const cos_v69_cl_state_t *s, uint32_t i);
uint32_t cos_v69_cl_byz_fail(const cos_v69_cl_state_t *s);
uint8_t  cos_v69_cl_abstained(const cos_v69_cl_state_t *s);
uint8_t  cos_v69_cl_v69_ok(const cos_v69_cl_state_t *s);

typedef struct {
    cos_v69_draft_tree_t    *tree;            /* DRAFT / VERIFY */
    const int32_t           *target_tokens;
    uint32_t                 target_len;
    int16_t                  min_accept_q15;

    const cos_v69_agent_vote_t *agent_votes; /* DEBATE */
    uint32_t                    n_agent_votes;
    int16_t                     min_margin_q15;
    int16_t                     anti_conformity_q15;
    cos_v69_debate_result_t    *debate_out;

    const uint8_t              *byz_votes;    /* VOTE */
    uint32_t                    n_byz_votes;
    uint32_t                    byzantine_budget;

    const int16_t              *expert_scores;/* ROUTE */
    uint32_t                    n_experts;
    cos_v69_route_t            *route_out;
    uint32_t                   *load_counter; /* size n_experts, optional */

    cos_v69_clock_t            *clock;        /* GOSSIP */
    const cos_v69_clock_t      *peer_clock;
    uint32_t                    clock_node;

    cos_v69_bandit_t           *bandit;       /* ELO */
    int16_t                     elo_k_q15;
    int16_t                     ucb_c_q15;

    cos_v69_dedup_t            *dedup;        /* DEDUP */
    const cos_v69_sketch_t     *dedup_sk;
    uint32_t                    dedup_thresh;
} cos_v69_cl_ctx_t;

int cos_v69_cl_exec(cos_v69_cl_state_t *s,
                    const cos_v69_inst_t *prog,
                    uint32_t n,
                    cos_v69_cl_ctx_t *ctx,
                    uint64_t cost_budget);

/* --------------------------------------------------------------------
 *  Composed 10-bit branchless decision.
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
    uint8_t allow;
} cos_v69_decision_t;

cos_v69_decision_t cos_v69_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok);

/* --------------------------------------------------------------------
 *  Version string.
 * -------------------------------------------------------------------- */

extern const char cos_v69_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V69_CONSTELLATION_H */
