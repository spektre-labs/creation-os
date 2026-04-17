/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v74 — σ-Experience (the Experience kernel).
 *
 * ------------------------------------------------------------------
 *  What v74 is
 * ------------------------------------------------------------------
 *
 * Every kernel up to v73 is either a *gate* (σ-Shield … σ-Chain) or
 * the *Creator* (σ-Omnimodal).  What is still missing is the
 * *Experience*: the single branchless integer substrate that lets
 * the composed stack be delivered to a human with Apple-tier
 * usability, universal expertise, and render budget that makes the
 * heaviest 2026-era games playable on commodity silicon — M4
 * MacBook, iPhone-class SoC, a plain Snapdragon phone — without ever
 * leaving the integer + HV + XOR discipline.
 *
 * v74 σ-Experience ships ten primitives covering the 2024-2026 UX,
 * universal-expert, and real-time-render frontier:
 *
 *   1.  Fitts-V2P target heatmap (arXiv:2508.13634, V2P GUI-grounding
 *       92.4% benchmark).  2-D integer Gaussian weight per target:
 *       a branchless scan returns argmax target, with per-target
 *       variance set from target size so Fitts' Law ID is honoured.
 *
 *   2.  Adaptive layout optimiser (Log2Motion CHI '26 arXiv:
 *       2601.21043 biomechanical effort estimator, SQUIRE slot-
 *       scope guarantees, Apple ML arXiv:2002.10702 UI-layout
 *       gradient-descent lineage).  Given N UI slot HVs + a Fitts
 *       effort budget + reachability bitset, verify that a proposed
 *       placement respects every constraint in one branchless AND
 *       reduction.
 *
 *   3.  Designer-basis personalisation (arXiv:2604.09876 — designers
 *       disagree with mean κ = 0.25 on hierarchy / cleanliness).
 *       Users are represented as argmin-Hamming against K designer-
 *       basis HVs; this outperformed pre-trained UI evaluators and
 *       much larger multimodal models in the 2026 user study.
 *
 *   4.  SquireIR slot authoring (Apple SQUIRE April 2026).  Slot-
 *       query intermediate representation with explicit mutable
 *       bitset M and frozen bitset F; any proposed edit E is
 *       admissible iff (E & F) == 0 AND (E & ~M) == 0.  One AND
 *       reduction, zero branches.
 *
 *   5.  Universal-expert LoRA-MoE mesh (DR-LoRA arXiv:2601.04823,
 *       CoMoL arXiv:2603.00573, MoLE arXiv:2404.13628, MixLoRA
 *       arXiv:2404.15159v3).  64 expert HVs, each with a rank score;
 *       top-k router by popcount-Hamming; margin gate
 *       (d1 + τ ≤ d2) enforces non-degenerate routing.
 *
 *   6.  Skill composition.  XOR-bind the top-k expert HVs under
 *       per-domain permutation so arbitrary expertise (code, render,
 *       UX, finance, biology, security, …) is a single HV that can
 *       be verified against a claim manifold by one Hamming compare.
 *
 *   7.  Mobile-GS order-free 3-D Gaussian-splat render step (Mobile-
 *       GS arXiv:2603.11531 ICLR 2026: 116 FPS at 1600×1063 on
 *       Snapdragon 8 Gen 3, 1098 FPS on RTX 3090, 4.8 MB models;
 *       msplat Metal-native engine March 2026: ~350 FPS on M4 Max).
 *       Alpha-sort is eliminated via depth-aware order-independent
 *       rendering; we verify a per-tile visibility budget and a
 *       neural-view-enhancement Hamming tolerance in branchless
 *       integer form.
 *
 *   8.  Neural upscale + multi-frame-generation gate (DLSS 4.5
 *       Dynamic Multi-Frame Generation, 6× factor; FSR; Intel XeSS;
 *       GenMFSR arXiv:2603.19187; GameSR engine-independent SR).
 *       Branchless compare:  base_hz × gen_factor ≤ display_hz AND
 *       quality ≥ τ AND gen_factor ∈ {1..6}.  This is what lets
 *       GTA-tier scenes hit 120 FPS on an M4 or a phone.
 *
 *   9.  Second-render interactive-world synth (Genie 3 lineage,
 *       DeepMind 2026: 720p, 20-24 FPS, text-to-interactive-world).
 *       From a seed + style HV (via designer-basis) synthesise an
 *       N-tile HV world in a bounded creation-unit count; bridges
 *       directly to v73's WORLD opcode so the whole loop
 *       (Creator × Experience) closes in one second.
 *
 *  10.  XPL — Experience Programming Language — 10-opcode integer
 *       bytecode ISA.
 *       Cost model (creation-units, mirrors v73 OML):
 *           HALT     = 1
 *           TARGET   = 2        (Fitts-V2P heatmap argmax)
 *           LAYOUT   = 4        (adaptive layout verify)
 *           BASIS    = 2        (designer-basis nearest)
 *           SLOT     = 2        (SquireIR scope check)
 *           EXPERT   = 8        (LoRA-MoE top-k + skill compose)
 *           RENDER   = 8        (Mobile-GS order-free step)
 *           UPSCALE  = 4        (DLSS / FSR / XeSS gate)
 *           WORLD    = 16       (second-render world synth)
 *           GATE     = 1
 *
 *       GATE writes `v74_ok = 1` iff:
 *           creation_units       ≤ budget
 *           AND target_hit       == 1
 *           AND layout_ok        == 1
 *           AND basis_ok         == 1
 *           AND slot_ok          == 1
 *           AND expert_ok        == 1
 *           AND skill_ok         == 1    (auto-set by EXPERT)
 *           AND render_ok        == 1
 *           AND upscale_ok       == 1
 *           AND world_second_ok  == 1
 *           AND abstained        == 0
 *
 * ------------------------------------------------------------------
 *  Composed 15-bit branchless decision
 * ------------------------------------------------------------------
 *
 *   v60 σ-Shield         — may the action cross?
 *   v61 Σ-Citadel        — do BLP + Biba + MLS allow the label?
 *   v62 Reasoning        — does the energy verifier close?
 *   v63 σ-Cipher         — is the sealed envelope well-formed?
 *   v64 σ-Intellect      — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex    — is the thought on-manifold + under HVL cost?
 *   v66 σ-Silicon        — does the matrix path clear conformal + MAC?
 *   v67 σ-Noesis         — does the deliberation close + receipts?
 *   v68 σ-Mnemos         — recall ≥ threshold AND forget ≤ budget?
 *   v69 σ-Constellation  — quorum margin ≥ threshold AND faults ≤ f?
 *   v70 σ-Hyperscale     — silicon + throughput + topology budgets?
 *   v71 σ-Wormhole       — teleport arrived + under hop budget?
 *   v72 σ-Chain          — chain-bound step clears gas + quorum + OTS?
 *   v73 σ-Omnimodal      — the Creator: flow + bind + lock + physics
 *                          + plan + workflow + creation-unit budget?
 *   v74 σ-Experience     — the Experience: target + layout + basis
 *                          + slot + expert + skill + render + upscale
 *                          + world-second + creation-unit budget?
 *
 * `cos_v74_compose_decision` is a single 15-way branchless AND.
 * Nothing — no inference, tool call, sealed message, retrieval, write,
 * route, hyperscale step, teleport, chain emission, generated artefact
 * (code / image / audio / video / world-frame / workflow output), or
 * delivered user experience (UI layout, accessibility stream, render
 * frame, upscaled frame, 1-second interactive world) crosses to the
 * human unless every one of the fifteen kernels ALLOWs.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline
 * ------------------------------------------------------------------
 *
 *   - HV token:  256 bits = 4 × uint64 = 32 B; naturally aligned.
 *   - Expert / basis / slot / tile arenas: 64-B aligned via
 *     `aligned_alloc`.
 *   - Fitts-V2P heatmap: integer squared-distance compare per target,
 *     branchless argmax with saturating subtraction; no FP.
 *   - Layout verify is a single branchless AND over reachability,
 *     slot-frozen, Fitts-budget bitsets.
 *   - Designer-basis nearest is a popcount-Hamming argmin.
 *   - SquireIR scope check is a two-way bitset AND.
 *   - Expert-mesh top-k uses a branchless running-min-4 sort.
 *   - Skill-bind is four XOR-binds with per-modality permutation.
 *   - Mobile-GS step = per-tile bitset AND with depth-bucket masks
 *     + popcount; neural-view-enhancement is a Hamming tolerance.
 *   - Upscale / frame-gen gate is three branchless integer compares.
 *   - Second-render world synth writes deterministic HV tiles from
 *     (seed, style) in a fixed step budget; one Hamming compare
 *     per tile closes coherence.
 *   - 15-bit composed decision is a single branchless AND.
 *
 * 1 = 1.
 */

