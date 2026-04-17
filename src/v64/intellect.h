/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v64 — σ-Intellect (agentic AGI kernel).
 *
 * The 2026 arXiv / Zenodo / OpenReview frontier converged on six
 * independent findings that, taken together, give an agent an
 * unfair advantage against any plain transformer LLM on its own
 * home turf — reasoning, tool use, coding, speed, efficiency, and
 * continuous learning:
 *
 *   1. MCTS-for-LLMs (Empirical-MCTS, arXiv:2602.04248; rStar-Math;
 *      Nemotron-MCTS 2025) — Monte-Carlo tree search with LLM-
 *      generated candidates and an energy / process-reward prior
 *      beats chain-of-thought single-shot on AIME25, ARC-AGI-2, and
 *      MathArena Apex.  The search turns a stateless oracle into a
 *      stateful planner.
 *
 *   2. Skill libraries (EvoSkill, arXiv:2603.02766; Voyager, 2023) —
 *      an agent that persists *compositional, reusable skills*
 *      improves monotonically.  +7.3 % OfficeQA, +12.1 % SealQA,
 *      +5.3 % zero-shot transfer without a single extra gradient
 *      step.
 *
 *   3. Verified tool use (Dynamic ReAct, arXiv:2509.20386; SmolAgents
 *      2026) — a typed, precondition-checked, σ-gated tool interface
 *      is what unlocks hundreds-of-tools environments without the
 *      context window collapsing.  Schema-typed, TOCTOU-free
 *      argument binding makes ToolWorm / RCE-via-prompt attacks
 *      impossible by construction.
 *
 *   4. Reflexion / experiential reflective learning (ERL,
 *      arXiv:2603.24639; ReflexiCoder, arXiv:2603.05863) — turn every
 *      failure into a transferable heuristic and inject it on next
 *      attempt.  +7.8 % Gaia2 over ReAct baseline without any model
 *      update.  This is the kernel's "continuous learning from the
 *      web" line — the heuristics are the delta.
 *
 *   5. AlphaEvolve / EvoX (DeepMind 2025; arXiv EvoX 2026) —
 *      evolutionary code / algorithm search where each mutation is
 *      accepted iff it strictly lowers an evaluated cost.  Pairs
 *      one-to-one with σ: accept iff σ decreases.  Beats human
 *      baselines on game-theory solvers and ~200 optimisation tasks.
 *
 *   6. Mixture-of-Depths (MoD, arXiv:2404.02258; MoDA 2026
 *      arXiv:2603.15619; A-MoD arXiv:2412.20875) — per-token routing
 *      skips unnecessary transformer layers.  ε-dominated tokens
 *      (easy, certain) take the shallow path; α-dominated tokens
 *      (hard, ambiguous) take the full depth.  97.3 % of FlashAttn-2
 *      efficiency at full quality.  This is the kernel's "maailman
 *      nopein" line — spend compute only where the answer is not
 *      already obvious.
 *
 * The composition: an agent that plans (MCTS-σ), remembers skills
 * (skill library), uses tools safely (verified tool-use), learns
 * from mistakes (Reflexion ratchet), evolves code when it stalls
 * (AlphaEvolve-σ), and routes per-token compute by σ (MoD-σ).
 * Every subsystem is a branchless C kernel on the hot path, every
 * decision is σ-gated, every emission passes a 5-bit composed
 * decision with v60 σ-Shield + v61 Σ-Citadel + v62 Reasoning Fabric
 * + v63 σ-Cipher.
 *
 * Hardware discipline (`AGENTS.md`, `.cursorrules`, M4 invariants):
 *
 *   - All buffers `aligned_alloc(64, ...)`.  Never `malloc` on the
 *     hot path.  64-byte alignment = one M4 cache line per NEON load.
 *   - Prefetch the next iteration (`__builtin_prefetch(p+64, 0, 3)`).
 *   - Branchless hot paths: select via 0/1 masks, never `if` in the
 *     inner loop.  The CPU does not speculate branches that do not
 *     exist.
 *   - Four NEON accumulators in parallel so M4's wide decode stays
 *     full.
 *   - Priority order (10. PRIORITY ORDER, .cursorrules):
 *       L0  skill-library lookup           → done (cheap)
 *       L1  MCTS-σ cached node              → done
 *       L2  EBT-verified MCTS expansion     → done
 *       L3  full transformer forward pass   → only then.
 *
 * Composition with upstream kernels: no reasoning emission leaves
 * v64 unless
 *   allow = v60_ok & v61_ok & v62_ok & v63_ok & v64_ok
 * where v64_ok =
 *     (tool_call_authorised)
 *   & (mcts_best_child.sigma ≤ threshold)
 *   & (skill_match.confidence ≥ floor)
 *   & (reflexion_delta ≥ 0  (monotone σ improvement))
 *   & (mod_depth_within_budget).
 *
 * Tier semantics (v57 dialect):
 *   M  this kernel runs and is checked at runtime by `make check-v64`.
 *   I  contracts that depend on external weights (LLM adapters) are
 *      documented and exercised on synthetic inputs only.
 *   P  hardware paths (Apple Neural Engine fast path, SME tile ops)
 *      are planned and compile-only.
 *
 * v64 ships M-tier for all six subsystems and the 5-bit composition.
 * No silent downgrades.  Absent optional toolchains (liboqs, libsodium,
 * Core ML) continue to report SKIP honestly in the upstream slots;
 * v64 itself has zero optional dependencies on its hot path.
 */

