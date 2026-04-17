/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v71 — σ-Wormhole (hyperdimensional wormhole /
 *                                portal-cognition / direct-addressing
 *                                substrate for AGI-scale retrieval,
 *                                routing and consolidation).
 *
 * Every kernel up to v70 is *local*: σ-Shield gates the action,
 * Σ-Citadel the lattice, Reasoning verifies energy, Cipher the
 * envelope, Intellect the MCTS, Hypercortex the cortex, Silicon the
 * matrix, Noesis the deliberation, Mnemos the memory, Constellation
 * the fleet, Hyperscale the substrate.  What is still missing is a
 * *non-local* primitive — a way to jump from "here" to "there" in the
 * concept manifold in O(1) wordwise operations, *without* walking the
 * transformer, the MCTS tree, the debate council, or the retrieval
 * graph step-by-step.  Every frontier lab (OpenAI o-series beam,
 * DeepMind AlphaProof cascade, Anthropic Claude multi-turn, xAI Grok
 * long context, Meta / Microsoft / Google) serialises *every* thought
 * through O(depth) layers of matmul.  The biological cortex does not:
 * hippocampal replay, schema-guided retrieval, System-1 intuition and
 * REM consolidation all *jump* the state through long-range bridges
 * — pattern completion from a fragment, one shot.
 *
 * The 2024-2026 theoretical and engineering corpus converged on ten
 * independent findings that together give us the primitives of a
 * *wormhole plane* for a local AI agent — ten branchless integer
 * kernels that compose with v60..v70 through a 12-bit AND:
 *
 *   1.  Vector-Symbolic / Holographic Reduced Representations
 *       (Plate 1995; Kanerva 2009; Gayler 2003) — bipolar
 *       hypervectors whose binding operator is XOR, which is
 *       involutive.  Given any source HV `s` and any target HV `t`,
 *       the *canonical bridge* is `b = s ⊕ t`; applying `b` to `s`
 *       produces exactly `t`:
 *
 *           t = s ⊕ b  =  s ⊕ s ⊕ t  =  t
 *
 *       That is a *wormhole*: a single XOR lands the state on a
 *       distant target with zero interpolation, zero latency, zero
 *       multiplies.  The only cost is the one-time XOR pass.
 *
 *   2.  ER = EPR (Maldacena & Susskind 2013) — every pair of
 *       entangled states is connected by a Planck-scale wormhole.
 *       Translated into VSA: every *bound* pair (s,t) shares a
 *       bridge HV `b = s ⊕ t`; storing that bridge in a portal table
 *       *is* the wormhole, materialised as 2 KiB of bits in cache-
 *       aligned memory.  The ER=EPR analogy is not physical here —
 *       it is the *information-theoretic* short circuit that the
 *       VSA algebra already gives us for free.
 *
 *   3.  Kleinberg small-world routing (Kleinberg, STOC 2000) — a
 *       grid augmented with O(log n) random long-range edges per
 *       node admits greedy routing in O(log² n) hops, provided the
 *       long-range edges follow a 1/d-distance distribution.  We
 *       seed portal anchors accordingly and greedy-route by Hamming
 *       distance: pick the portal whose anchor is closest to the
 *       destination, teleport, repeat.  Worst-case hop budget is
 *       explicit and branchless.
 *
 *   4.  Modern Hopfield / attention-as-Hopfield (Ramsauer *et al.*,
 *       ICLR 2021; Hopfield 1982; Krotov & Hopfield 2016) —
 *       associative retrieval is exponential-capacity in the
 *       modern Hopfield form and is *equivalent to one layer of
 *       attention*.  A cleanup memory over labelled HVs with a
 *       constant-time popcount sweep is the integer-only version
 *       of that retrieval: O(cap · D/64) popcount ops, one-shot
 *       pattern completion.
 *
 *   5.  Fast Weight Programmers / Linear Transformers (Schmidhuber
 *       1991; Schlag, Irie & Schmidhuber, ICML 2021) — transformer
 *       attention is a "fast weight" outer-product memory; a
 *       wormhole bridge is exactly such a fast weight, written
 *       once (at portal insert) and read with one XOR on every
 *       subsequent query.  No gradient step, no retraining.
 *
 *   6.  Hyperbolic / Poincaré embeddings (Nickel & Kiela, NeurIPS
 *       2017; Bronstein *et al.* 2021) — negative-curvature spaces
 *       have exponentially more "room" near the boundary than near
 *       the centre.  Concept hierarchies stored at the boundary
 *       admit exponentially more wormhole shortcuts across them.
 *       We use a branchless bit-skew proxy — `|popcount(hv) − D/2|`
 *       — as a cheap "are we near the boundary?" gate that decides
 *       whether teleportation is authorised for this query.
 *
 *   7.  Hyperdimensional retrieval at scale (HDC / VSA ARC-AGI
 *       arXiv:2511.08747; VaCoAl multi-hop Wikidata
 *       arXiv:2604.11665; Attention-as-Binding AAAI 2026;
 *       Holographic Invariant Storage arXiv:2603.13558) — multi-hop
 *       traversal, analogy and role-filler recovery all reduce to
 *       XOR-bind + cleanup + bundle.  A "portal" is thus a first-
 *       class object in this algebra, not a heuristic.
 *
 *   8.  Cryptographic integrity on the bridge (HMAC-SHA /
 *       construction of Bellare-Canetti-Krawczyk 1996; HD-crypto
 *       VSA-HMAC sketches) — because every bridge is a bit-string,
 *       we can bind it with a secret permutation of a session key
 *       HV to produce a signature HV.  Verify reduces to one
 *       popcount-Hamming compare.  This defeats the "poisoned
 *       portal" attack class where an adversary injects a bridge
 *       that teleports *outside* the legal lattice.
 *
 *   9.  Hippocampal replay and schema consolidation (Diekelmann &
 *       Born 2010; Tolman 1948; Tse *et al.* 2007; Ji & Wilson
 *       Nat. Neurosci. 2007) — the biological brain does not
 *       reason step-by-step at recall time; it pattern-completes
 *       from fragments via long-range, sleep-consolidated bridges
 *       between distant schemas.  v71 gives the agent the
 *       *algorithmic* form of that: pre-computed bridges between
 *       pairs of concept HVs that let System-1 intuition short-
 *       circuit System-2 deliberation whenever the gate permits.
 *
 *  10.  Multi-hop path receipts — every teleport sequence produces
 *       a bundled HV that uniquely identifies the path taken and
 *       is recoverable with cleanup.  This is the AlphaFold-3-
 *       /AlphaProof-style "show your work" discipline applied to
 *       non-local jumps: even though no individual hop was
 *       expensive, the entire path is audit-able after the fact.
 *       The receipt HV is the wormhole version of a proof object.
 *
 * Ten primitives, one composition:
 *
 *      1. Portal table (Einstein-Rosen bridges, XOR-symmetric)
 *      2. Anchor match (constant-time cleanup over portal anchors)
 *      3. Teleport (single-XOR direct-address jump)
 *      4. Small-world routing (Kleinberg greedy + multi-hop)
 *      5. Tensor-bond wormhole (ER=EPR pairing between two stores)
 *      6. Integrity verify (HMAC-style bridge signature)
 *      7. Hyperbolic-depth gate (boundary-regime Poincaré proxy)
 *      8. Multi-hop hop budget + abstain
 *      9. Path receipt (bundled HV of hop anchors)
 *     10. WHL — Wormhole Language 10-opcode integer ISA
 *
 * Composition (12-bit branchless decision):
 *
 *   v60 σ-Shield         — was the action allowed?
 *   v61 Σ-Citadel        — does the data flow honour BLP + Biba?
 *   v62 Reasoning        — is the EBT energy below budget?
 *   v63 σ-Cipher         — is the AEAD tag authentic + quote bound?
 *   v64 σ-Intellect      — does the agentic MCTS authorise emit?
 *   v65 σ-Hypercortex    — is the thought on-manifold + under HVL cost?
 *   v66 σ-Silicon        — does the matrix path clear conformal +
 *                          MAC budget?
 *   v67 σ-Noesis         — does the deliberation close + receipts?
 *   v68 σ-Mnemos         — does recall ≥ threshold AND forget ≤ budget?
 *   v69 σ-Constellation  — does quorum margin ≥ threshold AND
 *                          Byzantine faults ≤ budget?
 *   v70 σ-Hyperscale     — does the silicon-substrate path clear
 *                          silicon budget AND throughput floor AND
 *                          topology balance?
 *   v71 σ-Wormhole       — does the teleport arrive on the target
 *                          manifold (sim ≥ arrival) AND under the
 *                          hop budget AND with a valid integrity
 *                          signature AND within the Poincaré-
 *                          boundary regime AND not abstained?
 *
 * `cos_v71_compose_decision` is a single 12-way branchless AND.
 * Nothing — no inference, tool call, sealed message, retrieval,
 * write, route, hyperscale step, or teleport — crosses to the agent
 * unless every one of the twelve kernels ALLOWs.
 *
 * Hardware discipline (AGENTS.md / .cursorrules / M4 invariants):
 *
 *   - D = 8 192 bits = 1 024 B = 16 × 64-byte cache lines; half the
 *     Hypercortex HV, so portal tables stay dense.  We keep the
 *     *word count* `D/64 = 128` as the inner popcount loop length.
 *     Every hypervector is naturally aligned to 64-byte boundaries.
 *   - Hamming via `__builtin_popcountll` over 128 × uint64_t words;
 *     on AArch64 this lowers to NEON `cnt` + horizontal add.  No
 *     floating point, ever, on the hot path.
 *   - All arenas `aligned_alloc(64, …)`; the portal table, path-
 *     receipt bundle, integrity secret, scratch HV, WHL register
 *     file and HSL-style cost counter are all 64-byte aligned at
 *     construction; the hot path never allocates.
 *   - Teleport is one XOR pass + one popcount.  Routing is greedy
 *     over Hamming distance — constant-time per portal (no early
 *     exit) so side-channel timing leaks only `cap`, not which
 *     portal matched.
 *   - The 12-bit composed decision is a single branchless AND; no
 *     allow-listed path can be reached unless every kernel agrees.
 *   - The Poincaré-boundary gate is a single integer compare on the
 *     signed skew `|popcount(hv) - D/2|`; no exp, no div, no FP.
 *
 * Tier semantics (v57 dialect):
 *   M  this kernel runs and is checked at runtime by `make check-v71`.
 *   I  Fast-weight / modern-Hopfield references are interpretive
 *      (they share the same XOR-bind + cleanup surface; no new
 *      silicon claim is made beyond the v65/v68 tier bases).
 *   P  Hyperbolic / CHERI / memristor-CAM fast paths for portal
 *      match are *planned* compile-only targets — documented, not
 *      claimed.
 *
 * v71 ships M-tier for all ten primitives + the 12-bit composition.
 * No silent downgrade path.  Zero runtime dependencies on libsodium,
 * liboqs, Core ML, or any toolchain beyond libc on the hot path.
 *
 * 1 = 1.
 */