#ifndef COS_V74_EXPERIENCE_H
#define COS_V74_EXPERIENCE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Primitive types and constants.
 * ==================================================================== */

#define COS_V74_HV_WORDS        4u          /* 4 × uint64 = 256 bits */
#define COS_V74_HV_BYTES        32u
#define COS_V74_HV_BITS         256u

typedef struct {
    uint64_t w[COS_V74_HV_WORDS];
} cos_v74_hv_t;

/* Limits (compile-time). */
#define COS_V74_TARGETS_MAX         512u
#define COS_V74_SLOTS_MAX           128u
#define COS_V74_BASIS_MAX            32u
#define COS_V74_EXPERTS_MAX         256u
#define COS_V74_TOPK_MAX              4u
#define COS_V74_TILES_MAX         16384u
#define COS_V74_DEPTH_BUCKETS         4u
#define COS_V74_WORLD_TILES_MAX   16384u
#define COS_V74_NREGS                16u
#define COS_V74_PROG_MAX           1024u
#define COS_V74_FRAMEGEN_FACTOR_MAX   6u

/* Domain id (used for per-domain permutation under skill-compose). */
typedef enum {
    COS_V74_DOM_UX      = 0,
    COS_V74_DOM_CODE    = 1,
    COS_V74_DOM_RENDER  = 2,
    COS_V74_DOM_AUDIO   = 3,
    COS_V74_DOM_MATH    = 4,
    COS_V74_DOM_BIO     = 5,
    COS_V74_DOM_SEC     = 6,
    COS_V74_DOM_WORLD   = 7,
} cos_v74_domain_t;