#ifndef COS_V64_INTELLECT_H
#define COS_V64_INTELLECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Shared types — one vocabulary, six subsystems.
 * ==================================================================== */

/*  cos_v64_sigma_t — σ decomposed into epistemic (ε, reducible) and
 *  aleatoric (α, irreducible) components.  This matches v59 σ-Budget
 *  and v60 σ-Shield; the two lanes are preserved so downstream code
 *  can decide "expand compute" (high ε) vs "abstain / ask human"
 *  (high α).  Stored as fixed-point Q0.15 so the hot path is integer-
 *  arithmetic only — no FP on the routing decision.
 */
typedef struct {
    uint16_t eps;              /* epistemic   in [0, 32768] = Q0.15  */
    uint16_t alp;              /* aleatoric   in [0, 32768] = Q0.15  */
} cos_v64_sigma_t;

/*  cos_v64_decision_t — 5-bit composed gate verdict.
 *
 *  Five lanes composed branchlessly:
 *      v60 σ-Shield      — was the action allowed?
 *      v61 Σ-Citadel     — does the data flow honour BLP+Biba?
 *      v62 Reasoning     — is the EBT energy below budget?
 *      v63 σ-Cipher      — is the AEAD tag authentic and quote bound?
 *      v64 σ-Intellect   — does the intellect kernel authorise emit?
 *
 *  Each lane is 0/1; `.allow` is the 5-way AND.  No branches on the
 *  hot path; downstream code can read `.allow` or inspect failing
 *  lane(s) for telemetry (mirror pattern of cos_v62 / cos_v63).
 */
typedef struct {
    uint8_t v60_ok;            /* 1 if σ-Shield permitted            */
    uint8_t v61_ok;            /* 1 if Σ-Citadel lattice permitted   */
    uint8_t v62_ok;            /* 1 if EBT energy ≤ budget           */
    uint8_t v63_ok;            /* 1 if AEAD tag + quote bound        */
    uint8_t v64_ok;            /* 1 if σ-Intellect authorised        */
    uint8_t allow;             /* v60 & v61 & v62 & v63 & v64        */
    uint8_t _pad[2];
} cos_v64_decision_t;

cos_v64_decision_t
cos_v64_compose_decision(uint8_t v60_ok,
                         uint8_t v61_ok,
                         uint8_t v62_ok,
                         uint8_t v63_ok,
                         uint8_t v64_ok);

/* ====================================================================
 *  1.  Monte-Carlo Tree Search — MCTS-σ  (Empirical-MCTS class)
 *      rStar-Math / Nemotron-MCTS / AIME25.  arXiv:2602.04248.
 * ==================================================================== */

/*  One node of the PUCT tree.  Branch factor and depth are caller-
 *  bounded; the kernel never allocates on the hot path.  Nodes form a
 *  flat array (`tree->nodes`) for cache-friendly traversal; `parent`
 *  and `child0` are array indices, not pointers, so the whole tree
 *  can be mmap'd and re-attached across processes without fix-ups.
 *  UINT32_MAX sentinels denote "none".
 */