#ifndef COS_V71_WORMHOLE_H
#define COS_V71_WORMHOLE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Fixed-size hypervector (bipolar, bit-packed).
 *
 *  D = 8 192 bits = 1 024 bytes = 16 × 64-byte cache lines.  Bind is
 *  XOR (involutive); similarity is (D − 2·Hamming) in Q0.15 units.
 *  Kept smaller than the v65 cortex HV (D = 16 384) so that portal
 *  tables of 1 024+ entries still fit comfortably in L2.
 * ==================================================================== */

#define COS_V71_HV_DIM_BITS   8192u
#define COS_V71_HV_DIM_BYTES  1024u
#define COS_V71_HV_DIM_WORDS  (COS_V71_HV_DIM_BITS / 64u)  /* = 128 */

typedef struct {
    uint64_t w[COS_V71_HV_DIM_WORDS];
} cos_v71_hv_t;

/* Constants. */
#define COS_V71_PORTAL_CAP_MAX        4096u   /* portal table hard cap */
#define COS_V71_ROUTE_HOPS_MAX          32u   /* multi-hop ceiling     */
#define COS_V71_NREGS                   16u   /* WHL register file     */
#define COS_V71_PROG_MAX              1024u   /* WHL program cap       */

/* Default thresholds (Q0.15). */
#define COS_V71_ARRIVAL_DEFAULT_Q15   ((int32_t)22938)  /* ~0.70 arrival */
#define COS_V71_BOUNDARY_DEFAULT_BITS ((uint32_t)1024u) /* ≥1 024-bit skew */