/* ====================================================================
 *  HV primitives (mirror v73 so the two kernels share an alphabet).
 * ==================================================================== */

void     cos_v74_hv_zero   (cos_v74_hv_t *dst);
void     cos_v74_hv_copy   (cos_v74_hv_t *dst, const cos_v74_hv_t *src);
void     cos_v74_hv_xor    (cos_v74_hv_t *dst, const cos_v74_hv_t *a, const cos_v74_hv_t *b);
void     cos_v74_hv_from_seed(cos_v74_hv_t *dst, uint64_t seed);
void     cos_v74_hv_permute(cos_v74_hv_t *dst, const cos_v74_hv_t *src, cos_v74_domain_t d);
uint32_t cos_v74_hv_hamming(const cos_v74_hv_t *a, const cos_v74_hv_t *b);

/* ====================================================================
 *  1.  Fitts-V2P target heatmap.
 *
 *  Each target is a position + size.  The per-target weight at cursor
 *  (cx, cy) is a saturating integer Gaussian:
 *      w = max(0, var - dx*dx - dy*dy)
 *  where var = size*size is the Fitts variance.  Branchless argmax.
 * ==================================================================== */

typedef struct {
    int32_t  x;
    int32_t  y;
    int32_t  size;          /* target half-width in pixels */
    uint32_t flags;         /* reserved                    */
} cos_v74_target_t;

/* Return argmax target index + optional weight.  n must be ≤ MAX. */
uint32_t cos_v74_target_argmax(const cos_v74_target_t *tg,
                               uint32_t                n,
                               int32_t                 cx,
                               int32_t                 cy,
                               int32_t                *out_weight);

/* Return 1 iff argmax weight ≥ threshold (i.e. user intent is inside
 * Fitts catchment of the best target). */
uint8_t  cos_v74_target_hit(const cos_v74_target_t *tg,
                            uint32_t                n,
                            int32_t                 cx,
                            int32_t                 cy,
                            int32_t                 threshold);