typedef struct {
    uint32_t        parent;    /* index of parent node, UINT32_MAX=root */
    uint32_t        child0;    /* index of first child, UINT32_MAX=leaf */
    uint16_t        nchild;    /* number of children                    */
    uint16_t        depth;     /* depth from root                       */
    uint32_t        visits;    /* N(s, a) — integer                     */
    int32_t         value_q15; /* Σ-reward / visits, Q0.15 (signed)     */
    cos_v64_sigma_t sigma;     /* cached σ for this node                */
    uint16_t        prior_q15; /* EBT / policy prior, Q0.15             */
    uint16_t        action_id; /* action index that led here (0..65535) */
} cos_v64_node_t;

/*  cos_v64_tree_t — caller-owned arena.  The kernel never resizes it.
 *  On overflow, `cos_v64_mcts_expand` returns -1 and the caller can
 *  back off (abstain, refuse, or reallocate between iterations).
 *  `cpuct_q15` is the PUCT exploration constant in Q0.15.
 */
typedef struct {
    cos_v64_node_t *nodes;     /* aligned_alloc(64, cap * sizeof(node))   */
    uint32_t        cap;       /* capacity                                */
    uint32_t        len;       /* nodes in use                            */
    uint32_t        root;      /* root index (always 0 if nonempty)       */
    uint16_t        cpuct_q15; /* PUCT exploration constant in Q0.15      */
    uint16_t        _pad;
} cos_v64_tree_t;

/*  Create / destroy.  Arena is 64-byte aligned.  cap must be > 1.
 *  cpuct_q15 is Q0.15 (1.0 ≈ 32768); reasonable default: 49152 = 1.5.
 */
cos_v64_tree_t *cos_v64_tree_new(uint32_t cap, uint16_t cpuct_q15);
void            cos_v64_tree_free(cos_v64_tree_t *t);

/*  Initialise the root node with a caller-supplied prior and σ.
 *  Resets `len` to 1 and `root` to 0.  Never allocates.
 */
void cos_v64_mcts_reset(cos_v64_tree_t       *t,
                        uint16_t              root_prior_q15,
                        cos_v64_sigma_t       root_sigma);

/*  PUCT selection — branchless tie-break on (score, action_id).
 *
 *      a* = argmax  Q(s, a) + c_puct * P(s, a) * sqrt(N(s)) / (1+N(s,a))
 *
 *  All arithmetic is fixed-point Q0.15 over int32, no FP.  Returns
 *  the *child-node index* of the selected action, or UINT32_MAX if the
 *  node has no children (leaf).  The caller expands the leaf next.
 */
uint32_t cos_v64_mcts_select(const cos_v64_tree_t *t, uint32_t node);

/*  Expand a leaf by appending `nchild` children with caller-supplied
 *  priors.  Returns 0 on success, -1 on arena overflow.  `priors_q15`
 *  is an array of `nchild` entries in Q0.15 (sum not required to be
 *  1; the PUCT formula normalises via the sqrt(N) term).
 */
int cos_v64_mcts_expand(cos_v64_tree_t *t,
                        uint32_t        node,
                        uint16_t        nchild,
                        const uint16_t *priors_q15);

/*  Backup a reward ∈ [-32768, 32767] (Q0.15) along the ancestor chain.
 *  Increments `visits` by 1 and updates `value_q15` as a running mean.
 *  Branchless; constant in tree depth.
 */
void cos_v64_mcts_backup(cos_v64_tree_t *t,
                         uint32_t        leaf,
                         int32_t         reward_q15);

/*  Pick the final action (most-visited child) from the root.  Ties
 *  broken branchlessly by higher value_q15 then lower action_id.
 *  Returns child index in [0, root.nchild), or UINT32_MAX if root is
 *  a leaf (no children).
 */
uint32_t cos_v64_mcts_best(const cos_v64_tree_t *t);

/* ====================================================================
 *  2.  Skill library — Voyager / EvoSkill class.
 *      arXiv:2603.02766; Wang et al. 2023.
 * ==================================================================== */