/* ====================================================================
 *  1.  Primitive HV operations.
 *
 *  All branchless on the inner loop; integer only; caller-owned
 *  buffers.  Mirrors the v65 primitives but at the v71 D = 8 192 bit
 *  dimension — v71 does NOT link against v65 (each kernel is self-
 *  contained; composition happens only at the `_compose_decision`
 *  API surface).
 * ==================================================================== */

void cos_v71_hv_zero(cos_v71_hv_t *dst);
void cos_v71_hv_copy(cos_v71_hv_t *dst, const cos_v71_hv_t *src);
void cos_v71_hv_from_seed(cos_v71_hv_t *dst, uint64_t seed);
void cos_v71_hv_xor(cos_v71_hv_t       *dst,
                    const cos_v71_hv_t *a,
                    const cos_v71_hv_t *b);

/* Cyclic permutation by `shift` bit positions; used as the
 * integrity-signature "permutation of the secret key". */
void cos_v71_hv_permute(cos_v71_hv_t       *dst,
                        const cos_v71_hv_t *src,
                        int32_t             shift);

/* popcount across all 128 words. */
uint32_t cos_v71_hv_popcount(const cos_v71_hv_t *a);

/* Hamming distance = popcount(a ^ b). */
uint32_t cos_v71_hv_hamming(const cos_v71_hv_t *a,
                            const cos_v71_hv_t *b);