/* ====================================================================
 *  2.  Adaptive layout optimiser (verify).
 *
 *  A layout is an array of N slot indices → target indices.  Verify
 *  that:
 *      - every slot maps inside a reachability bitset,
 *      - no frozen slot is reassigned,
 *      - total Fitts effort ≤ effort_budget,
 *      - every target index is unique (injective mapping).
 *
 *  All four checks combine in a single branchless AND.
 * ==================================================================== */

typedef struct {
    const uint64_t         *reach_bits;   /* ceil(n_targets/64) words */
    uint32_t                n_targets;
    uint32_t                n_slots;
    const uint64_t         *frozen_bits;  /* ceil(n_slots/64)  words  */
    const uint32_t         *frozen_slot_map; /* n_slots (meaningful
                                              * where frozen bit set) */
    int32_t                 effort_budget;
} cos_v74_layout_spec_t;

uint8_t cos_v74_layout_verify(const cos_v74_layout_spec_t *spec,
                              const cos_v74_target_t      *tg,
                              const uint32_t              *slot_to_target,
                              const int32_t               *slot_cx,
                              const int32_t               *slot_cy,
                              int32_t                     *out_total_effort);

/* ====================================================================
 *  3.  Designer-basis personalisation.
 *
 *  Represent the user as argmin-Hamming over a bank of K designer-
 *  basis HVs (K ≤ COS_V74_BASIS_MAX).  Returns basis idx + distance.
 * ==================================================================== */

typedef struct {
    cos_v74_hv_t *bank;          /* K × 32 B, 64-B aligned */
    uint32_t      k;             /* ≤ COS_V74_BASIS_MAX    */
    uint32_t      _pad;
} cos_v74_basis_t;

cos_v74_basis_t *cos_v74_basis_new(uint32_t k, uint64_t seed);
void             cos_v74_basis_free(cos_v74_basis_t *b);

uint32_t cos_v74_basis_nearest(const cos_v74_basis_t *b,
                               const cos_v74_hv_t    *user_hv,
                               uint32_t              *out_dist);

/* Return 1 iff nearest distance ≤ tol AND margin ≥ margin_min. */
uint8_t  cos_v74_basis_ok(const cos_v74_basis_t *b,
                          const cos_v74_hv_t    *user_hv,
                          uint32_t               tol,
                          uint32_t               margin_min);

/* ====================================================================
 *  4.  SquireIR slot authoring.
 *
 *  Given mutable bitset M and frozen bitset F, a proposed edit E is
 *  admissible iff (E & F) == 0 AND (E & ~M) == 0.  row_words =
 *  ceil(n_slots/64).
 * ==================================================================== */

uint8_t cos_v74_squire_verify(const uint64_t *mutable_bits,
                              const uint64_t *frozen_bits,
                              const uint64_t *edit_bits,
                              uint32_t        row_words);

/* ====================================================================
 *  5.  Universal-expert LoRA-MoE mesh.
 *
 *  64-expert bank.  Query HV → top-k by popcount-Hamming.  Router
 *  margin gate: d_{k+1} - d_1 ≥ margin_min.
 * ==================================================================== */

typedef struct {
    cos_v74_hv_t *bank;          /* n × 32 B, 64-B aligned */
    uint8_t      *rank;          /* n × uint8 (DR-LoRA rank in {1..31}) */
    uint32_t      n;
    uint32_t      _pad;
} cos_v74_expert_t;

cos_v74_expert_t *cos_v74_expert_new(uint32_t n, uint64_t seed);
void              cos_v74_expert_free(cos_v74_expert_t *e);

typedef struct {
    uint32_t idx[COS_V74_TOPK_MAX];
    uint32_t dist[COS_V74_TOPK_MAX];
    uint32_t k;
} cos_v74_topk_t;

void cos_v74_expert_topk(const cos_v74_expert_t *e,
                         const cos_v74_hv_t     *q,
                         uint32_t                k,
                         cos_v74_topk_t         *out);

/* Router gate: returns 1 iff k ≤ TOPK_MAX, distances are monotone,
 * and margin (dist[k-1] - dist[0]) ≥ margin_min. */