/*  cos_v64_skill_t — one compositional skill.
 *
 *  A skill is identified by a 32-byte σ-signature (SigLIP-style
 *  hash of (precondition, effect, σ-profile) — caller-supplied).
 *  Retrieval is by Hamming distance over the signature, which keeps
 *  the inner loop popcount-native on NEON and integer-only.  No FP,
 *  no embedding matrix multiplication.
 *
 *  `confidence_q15` is the σ-weighted success rate in Q0.15 and is
 *  updated by `cos_v64_reflect_update` after each use.
 *
 *  `uses`, `wins` are monotone counters; when either overflows (very
 *  long-running deployment) the kernel down-shifts both by 1 so the
 *  ratio is preserved.
 */
#define COS_V64_SKILL_SIG_BYTES  32
typedef struct {
    uint8_t         sig[COS_V64_SKILL_SIG_BYTES]; /* σ-hash (opaque)    */
    uint16_t        confidence_q15;               /* σ-weighted win rate */
    uint16_t        uses;                         /* total invocations   */
    uint16_t        wins;                         /* successful outcomes */
    uint16_t        skill_id;                     /* caller-assigned id  */
} cos_v64_skill_t;

typedef struct {
    cos_v64_skill_t *skills;   /* aligned_alloc(64, cap * sizeof(skill)) */
    uint32_t         cap;      /* capacity                               */
    uint32_t         len;      /* skills in use                          */
} cos_v64_skill_lib_t;

cos_v64_skill_lib_t *cos_v64_skill_lib_new(uint32_t cap);
void                 cos_v64_skill_lib_free(cos_v64_skill_lib_t *l);

/*  Register a new skill.  Returns 0 on success, -1 on overflow, -2
 *  if a signature collision occurs (library already contains this
 *  skill; use `cos_v64_skill_lookup` + `cos_v64_reflect_update`).
 */
int cos_v64_skill_register(cos_v64_skill_lib_t *l,
                           const uint8_t       *sig,
                           uint16_t             skill_id,
                           uint16_t             initial_conf_q15);

/*  Lookup by exact signature.  Returns index or UINT32_MAX on miss.
 *  Constant-time in `len` (linear scan, branchless early-exit
 *  disallowed for timing hygiene — always walks the full library).
 */
uint32_t cos_v64_skill_lookup(const cos_v64_skill_lib_t *l,
                              const uint8_t             *sig);

/*  Retrieve the skill with the smallest Hamming distance whose
 *  `confidence_q15 ≥ min_conf_q15`.  Returns UINT32_MAX if the
 *  library is empty or no skill clears the floor.  Ties broken by
 *  higher confidence then lower skill_id.
 */
uint32_t cos_v64_skill_retrieve(const cos_v64_skill_lib_t *l,
                                const uint8_t             *query_sig,
                                uint16_t                   min_conf_q15,
                                uint32_t                  *out_hamming);

/* ====================================================================
 *  3.  Verified tool use — Dynamic-ReAct / SmolAgents class.
 *      arXiv:2509.20386; SWE-World arXiv:2602.03419.
 * ==================================================================== */

/*  Tool descriptor.  The kernel never executes tools itself; it only
 *  authorises or refuses invocations based on caller-supplied σ,
 *  precondition bitmask, and schema hash.  Effect hash is optional
 *  but when present enables the reflexion ratchet to compare
 *  observed effect against the declared one (bit-exact).
 */
typedef struct {
    uint32_t        tool_id;
    uint16_t        schema_ver;   /* bumped when args layout changes */
    uint16_t        required_caps;/* v60 cap bitmask requested       */
    uint8_t         schema_hash[32]; /* BLAKE2b-256 of argument schema */
    uint8_t         effect_hash[32]; /* declared effect hash (optional)*/
} cos_v64_tool_desc_t;

/*  Tool-use decision.  Branchless; mirrors v60's priority cascade but
 *  for tool call semantics:
 *
 *      ALLOW               — caps match, σ acceptable, schema current
 *      DENY_CAPS           — required_caps ⊄ allowed_caps
 *      DENY_SIGMA          — σ.alp above aleatoric threshold
 *      DENY_SCHEMA         — schema_hash disagrees with arg_hash
 *      DENY_TOCTOU         — arg hash at entry ≠ arg hash at use
 *      DENY_RESERVED       — schema version newer than kernel
 */