/* Similarity in Q0.15 ∈ [−32768, +32768]:
 *      sim = (D − 2·Hamming) · (32768 / D)
 * For D = 8 192 this is (D − 2·H) · 4.  Integer. */
int32_t cos_v71_hv_similarity_q15(const cos_v71_hv_t *a,
                                  const cos_v71_hv_t *b);

/* ====================================================================
 *  2.  Portal table — Einstein-Rosen bridges.
 *
 *      bridge[i]  = anchor[i] ⊕ target[i]
 *      sig[i]     = bridge[i] ⊕ permute(secret, (int32_t)seed[i])
 *
 *  Apply:  t_recovered = anchor[i] ⊕ bridge[i]   (== target[i])
 *
 *  Because XOR is involutive, the bridge is *bidirectional*: the same
 *  bridge also lands `target[i] ⊕ bridge[i] == anchor[i]`.
 *
 *  Insertion is O(1) + three HV writes (anchor, bridge, sig).  All
 *  storage is `aligned_alloc(64, …)`; the lookup sweep is constant-
 *  time per capacity (side-channel bounded by `cap`, not by which
 *  portal matched).
 * ==================================================================== */

typedef struct {
    cos_v71_hv_t *anchors;      /* cap × 1 024 B, 64-byte aligned */
    cos_v71_hv_t *bridges;      /* cap × 1 024 B */
    cos_v71_hv_t *sigs;         /* cap × 1 024 B */
    uint64_t     *seeds;        /* cap × 8 B  — per-portal signature seed */
    uint32_t     *labels;       /* cap × 4 B  — caller-chosen label */
    uint32_t      cap;
    uint32_t      len;
    cos_v71_hv_t *secret;       /* 1 × 1 024 B — signing key (caller-owned) */
} cos_v71_portal_t;