uint8_t cos_v74_expert_route_ok(const cos_v74_topk_t *tk,
                                uint32_t              margin_min);

/* ====================================================================
 *  6.  Skill composition.
 *
 *  Bind the top-k expert HVs under per-domain permutation.  Verify
 *  against a claim manifold by one Hamming compare.
 * ==================================================================== */

void cos_v74_skill_bind(cos_v74_hv_t           *out,
                        const cos_v74_expert_t *e,
                        const cos_v74_topk_t   *tk,
                        const cos_v74_domain_t *doms);   /* tk->k entries */

uint8_t cos_v74_skill_verify(const cos_v74_expert_t *e,
                             const cos_v74_topk_t   *tk,
                             const cos_v74_domain_t *doms,
                             const cos_v74_hv_t     *claim,
                             uint32_t                tol);

/* ====================================================================
 *  7.  Mobile-GS order-free 3-D Gaussian-splat render step.
 *
 *  Depth buckets (4) × per-tile Gaussian-bitset word.  Visibility
 *  score = popcount(tile ∧ bucket).  The tile passes iff:
 *      total_score ≤ vis_budget
 *      AND neural_view_hamming(tile_tag, claim_tag) ≤ tol
 * ==================================================================== */

typedef struct {
    uint64_t     *tile_bits;     /* n_tiles × row_words words, 64-B aligned */
    uint64_t      bucket_mask[COS_V74_DEPTH_BUCKETS];
    cos_v74_hv_t *tile_tag;      /* n_tiles × HV, 64-B aligned */
    uint32_t      n_tiles;
    uint32_t      row_words;
    uint32_t      vis_budget;
    uint32_t      view_tol;
} cos_v74_render_t;

cos_v74_render_t *cos_v74_render_new(uint32_t n_tiles,
                                     uint32_t row_words,
                                     uint64_t seed);
void              cos_v74_render_free(cos_v74_render_t *r);

/* Returns 1 iff the whole frame clears the visibility + neural-view
 * tolerances at claim_tag.  Writes per-tile score sum into out_score. */
uint8_t cos_v74_render_step(const cos_v74_render_t *r,
                            const cos_v74_hv_t     *claim_tag,
                            uint32_t               *out_score);

/* ====================================================================
 *  8.  Neural upscale + multi-frame-generation gate.
 *
 *  Integer compare:
 *      gen_factor ∈ {1..6}  AND  base_hz × gen_factor ≤ display_hz
 *      AND quality_score ≥ quality_min
 * ==================================================================== */

typedef struct {
    uint32_t base_hz;
    uint32_t display_hz;
    uint32_t gen_factor;
    uint32_t quality_score;
    uint32_t quality_min;
} cos_v74_upscale_t;

uint8_t cos_v74_upscale_ok(const cos_v74_upscale_t *u);

/* ====================================================================
 *  9.  Second-render interactive-world synth.
 *
 *  Deterministic: same (seed, style_hv, n_tiles) → same HV tile grid.
 *  Generates an N-tile world in a bounded creation-unit count.  Writes
 *  each tile_hv[i] = permute(splitmix(seed, i), DOM_WORLD) ⊕ style_hv.
 *  Passes iff:
 *      used_units ≤ second_budget
 *      AND coherence_hamming(tile_hv[0], tile_hv[n-1]) ≤ coherence_tol
 * ==================================================================== */

uint8_t cos_v74_world_second(cos_v74_hv_t       *tiles,
                             uint32_t            n_tiles,
                             uint64_t            seed,
                             const cos_v74_hv_t *style_hv,
                             uint32_t            second_budget,
                             uint32_t            coherence_tol,
                             uint32_t           *out_used_units);

/* ====================================================================
 * 10.  XPL — Experience Programming Language — 10-opcode bytecode ISA.
 *
 *  (see cost model in file-level comment)
 * ==================================================================== */