typedef enum {
    COS_V64_TOOL_ALLOW         = 0,
    COS_V64_TOOL_DENY_CAPS     = 1,
    COS_V64_TOOL_DENY_SIGMA    = 2,
    COS_V64_TOOL_DENY_SCHEMA   = 3,
    COS_V64_TOOL_DENY_TOCTOU   = 4,
    COS_V64_TOOL_DENY_RESERVED = 5
} cos_v64_tool_verdict_t;

typedef struct {
    cos_v64_tool_verdict_t verdict;
    uint8_t                reason_bits;     /* multi-cause lane mask */
    uint8_t                _pad[3];
} cos_v64_tool_authz_t;

/*  Branchless authorise.  `arg_hash_at_entry` and `arg_hash_at_use`
 *  must both be 32-byte BLAKE2b-256 hashes computed by the caller; if
 *  they disagree the verdict is DENY_TOCTOU regardless of other lanes.
 *  `alp_threshold_q15` gates the aleatoric σ lane.
 *
 *  Constant-time in all 32-byte comparisons; no early exit on a
 *  matching / mismatching byte.
 */
cos_v64_tool_authz_t
cos_v64_tool_authorise(const cos_v64_tool_desc_t *desc,
                       uint16_t                    allowed_caps,
                       uint16_t                    schema_ver_max,
                       cos_v64_sigma_t             sigma,
                       uint16_t                    alp_threshold_q15,
                       const uint8_t              *arg_hash_at_entry,
                       const uint8_t              *arg_hash_at_use);

/* ====================================================================
 *  4.  Reflexion ratchet — ERL / ReflexiCoder class.
 *      arXiv:2603.24639; arXiv:2603.05863.
 * ==================================================================== */

/*  One reflexion step: compare predicted outcome to observed, update
 *  the skill library in place.  `predicted_sigma` and `observed_sigma`
 *  are fixed-point Q0.15 lanes; the returned delta is
 *      Δσ = predicted.eps - observed.eps  (Q0.15 signed)
 *  plus
 *      Δα = predicted.alp - observed.alp.
 *  Positive Δε means "we were less certain than we needed to be" →
 *  the heuristic reduces predicted ε on next use; negative means we
 *  were overconfident → increase ε.  No FP on the hot path.
 *
 *  The skill's `uses` is incremented; `wins` is incremented iff
 *  `observed.alp ≤ alp_win_threshold_q15`.  Confidence is updated as
 *      conf' = (wins' << 15) / uses'     (Q0.15)
 *  — a branchless integer division in the caller's arena.
 *
 *  Returns 0 on success, -1 if `skill_idx` is out of range.
 */
typedef struct {
    int32_t   delta_eps_q15;   /* predicted.eps − observed.eps     */
    int32_t   delta_alp_q15;   /* predicted.alp − observed.alp     */
    uint16_t  new_conf_q15;    /* skill confidence after update    */
    uint16_t  _pad;
    uint32_t  skill_idx;       /* index of the updated skill       */
} cos_v64_reflect_t;

int cos_v64_reflect_update(cos_v64_skill_lib_t *l,
                           uint32_t             skill_idx,
                           cos_v64_sigma_t      predicted,
                           cos_v64_sigma_t      observed,
                           uint16_t             alp_win_threshold_q15,
                           cos_v64_reflect_t   *out_reflect);

/* ====================================================================
 *  5.  AlphaEvolve-σ — σ-gated evolutionary code / parameter search.
 *      DeepMind 2025; arXiv:2602.XXXX EvoX 2026.
 *
 *  We model the search state as a vector of ternary weights in
 *  {-1, 0, +1} packed two-bits-per-weight (so 16 weights / uint32).
 *  This is the BitNet-b1.58 (arXiv:2402.17764) storage layout; it
 *  keeps the kernel allocation-free and gives a clean lattice for
 *  mutation.  A mutation flips some weights' signs (caller-supplied
 *  mask of bit positions); the kernel accepts the mutation iff
 *  `child_sigma.alp ≤ parent_sigma.alp` and `child_energy ≤
 *  parent_energy * accept_slack_q15 / 32768`.  Otherwise it rolls
 *  back.  No FP, no temperature schedule; σ *is* the schedule.
 * ==================================================================== */