cos_v71_portal_t *cos_v71_portal_new(uint32_t cap,
                                     const cos_v71_hv_t *secret);
void              cos_v71_portal_free(cos_v71_portal_t *p);

/* Insert a new (anchor, target) wormhole.  The bridge is computed as
 * `anchor ⊕ target`; the signature is `bridge ⊕ permute(secret, seed)`
 * with `seed = (len << 11) ^ label` (deterministic, per-entry).
 * Returns the new slot index, or -1 on overflow. */
int cos_v71_portal_insert(cos_v71_portal_t   *p,
                          const cos_v71_hv_t *anchor,
                          const cos_v71_hv_t *target,
                          uint32_t            label);

/* ====================================================================
 *  3.  Anchor match — constant-time cleanup over portal anchors.
 *
 *  Returns the portal index whose anchor is closest in Hamming to `q`,
 *  or UINT32_MAX if the table is empty.  The sweep never exits early;
 *  `sim_q15` receives the Q0.15 similarity of the winner.
 * ==================================================================== */

uint32_t cos_v71_portal_match(const cos_v71_portal_t *p,
                              const cos_v71_hv_t     *q,
                              int32_t                *out_sim_q15);

/* ====================================================================
 *  4.  Teleport — direct-address single-XOR jump.
 *
 *      dst = q ⊕ bridges[idx]
 *
 *  When `q == anchors[idx]` this lands exactly on `targets[idx]`.
 *  Branchless, constant-time in D.  Returns 0 on success, -1 on bad
 *  index. */

int cos_v71_portal_teleport(const cos_v71_portal_t *p,
                            uint32_t                idx,
                            const cos_v71_hv_t     *q,
                            cos_v71_hv_t           *dst);

/* ====================================================================
 *  5.  Multi-hop greedy routing (Kleinberg small-world).
 *
 *  Given a source HV and a destination HV, greedy-route: at each hop,
 *  pick the portal whose *target* lies closest to `dst`, teleport from
 *  the current position, repeat until similarity(current, dst) ≥
 *  `arrival_q15` or `max_hops` hops have been spent.  Returns the
 *  number of hops actually taken; writes the final position to
 *  `out_pos`, and — if `out_trail` / `out_trail_n` are non-NULL —
 *  records the portal index used at each hop.
 *
 *  Worst-case bound: `max_hops`; Kleinberg expectation (with portals
 *  seeded per the 1/d distribution) is O(log² |cap|).
 * ==================================================================== */

typedef struct {
    uint32_t hops;                                 /* hops actually taken */
    int32_t  final_sim_q15;                        /* sim(current, dst)  */
    uint32_t trail[COS_V71_ROUTE_HOPS_MAX];        /* portal index per hop */
    uint8_t  arrived;                              /* 1 iff sim ≥ arrival */
    uint8_t  _pad[3];
} cos_v71_route_t;

void cos_v71_portal_route(const cos_v71_portal_t *p,
                          const cos_v71_hv_t     *src,
                          const cos_v71_hv_t     *dst,
                          int32_t                 arrival_q15,
                          uint32_t                max_hops,
                          cos_v71_hv_t           *out_pos,
                          cos_v71_route_t        *out_route);

/* ====================================================================
 *  6.  Tensor-bond wormhole — ER=EPR pairing between two stores.
 *
 *  Given HV stores A and B (indexed by row), the bond HV of row i in A
 *  and row j in B is `bond = A[i] ⊕ B[j]`.  Teleporting from A[i] to
 *  B[j] is exactly one XOR.  This exposes the MPS / PEPS bond-
 *  dimension trick in bit-exact VSA form — no singular-value
 *  decomposition, no floating-point, just one bridge HV per pair.
 *
 *  `bond_hvs`, `bonds` and `pairs` are caller-owned; we write `bonds`
 *  with the pairwise XORs.  The function also returns the aggregate
 *  Hamming distance across all bonds, useful for bond-dimension
 *  diagnostics (small total Hamming → strong coupling).
 * ==================================================================== */