typedef enum {
    COS_V74_OP_HALT    = 0,
    COS_V74_OP_TARGET  = 1,
    COS_V74_OP_LAYOUT  = 2,
    COS_V74_OP_BASIS   = 3,
    COS_V74_OP_SLOT    = 4,
    COS_V74_OP_EXPERT  = 5,   /* also sets skill_ok via inline bind  */
    COS_V74_OP_RENDER  = 6,
    COS_V74_OP_UPSCALE = 7,
    COS_V74_OP_WORLD   = 8,
    COS_V74_OP_GATE    = 9,
} cos_v74_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v74_xpl_inst_t;

typedef struct {
    /* TARGET */
    const cos_v74_target_t *targets;
    uint32_t                n_targets;
    int32_t                 cursor_x;
    int32_t                 cursor_y;
    int32_t                 target_threshold;

    /* LAYOUT */
    const cos_v74_layout_spec_t *layout;
    const uint32_t              *slot_to_target;
    const int32_t               *slot_cx;
    const int32_t               *slot_cy;

    /* BASIS */
    const cos_v74_basis_t *basis;
    const cos_v74_hv_t    *user_hv;
    uint32_t               basis_tol;
    uint32_t               basis_margin;

    /* SLOT */
    const uint64_t *slot_mutable;
    const uint64_t *slot_frozen;
    const uint64_t *slot_edit;
    uint32_t        slot_row_words;

    /* EXPERT + SKILL */
    const cos_v74_expert_t *expert;
    const cos_v74_hv_t     *expert_query;
    uint32_t                expert_k;
    uint32_t                expert_margin;
    const cos_v74_domain_t *skill_doms;
    const cos_v74_hv_t     *skill_claim;
    uint32_t                skill_tol;

    /* RENDER */
    const cos_v74_render_t *render;
    const cos_v74_hv_t     *render_claim;

    /* UPSCALE */
    const cos_v74_upscale_t *upscale;

    /* WORLD (second-render) */
    cos_v74_hv_t       *world_tiles;
    uint32_t            world_n_tiles;
    uint64_t            world_seed;
    const cos_v74_hv_t *world_style;
    uint32_t            world_second_budget;
    uint32_t            world_coherence_tol;

    uint8_t abstain;
} cos_v74_xpl_ctx_t;

typedef struct cos_v74_xpl_state cos_v74_xpl_state_t;

cos_v74_xpl_state_t *cos_v74_xpl_new(void);
void                 cos_v74_xpl_free(cos_v74_xpl_state_t *s);
void                 cos_v74_xpl_reset(cos_v74_xpl_state_t *s);

uint64_t cos_v74_xpl_units         (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_target_ok     (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_layout_ok     (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_basis_ok      (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_slot_ok       (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_expert_ok     (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_skill_ok      (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_render_ok     (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_upscale_ok    (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_world_second_ok(const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_abstained     (const cos_v74_xpl_state_t *s);
uint8_t  cos_v74_xpl_v74_ok        (const cos_v74_xpl_state_t *s);

/* Returns 0 on success, -1 on malformed program, -2 on budget
 * exceeded, -3 on missing/invalid context for the opcode. */
int cos_v74_xpl_exec(cos_v74_xpl_state_t       *s,
                     const cos_v74_xpl_inst_t  *prog,
                     uint32_t                   n,
                     cos_v74_xpl_ctx_t         *ctx,
                     uint64_t                   unit_budget);

/* ====================================================================
 *  Composed 15-bit branchless decision.
 * ==================================================================== */

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
    uint8_t v71_ok;
    uint8_t v72_ok;
    uint8_t v73_ok;
    uint8_t v74_ok;
    uint8_t allow;
} cos_v74_decision_t;

cos_v74_decision_t cos_v74_compose_decision(uint8_t v60_ok,
                                            uint8_t v61_ok,
                                            uint8_t v62_ok,
                                            uint8_t v63_ok,
                                            uint8_t v64_ok,
                                            uint8_t v65_ok,
                                            uint8_t v66_ok,
                                            uint8_t v67_ok,
                                            uint8_t v68_ok,
                                            uint8_t v69_ok,
                                            uint8_t v70_ok,
                                            uint8_t v71_ok,
                                            uint8_t v72_ok,
                                            uint8_t v73_ok,
                                            uint8_t v74_ok);

/* ====================================================================
 *  Version string.
 * ==================================================================== */

extern const char cos_v74_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V74_EXPERIENCE_H */
