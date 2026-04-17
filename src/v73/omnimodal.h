/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v73 — σ-Omnimodal (the Creator kernel).
 *
 * ------------------------------------------------------------------
 *  What v73 is
 * ------------------------------------------------------------------
 *
 * Every kernel up to v72 is a *gate*: σ-Shield, Σ-Citadel, Reasoning,
 * Cipher, Intellect, Hypercortex, Silicon, Noesis, Mnemos, Constell-
 * ation, Hyperscale, Wormhole, Chain.  What is still missing is the
 * *Creator* — the single branchless integer substrate that lets the
 * composed stack *produce* arbitrary modality output under a common
 * hyperdimensional token discipline: code (Cursor / Lovable / Devin /
 * Bolt / Base44 / Totalum lineage), image (Midjourney / FLUX / VINO
 * MMDiT lineage, arXiv:2601.02358), video (Sora / Veo / MOVA / Matrix-
 * Game-3 lineage, arXiv:2604.08995 + arXiv:2508.13009 + Matrix-Game-2
 * ICLR 2025 + GameNGen ICLR 2025), audio / voice clone (ElevenLabs /
 * SoundStream arXiv:2107.03312 / Encodec RVQ lineage / MSR-Codec 2024
 * semantic-timbre-prosody disentanglement), workflow (n8n v2.10
 * "Brain vs Hands" + JudgeFlow arXiv:2601.07477 + ReusStdFlow + SOAN
 * arXiv:2508.13732v1), physics-coherent flow (DiReCT arXiv:2603.25931,
 * StreamFlow arXiv:2511.22009, MixFlow arXiv:2604.09181, VRFM
 * arXiv:2502.09616), and GTA-tier procedural world synthesis (MultiGen
 * arXiv:2603.06679).
 *
 * v73 σ-Omnimodal is the *Creator*: a single branchless integer-only
 * C kernel that ships ten primitives covering the 2024-2026 generative
 * frontier, expressed in the Creation OS discipline — 256-bit HV
 * tokens, popcount-Hamming coherence, bipolar XOR binding, 64-byte-
 * aligned arenas, libc-only hot path, zero floating-point on any
 * decision surface.  Code, image, audio, video, game-world, and
 * workflow outputs all share the same HV token alphabet, so cross-
 * modal coherence becomes a single Hamming compare, not a framework
 * stack.
 *
 * ------------------------------------------------------------------
 *  Ten primitives (the 2024-2026 generative frontier)
 * ------------------------------------------------------------------
 *
 *   1.  Universal modality tokenizer (VQ-RVQ lineage)
 *       — SoundStream arXiv:2107.03312, MAGVIT arXiv:2212.05199,
 *       MAGVIT-v2 arXiv:2310.05737, Encodec, MSR-Codec 2024.  Any
 *       modality is quantised to a 256-bit bipolar HV token via
 *       branchless popcount-nearest codeword lookup; code, image,
 *       audio, video, 3D, workflow-node all speak the same alphabet.
 *
 *   2.  Rectified-flow integer scheduler
 *       — Flow Matching arXiv:2209.03003 + Rectified Flow arXiv:
 *       2210.02747 + StreamFlow arXiv:2511.22009 + MixFlow arXiv:
 *       2604.09181 + Variational RFM arXiv:2502.09616.  Integer K-step
 *       noise schedule; branchless per-step budget compare; fixed
 *       number of evaluations per generation regardless of prompt.
 *
 *   3.  VINO-style cross-modal binding (arXiv:2601.02358)
 *       — Interleaved multimodal conditioning via HV XOR-bind:
 *       `cond = text_hv ⊕ permute(image_hv, 1) ⊕ permute(video_hv, 2)
 *       ⊕ permute(code_hv, 3)` with per-reference permutation so
 *       identity is preserved across modalities.
 *
 *   4.  MOVA-style video+audio co-synth lock
 *       — Paired bipolar popcount across a shared reel: lip-sync,
 *       FX-sync, and music-sync are all single Hamming-window compares
 *       (lip_lag ≤ Δ AND fx_coherence ≥ τ AND music_tempo_err ≤ τ').
 *
 *   5.  Matrix-Game world-model substrate
 *       — Matrix-Game-3 arXiv:2604.08995 (720p / 40 FPS, 5B), Matrix-
 *       Game-2 arXiv:2508.13009 (25 FPS, GTA5 scenes, ICLR 2025),
 *       GameNGen (ICLR 2025, 20 FPS single-TPU).  Action-conditioned
 *       next-HV-frame step with camera-aware memory retrieval by
 *       popcount-Hamming over a tile grid.
 *
 *   6.  DiReCT-lineage physics-coherence gate (arXiv:2603.25931)
 *       — Disentangled semantic-physics regulariser expressed as a
 *       branchless integer energy compare: `|Δmomentum| ≤ τ_p AND
 *       collision_energy ≤ τ_c`.  Rejects frames that violate
 *       conservation.
 *
 *   7.  Workflow-DAG executor (n8n v2.10 "Brain vs Hands")
 *       — JudgeFlow arXiv:2601.07477 rank-responsibility scoring,
 *       ReusStdFlow extraction-storage-construction cache, SOAN
 *       arXiv:2508.13732v1 structural-unit encapsulation.  Topological
 *       sort via bitset reachability; trigger bitmask + action
 *       dispatch; every edge carries an HV tag for cross-workflow
 *       deduplication.
 *
 *   8.  Code-edit planner (Cursor / Claude Code / Devin / Lovable /
 *       Bolt / Base44 / Totalum / v0 / Replit Agent lineage; 2026
 *       ai-agents-benchmark.com Autonomous Software Creation rank)
 *       — Multi-file diff plan over an integer bitset dependency
 *       graph; idempotent sub-workflow verifier; bounded repair-loop
 *       counter.
 *
 *   9.  GTA-tier procedural asset graph (MultiGen arXiv:2603.06679)
 *       — External-memory-persistent tile grid + HV-bound asset DAG;
 *       small-world navigation reuses v71 portal-table shape (Kleinberg
 *       STOC 2000); every tile carries an HV tag so cross-player and
 *       cross-session coherence is a single Hamming compare.
 *
 *  10.  OML — Omnimodal Language — 10-opcode integer bytecode ISA.
 *       Cost model (creation-units):
 *           HALT      = 1
 *           TOKENIZE  = 2
 *           FLOW      = 8        (one K-step flow-matching evaluation)
 *           BIND      = 2        (VINO cross-modal XOR bind)
 *           LOCK      = 4        (MOVA AV lock verify)
 *           WORLD     = 16       (Matrix-Game world step)
 *           PHYSICS   = 4        (DiReCT energy compare)
 *           WORKFLOW  = 4        (n8n DAG step dispatch)
 *           PLAN      = 8        (Cursor-style code edit plan)
 *           GATE      = 1
 *
 *       GATE writes `v73_ok = 1` iff:
 *           creation_units   ≤ budget
 *           AND flow_ok      == 1     (scheduler closed within steps)
 *           AND bind_ok      == 1     (cross-modal tag matches)
 *           AND lock_ok      == 1     (MOVA AV sync verified)
 *           AND physics_ok   == 1     (DiReCT energy compare passed)
 *           AND plan_ok      == 1     (code-edit plan verified)
 *           AND workflow_ok  == 1     (DAG terminated, no cycle)
 *           AND abstained    == 0
 *
 * ------------------------------------------------------------------
 *  Composed 14-bit branchless decision
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
 *   v73 σ-Omnimodal      — the Creator: does the generated artefact
 *                          clear flow + bind + lock + physics + plan +
 *                          workflow + creation-unit budget?
 *
 * `cos_v73_compose_decision` is a single 14-way branchless AND.
 * Nothing — no inference, tool call, sealed message, retrieval, write,
 * route, hyperscale step, teleport, chain emission, or generated
 * artefact (code, image, audio, video, world-frame, workflow output)
 * crosses to the agent unless every one of the fourteen kernels ALLOWs.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline
 * ------------------------------------------------------------------
 *
 *   - HV token: 256 bits = 4 × uint64 = 32 B; naturally aligned.
 *   - Token codebook arena: 64-byte aligned via `aligned_alloc`.
 *   - Tokenizer inner loop is popcount + XOR over a fixed codebook
 *     size; constant-time per query.
 *   - Flow-matching schedule is a compile-time integer table; no
 *     data-dependent branch on the noise bytes.
 *   - Cross-modal bind is a fixed four-way XOR with a compile-time
 *     permutation; no syscalls.
 *   - AV-sync verify is three branchless integer compares ANDed.
 *   - World-model step is a popcount-Hamming memory retrieval +
 *     single XOR action injection; deterministic fixed time.
 *   - Physics gate is two branchless integer compares ANDed.
 *   - Workflow DAG uses a bitset adjacency matrix; topological sort
 *     is a fixed-depth wave-front with branchless pop counts.
 *   - Code-plan verifier is a bounded-depth bitset DFS with explicit
 *     depth budget.
 *   - 14-bit composed decision is a single branchless AND.
 *
 * 1 = 1.
 */

#ifndef COS_V73_OMNIMODAL_H
#define COS_V73_OMNIMODAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Primitive types and constants.
 * ==================================================================== */

#define COS_V73_HV_WORDS        4u          /* 4 × uint64 = 256 bits */
#define COS_V73_HV_BYTES        32u
#define COS_V73_HV_BITS         256u

typedef struct {
    uint64_t w[COS_V73_HV_WORDS];
} cos_v73_hv_t;

/* Limits (compile-time). */
#define COS_V73_CODEBOOK_MAX      4096u     /* max codewords per tokenizer */
#define COS_V73_FLOW_STEPS_MAX      64u     /* max K steps flow-matching */
#define COS_V73_REEL_MAX          4096u     /* AV reel length (frames)   */
#define COS_V73_WORLD_TILES_MAX  16384u     /* Matrix-Game tile grid cap */
#define COS_V73_WORKFLOW_NODES_MAX  128u    /* DAG nodes cap             */
#define COS_V73_PLAN_FILES_MAX     128u     /* code-plan files cap       */
#define COS_V73_NREGS               16u     /* OML register file         */
#define COS_V73_PROG_MAX          1024u     /* OML program cap           */

/* Modality id (used for cross-modal permutation). */
typedef enum {
    COS_V73_MOD_TEXT   = 0,
    COS_V73_MOD_IMAGE  = 1,
    COS_V73_MOD_AUDIO  = 2,
    COS_V73_MOD_VIDEO  = 3,
    COS_V73_MOD_CODE   = 4,
    COS_V73_MOD_3D     = 5,
    COS_V73_MOD_WF     = 6,   /* workflow node */
    COS_V73_MOD_GAME   = 7,   /* game-world tile */
} cos_v73_modality_t;

/* ====================================================================
 *  HV primitives — XOR bind / permute / popcount-Hamming.
 * ==================================================================== */

void cos_v73_hv_zero  (cos_v73_hv_t *dst);
void cos_v73_hv_copy  (cos_v73_hv_t *dst, const cos_v73_hv_t *src);
void cos_v73_hv_xor   (cos_v73_hv_t *dst, const cos_v73_hv_t *a, const cos_v73_hv_t *b);
void cos_v73_hv_from_seed(cos_v73_hv_t *dst, uint64_t seed);

/* Modality-keyed permutation: rotates + XORs with a modality salt.
 * Deterministic, branchless, constant-time. */
void cos_v73_hv_permute(cos_v73_hv_t *dst,
                        const cos_v73_hv_t *src,
                        cos_v73_modality_t m);

/* Popcount-Hamming distance (0..256). */
uint32_t cos_v73_hv_hamming(const cos_v73_hv_t *a, const cos_v73_hv_t *b);

/* ====================================================================
 *  1.  Universal modality tokenizer (VQ / RVQ lineage).
 *
 *  A codebook is an array of 256-bit HV codewords.  The tokenizer
 *  returns the nearest codeword index by popcount-Hamming over all
 *  entries (branchless scan).  Residual quantisation is encoded as a
 *  second-stage pass over the XOR residual.
 * ==================================================================== */

typedef struct {
    cos_v73_hv_t *book;          /* n × 32 B, 64-B aligned */
    uint32_t      n;             /* ≤ COS_V73_CODEBOOK_MAX */
    uint32_t      modality;      /* cos_v73_modality_t     */
} cos_v73_codebook_t;

cos_v73_codebook_t *cos_v73_codebook_new(uint32_t n,
                                         cos_v73_modality_t m,
                                         uint64_t           seed);
void                cos_v73_codebook_free(cos_v73_codebook_t *cb);

/* Nearest-codeword index.  Writes quantised HV to `out` if non-NULL. */
uint32_t cos_v73_tokenize(const cos_v73_codebook_t *cb,
                          const cos_v73_hv_t       *q,
                          cos_v73_hv_t             *out);

/* Residual tokenise: returns stage-1 idx, writes stage-2 idx into `out_rvq`
 * (via a second scan against the same codebook on the residual XOR). */
uint32_t cos_v73_tokenize_rvq(const cos_v73_codebook_t *cb,
                              const cos_v73_hv_t       *q,
                              uint32_t                 *out_rvq,
                              cos_v73_hv_t             *out_reconstruction);

/* ====================================================================
 *  2.  Rectified-flow integer scheduler.
 *
 *  K-step integer noise schedule with per-step budget.  Deterministic:
 *  same seed + same steps → same output HV.  Used as the scheduler
 *  layer above TOKENIZE for all modalities (image, audio, video, code).
 * ==================================================================== */

typedef struct {
    uint32_t      steps;         /* ≤ COS_V73_FLOW_STEPS_MAX */
    uint32_t      budget;
    uint32_t      _pad0;
    uint32_t      _pad1;
    uint64_t      sched[COS_V73_FLOW_STEPS_MAX];  /* per-step noise mask */
} cos_v73_flow_t;

void cos_v73_flow_init(cos_v73_flow_t *f, uint32_t steps, uint32_t budget, uint64_t seed);

/* Run the schedule over a conditioning HV.  Produces a deterministic
 * output HV.  Returns 1 iff `used_units ≤ budget`. */
uint8_t cos_v73_flow_run(const cos_v73_flow_t *f,
                         const cos_v73_hv_t   *cond,
                         cos_v73_hv_t         *out,
                         uint32_t             *used_units);

/* ====================================================================
 *  3.  VINO cross-modal binding.
 *
 *  Interleaved XOR-bind of up to four modalities with per-modality
 *  permutation (identity preservation trick in VSA / HRR lineage).
 *  Returns the bound conditioning HV.
 * ==================================================================== */

typedef struct {
    const cos_v73_hv_t *hv;
    cos_v73_modality_t  mod;
} cos_v73_mod_input_t;

void cos_v73_bind(cos_v73_hv_t              *out,
                  const cos_v73_mod_input_t *inputs,
                  uint32_t                   n);

/* Verify cross-modal binding integrity against a claim HV: returns 1
 * iff hamming(bind(inputs), claim) ≤ tol. */
uint8_t cos_v73_bind_verify(const cos_v73_mod_input_t *inputs,
                            uint32_t                   n,
                            const cos_v73_hv_t        *claim,
                            uint32_t                   tol);

/* ====================================================================
 *  4.  MOVA video+audio co-synth lock.
 *
 *  For each frame, compute a paired bipolar popcount between the video
 *  HV and the audio HV, and check:
 *      lip_lag         ≤ lip_lag_max
 *      fx_coherence    ≥ fx_thresh        (low Hamming == high coherence)
 *      music_tempo_err ≤ music_tempo_max
 *
 *  All three are integer compares ANDed branchlessly.
 * ==================================================================== */

typedef struct {
    const cos_v73_hv_t *video;    /* reel_len × HV */
    const cos_v73_hv_t *audio;    /* reel_len × HV */
    uint32_t            reel_len;
    uint32_t            lip_lag_max;
    uint32_t            fx_thresh;      /* MAX hamming to accept */
    uint32_t            tempo_err_max;
} cos_v73_av_lock_t;

uint8_t cos_v73_av_lock_verify(const cos_v73_av_lock_t *lock,
                               uint32_t                *out_lip_lag,
                               uint32_t                *out_fx_worst,
                               uint32_t                *out_tempo_err);

/* ====================================================================
 *  5.  Matrix-Game world-model substrate.
 *
 *  A tile grid holds HV tags per tile; an action is an HV (key / mouse
 *  encoded).  Next-frame step:
 *      new_tag = current_tag ⊕ action ⊕ perm(camera_tag, mod_GAME)
 *  with popcount-Hamming memory retrieval for long-horizon coherence
 *  (Matrix-Game-3).
 * ==================================================================== */

typedef struct {
    cos_v73_hv_t *tiles;         /* n_tiles × HV, 64-B aligned */
    uint32_t      n_tiles;
    uint32_t      camera_idx;    /* current camera tile        */
    cos_v73_hv_t  camera_tag;
    cos_v73_hv_t  memory_digest; /* XOR bundle of visited tiles */
} cos_v73_world_t;

cos_v73_world_t *cos_v73_world_new(uint32_t n_tiles, uint64_t seed);
void             cos_v73_world_free(cos_v73_world_t *w);

/* Step: apply action → new tile tag; updates camera + memory digest. */
void cos_v73_world_step(cos_v73_world_t      *w,
                        const cos_v73_hv_t   *action,
                        uint32_t              next_camera_idx);

/* Retrieval: find best-matching tile by popcount-Hamming to query. */
uint32_t cos_v73_world_retrieve(const cos_v73_world_t *w,
                                const cos_v73_hv_t    *q,
                                uint32_t              *out_dist);

/* ====================================================================
 *  6.  DiReCT physics-coherence gate.
 *
 *  Integer energy compare: |Δmomentum| ≤ τ_p AND collision_energy ≤ τ_c.
 *  Caller provides integer energies (Q0.15 or raw).
 * ==================================================================== */

uint8_t cos_v73_physics_gate(int32_t  delta_momentum,
                             int32_t  momentum_tol,
                             uint32_t collision_energy,
                             uint32_t collision_tol);

/* ====================================================================
 *  7.  Workflow DAG executor (n8n + JudgeFlow + SOAN + ReusStdFlow).
 *
 *  Nodes carry an HV tag; edges form an adjacency bitset matrix
 *  (n × ceil(n/64) words).  Executor does a branchless topological
 *  wave: frontier bitset, mark-done bitset, per-round decrement.
 * ==================================================================== */

typedef struct {
    uint32_t      n;             /* ≤ COS_V73_WORKFLOW_NODES_MAX */
    uint32_t      row_words;     /* = ceil(n/64) */
    uint64_t     *adj;           /* n × row_words, 64-B aligned */
    cos_v73_hv_t *tags;          /* n × HV, 64-B aligned */
    uint64_t     *ready;         /* row_words bitset                */
    uint64_t     *done;          /* row_words bitset                */
    uint32_t      faults;        /* running count of failed nodes   */
    uint32_t      _pad;
} cos_v73_workflow_t;

cos_v73_workflow_t *cos_v73_workflow_new(uint32_t n_nodes);
void                cos_v73_workflow_free(cos_v73_workflow_t *wf);
void                cos_v73_workflow_add_edge(cos_v73_workflow_t *wf,
                                              uint32_t from, uint32_t to);
void                cos_v73_workflow_set_tag(cos_v73_workflow_t *wf,
                                             uint32_t i,
                                             const cos_v73_hv_t *tag);

/* Execute: returns 1 iff all nodes terminated within `max_rounds`
 * without a cycle, with `faults ≤ fault_budget`.
 * Writes JudgeFlow-style per-node responsibility score (higher = more
 * blame) into `score` (may be NULL). */
uint8_t cos_v73_workflow_execute(cos_v73_workflow_t *wf,
                                 uint32_t            max_rounds,
                                 uint32_t            fault_budget,
                                 uint32_t           *score);

/* ====================================================================
 *  8.  Code-edit planner (Cursor / Devin / Lovable / Bolt lineage).
 *
 *  Multi-file plan: each plan step is (file_idx, hv_diff_tag).  The
 *  verifier:
 *      - checks every file_idx is within bounds
 *      - checks no cyclic dep in the dep-bitset
 *      - checks repair-loop count ≤ repair_budget
 *      - XORs all diff tags → manifest_hv; compares against claim
 *
 *  All branchless.
 * ==================================================================== */

typedef struct {
    uint32_t      n_files;
    uint32_t      _pad;
    uint64_t     *dep;           /* n_files × row_words, 64-B aligned */
    uint32_t      row_words;
} cos_v73_plan_graph_t;

typedef struct {
    uint32_t      file_idx;
    uint32_t      repair_count;
    cos_v73_hv_t  diff_tag;
} cos_v73_plan_step_t;

cos_v73_plan_graph_t *cos_v73_plan_new(uint32_t n_files);
void                  cos_v73_plan_free(cos_v73_plan_graph_t *g);
void                  cos_v73_plan_add_dep(cos_v73_plan_graph_t *g,
                                           uint32_t from, uint32_t to);

uint8_t cos_v73_plan_verify(const cos_v73_plan_graph_t *g,
                            const cos_v73_plan_step_t  *steps,
                            uint32_t                    n_steps,
                            uint32_t                    repair_budget,
                            const cos_v73_hv_t         *manifest_claim,
                            uint32_t                    manifest_tol);

/* ====================================================================
 *  9.  GTA-tier asset-graph (MultiGen lineage).
 *
 *  A tile grid of HV-bound assets with small-world Hamming descent.
 *  Navigate from anchor to target in ≤ hop_budget hops, writing path-
 *  coherence HV.
 * ==================================================================== */

uint8_t cos_v73_asset_navigate(const cos_v73_world_t *w,
                               const cos_v73_hv_t    *anchor,
                               const cos_v73_hv_t    *target,
                               uint32_t               hop_budget,
                               cos_v73_hv_t          *out_path_hv,
                               uint32_t              *out_hops);

/* ====================================================================
 * 10.  OML — Omnimodal Language — 10-opcode integer bytecode ISA.
 *
 *  (see cost model in file-level comment)
 * ==================================================================== */

typedef enum {
    COS_V73_OP_HALT     = 0,
    COS_V73_OP_TOKENIZE = 1,
    COS_V73_OP_FLOW     = 2,
    COS_V73_OP_BIND     = 3,
    COS_V73_OP_LOCK     = 4,
    COS_V73_OP_WORLD    = 5,
    COS_V73_OP_PHYSICS  = 6,
    COS_V73_OP_WORKFLOW = 7,
    COS_V73_OP_PLAN     = 8,
    COS_V73_OP_GATE     = 9,
} cos_v73_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v73_oml_inst_t;

typedef struct {
    const cos_v73_codebook_t    *cb;
    const cos_v73_hv_t          *tok_input;
    const cos_v73_flow_t        *flow;
    const cos_v73_hv_t          *flow_cond;
    const cos_v73_mod_input_t   *bind_inputs;
    uint32_t                     bind_n;
    const cos_v73_hv_t          *bind_claim;
    uint32_t                     bind_tol;
    const cos_v73_av_lock_t     *lock;
    cos_v73_world_t             *world;
    const cos_v73_hv_t          *world_action;
    uint32_t                     world_next_cam;
    int32_t                      phys_dmom;
    int32_t                      phys_dmom_tol;
    uint32_t                     phys_ce;
    uint32_t                     phys_ce_tol;
    cos_v73_workflow_t          *workflow;
    uint32_t                     workflow_rounds;
    uint32_t                     workflow_faults;
    const cos_v73_plan_graph_t  *plan_graph;
    const cos_v73_plan_step_t   *plan_steps;
    uint32_t                     plan_n;
    uint32_t                     plan_repair;
    const cos_v73_hv_t          *plan_claim;
    uint32_t                     plan_tol;
    uint8_t                      abstain;
} cos_v73_oml_ctx_t;

typedef struct cos_v73_oml_state cos_v73_oml_state_t;

cos_v73_oml_state_t *cos_v73_oml_new(void);
void                 cos_v73_oml_free(cos_v73_oml_state_t *s);
void                 cos_v73_oml_reset(cos_v73_oml_state_t *s);

uint64_t cos_v73_oml_units  (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_flow_ok    (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_bind_ok    (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_lock_ok    (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_physics_ok (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_plan_ok    (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_workflow_ok(const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_abstained  (const cos_v73_oml_state_t *s);
uint8_t  cos_v73_oml_v73_ok     (const cos_v73_oml_state_t *s);

int cos_v73_oml_exec(cos_v73_oml_state_t       *s,
                     const cos_v73_oml_inst_t  *prog,
                     uint32_t                   n,
                     cos_v73_oml_ctx_t         *ctx,
                     uint64_t                   unit_budget);

/* ====================================================================
 *  Composed 14-bit branchless decision.
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
    uint8_t allow;
    uint8_t _pad;
} cos_v73_decision_t;

cos_v73_decision_t cos_v73_compose_decision(uint8_t v60_ok,
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
                                            uint8_t v73_ok);

/* ====================================================================
 *  Version string.
 * ==================================================================== */

extern const char cos_v73_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V73_OMNIMODAL_H */