typedef struct {
    uint32_t row_a;
    uint32_t row_b;
} cos_v71_bond_pair_t;

uint32_t cos_v71_bond_build(const cos_v71_hv_t        *store_a,
                            uint32_t                   len_a,
                            const cos_v71_hv_t        *store_b,
                            uint32_t                   len_b,
                            const cos_v71_bond_pair_t *pairs,
                            uint32_t                   n_pairs,
                            cos_v71_hv_t              *bonds);

/* ====================================================================
 *  7.  Integrity verify — HMAC-style bridge signature.
 *
 *  Recomputes `expected = bridge ⊕ permute(secret, seed)` and compares
 *  it to the stored `sig`.  Returns 1 iff Hamming(sig, expected) ≤
 *  `noise_floor_bits` (default 0 for strict), else 0.  Constant-time
 *  in D; side-channel bounded.
 * ==================================================================== */

uint8_t cos_v71_portal_verify(const cos_v71_portal_t *p,
                              uint32_t                idx,
                              uint32_t                noise_floor_bits);

/* ====================================================================
 *  8.  Hyperbolic / Poincaré-boundary gate.
 *
 *  Branchless bit-skew proxy for "is the query HV near the hyperbolic
 *  boundary, where wormholes are cheap and dense?":
 *
 *      skew  = |popcount(q) - D/2|
 *      ok    = (skew >= boundary_threshold_bits)
 *
 *  Near the boundary (skew large) ⇒ teleport authorised; near the
 *  centre (skew small) ⇒ fall back to linear deliberation.  No exp,
 *  no div, no FP.
 * ==================================================================== */

uint8_t cos_v71_boundary_gate(const cos_v71_hv_t *q,
                              uint32_t            boundary_threshold_bits);

/* ====================================================================
 *  9.  Path receipt — bundled HV of hop anchors.
 *
 *  Given a route trail (portal indices), produce a single receipt HV
 *  that is the XOR-bundle of the corresponding anchor HVs.  This is
 *  the "show your work" object for non-local jumps: any two distinct
 *  paths produce near-orthogonal receipts, and a receipt is
 *  recoverable via cleanup against the portal anchor arena.
 *
 *  Note: XOR-bundle is order-insensitive.  If path ordering matters,
 *  the caller should permute each anchor by `perm^hop_i` before XOR-
 *  ing.  `cos_v71_path_receipt_ordered` does this automatically with
 *  cyclic bit rotations of `hop_i` bit positions.
 * ==================================================================== */

int cos_v71_path_receipt(const cos_v71_portal_t *p,
                         const uint32_t         *trail,
                         uint32_t                n_hops,
                         cos_v71_hv_t           *out_receipt);

int cos_v71_path_receipt_ordered(const cos_v71_portal_t *p,
                                 const uint32_t         *trail,
                                 uint32_t                n_hops,
                                 cos_v71_hv_t           *out_receipt);

/* ====================================================================
 * 10.  WHL — Wormhole Language — 10-opcode integer bytecode ISA.
 *
 *  Cost model (teleport-unit):
 *      HALT     = 1
 *      ANCHOR   = 2   (copy hv to regs[dst])
 *      BIND     = 2   (regs[dst] = regs[a] ⊕ regs[b])
 *      TELEPORT = 4   (regs[dst] = regs[a] ⊕ bridges[idx])
 *      ROUTE    = 16  (greedy routing from regs[a] to regs[b])
 *      BOND     = 2   (regs[dst] = regs[a] ⊕ regs[b] — alias of BIND
 *                       for readability)
 *      VERIFY   = 2   (integrity check on portal idx)
 *      HOP      = 2   (increment hop counter, branchless compare)
 *      MEASURE  = 2   (similarity(regs[a], regs[b]) → reg_q15[dst])
 *      GATE     = 1   (final gate)
 *
 *  GATE writes `v71_ok = 1` iff:
 *      cost             ≤ cost_budget
 *      AND hops         ≤ hop_budget
 *      AND integrity_ok == 1
 *      AND reg_q15[a]   ≥ imm          (arrival threshold)
 *      AND boundary_ok  == 1           (Poincaré gate, checked at
 *                                        ANCHOR-time)
 *      AND abstained    == 0
 *
 *  So no teleport program produces `v71_ok = 1` without simultaneously
 *  demonstrating cost-budget compliance, hop-budget compliance, a
 *  valid integrity signature on every traversed bridge, arrival on
 *  the target manifold, and a Poincaré-boundary-regime anchor.
 * ==================================================================== */