/*  cos_v64_bitnet_t — packed ternary state.  `weights` is a uint32
 *  array storing 16 ternary weights per element: bit pair 00 = 0,
 *  01 = +1, 10 = -1, 11 reserved (caller must never produce it; we
 *  validate on init and clip on mutate).
 */
typedef struct {
    uint32_t *weights;         /* aligned_alloc(64, nw_u32 * 4)    */
    uint32_t  nw_u32;          /* number of uint32 elements        */
    uint32_t  nweights;        /* logical weight count (≤ 16*nw_u32)*/
} cos_v64_bitnet_t;

cos_v64_bitnet_t *cos_v64_bitnet_new(uint32_t nweights);
void              cos_v64_bitnet_free(cos_v64_bitnet_t *b);

/*  Accept-or-rollback mutation.  `flip_mask_bits` is an array of
 *  `nflips` 32-bit weight indices (one per mutation target).  The
 *  kernel applies the mutation, evaluates `score_fn(weights,
 *  nweights, ctx) → (energy_q15, σ)`, compares to the parent state,
 *  and either keeps the mutation (returning 1) or rolls back
 *  (returning 0).  Never allocates; uses a caller-supplied scratch
 *  buffer of `nw_u32` uint32 to snapshot parent state.
 *
 *  Returns 1 accepted, 0 rejected, -1 on error.  The σ monotonicity
 *  guarantee: after N successful accept calls, `parent_sigma.alp`
 *  is monotone non-increasing.
 */
typedef struct {
    int32_t         energy_q15;
    cos_v64_sigma_t sigma;
    uint16_t        _pad;
} cos_v64_evolve_score_t;

typedef int (*cos_v64_score_fn)(const uint32_t *weights,
                                uint32_t        nweights,
                                void           *ctx,
                                cos_v64_evolve_score_t *out);

int cos_v64_evolve_mutate(cos_v64_bitnet_t        *b,
                          const uint32_t          *flip_idx,
                          uint32_t                 nflips,
                          cos_v64_score_fn         score_fn,
                          void                    *score_ctx,
                          uint32_t                *scratch,
                          uint16_t                 accept_slack_q15,
                          cos_v64_evolve_score_t  *inout_parent);

/* ====================================================================
 *  6.  Mixture-of-Depths-σ — per-token depth routing.
 *      arXiv:2404.02258 (Google), arXiv:2603.15619 MoDA 2026,
 *      arXiv:2412.20875 A-MoD.
 *
 *  Given a per-token σ decomposition for a prefill or decode call,
 *  decide the compute depth for each token:
 *      depth_t = clamp( D_min + round((alp_t / 32768) * (D_max - D_min)),
 *                       D_min, D_max )
 *  Branchless integer rounding (add-shift); constant-time in ntokens.
 *
 *  The mapping is *strictly monotone* in α: ε-dominated tokens (low
 *  α, high ε, "we know we don't know") route shallow; α-dominated
 *  tokens (high α, "genuinely ambiguous") route full depth.  This
 *  matches A-MoD's attention-based routing but replaces the learned
 *  router with the σ signal we already compute.
 *
 *  `out_depths` is a caller-supplied uint8 array of length `ntokens`.
 */
void cos_v64_mod_route(const cos_v64_sigma_t *sigmas,
                       uint32_t                ntokens,
                       uint8_t                 d_min,
                       uint8_t                 d_max,
                       uint8_t                *out_depths);

/*  Sum of depths routed — lets callers know the total compute cost
 *  before actually running the forward pass (so the σ-Budget kernel
 *  v59 can refuse a request that exceeds the allowed integer compute
 *  budget).  Constant-time in ntokens.
 */
uint64_t cos_v64_mod_cost(const uint8_t *depths, uint32_t ntokens);

/* ====================================================================
 *  7.  Version string — consumed by the cos CLI.
 * ==================================================================== */

extern const char cos_v64_version[];

#ifdef __cplusplus
}
#endif

#endif /* COS_V64_INTELLECT_H */