typedef enum {
    COS_V71_OP_HALT     = 0,
    COS_V71_OP_ANCHOR   = 1,
    COS_V71_OP_BIND     = 2,
    COS_V71_OP_TELEPORT = 3,
    COS_V71_OP_ROUTE    = 4,
    COS_V71_OP_BOND     = 5,
    COS_V71_OP_VERIFY   = 6,
    COS_V71_OP_HOP      = 7,
    COS_V71_OP_MEASURE  = 8,
    COS_V71_OP_GATE     = 9,
} cos_v71_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;    /* portal idx, threshold, hop count, etc. */
    uint16_t pad;
} cos_v71_inst_t;

typedef struct {
    /* ANCHOR source (loaded into regs[dst]; also triggers boundary gate) */
    const cos_v71_hv_t  *anchor_src;

    /* TELEPORT / ROUTE portal reference */
    const cos_v71_portal_t *portal;

    /* ROUTE endpoints (regs[a] = src, regs[b] = dst); result in regs[dst] */
    int32_t  route_arrival_q15;
    uint32_t route_max_hops;

    /* VERIFY noise floor */
    uint32_t verify_noise_floor_bits;

    /* MEASURE destination register for the Q0.15 similarity */
    /* (implicit: measured into state reg_q15[dst]) */

    /* Optional: abstain flag — if set, every GATE returns v71_ok = 0 */
    uint8_t abstain;
} cos_v71_whl_ctx_t;

typedef struct cos_v71_whl_state cos_v71_whl_state_t;

cos_v71_whl_state_t *cos_v71_whl_new(void);
void                 cos_v71_whl_free(cos_v71_whl_state_t *s);
void                 cos_v71_whl_reset(cos_v71_whl_state_t *s);

uint64_t cos_v71_whl_cost(const cos_v71_whl_state_t *s);
int32_t  cos_v71_whl_reg_q15(const cos_v71_whl_state_t *s, uint32_t i);
uint32_t cos_v71_whl_hops(const cos_v71_whl_state_t *s);
uint8_t  cos_v71_whl_integrity_ok(const cos_v71_whl_state_t *s);
uint8_t  cos_v71_whl_boundary_ok(const cos_v71_whl_state_t *s);
uint8_t  cos_v71_whl_abstained(const cos_v71_whl_state_t *s);
uint8_t  cos_v71_whl_v71_ok(const cos_v71_whl_state_t *s);

/* Execute `prog` against the given context.  `cost_budget` and
 * `hop_budget` are hard ceilings; exceeding either sets abstain.
 * Returns 0 on HALT, -1 on malformed op, -2 on budget exceeded. */
int cos_v71_whl_exec(cos_v71_whl_state_t    *s,
                     const cos_v71_inst_t   *prog,
                     uint32_t                n,
                     cos_v71_whl_ctx_t      *ctx,
                     uint64_t                cost_budget,
                     uint32_t                hop_budget);

/* ====================================================================
 *  Composed 12-bit branchless decision.
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
    uint8_t allow;
    uint8_t _pad[3];
} cos_v71_decision_t;

cos_v71_decision_t cos_v71_compose_decision(uint8_t v60_ok,
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
                                            uint8_t v71_ok);

/* ====================================================================
 *  Version string.
 * ==================================================================== */

extern const char cos_v71_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V71_WORMHOLE_H */
