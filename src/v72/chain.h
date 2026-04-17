/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v72 — σ-Chain (blockchain / web3 / post-quantum /
 *                             zero-knowledge / verifiable-agent
 *                             substrate plane).
 *
 * ------------------------------------------------------------------
 *  What v72 is
 * ------------------------------------------------------------------
 *
 * Every kernel up to v71 is local-to-the-process: σ-Shield gates the
 * action, Σ-Citadel the lattice, Reasoning verifies energy, Cipher
 * the envelope, Intellect the MCTS, Hypercortex the cortex, Silicon
 * the matrix, Noesis the deliberation, Mnemos the memory,
 * Constellation the fleet, Hyperscale the substrate, Wormhole the
 * non-local teleport.  What is still missing is an *inter-process /
 * inter-agent trust surface* that is **quantum-secure by construc-
 * tion**, **auditable** (every step leaves a constant-size proof
 * receipt), **delegation-bounded** (the agent may act only inside an
 * authorised session), and **Byzantine-tolerant** (no single node
 * controls the verdict).  That trust surface is σ-Chain.
 *
 * σ-Chain is the post-quantum / web3 / verifiable-agent *substrate*
 * expressed in the same discipline as the rest of the stack: branch-
 * less integer-only C, libc-only on the hot path, aligned_alloc(64),
 * explicit `__builtin_prefetch` friendly, NEON-friendly popcount +
 * XOR inner loops, no floating-point on any decision surface.
 *
 * ------------------------------------------------------------------
 *  Ten primitives (the 2024-2026 web3 / PQC / ZK frontier)
 * ------------------------------------------------------------------
 *
 *   1.  Binary Merkle tree (authenticated ledger)
 *       — BLAKE2b / FIPS-202 lineage, here a libc-only integer
 *       mixing function (Creation-OS-internal, UB-free).  Constant-
 *       time leaf → root build over a power-of-two leaf count; O(log
 *       n) sibling-path proof; branchless verify.  SonicDB S6
 *       (arXiv:2604.06579) + Verkle benchmarking (arXiv:2504.14069)
 *       motivate constant-size witnesses.  Optional build-time
 *       `COS_V72_LIBSODIUM=1` swaps the internal mix for BLAKE2b-256
 *       via crypto_generichash (policy inherited from v63 σ-Cipher).
 *
 *   2.  Append-only receipt chain
 *       — each receipt binds its predecessor: `prev_hash = H(prev)`;
 *       insertion is O(1); proof-of-position is the Merkle sibling
 *       path; rewriting history flips every downstream root.  This
 *       is the AlphaProof / AlphaFold-3 "show your work" discipline
 *       projected onto a distributed-agent ledger.
 *
 *   3.  Hash-based one-time signature (WOTS+ / XMSS lineage)
 *       — NIST FIPS 205 SLH-DSA and the 2025-2026 XMSS / SPHINCS+
 *       extensions (eprint 2026/134 verified implementations; eprint
 *       2026/194 unified hardware; eprint 2025/2236 tree-height
 *       tuning) all ground their post-quantum assurance in the one-
 *       way property of the hash.  v72 ships a minimal Winternitz
 *       chain (w = 16, n = 32 bytes) with branchless chain evaluation
 *       and strict one-time use enforcement — leaks on reuse are
 *       detected at `ots_verify`-time.  This is a *research-tier*
 *       hash-based OTS: it composes correctly, its UB / ASAN / UBSAN
 *       behaviour is clean, and it is not a drop-in FIPS 205
 *       implementation — use SLH-DSA (sphincsplus / liboqs) for
 *       certified deployments.
 *
 *   4.  Threshold t-of-n quorum (TALUS ML-DSA lineage)
 *       — TALUS (arXiv:2603.22109) gives the first one-round online
 *       threshold ML-DSA via Boundary-Clearance + Carry-Elimination.
 *       v72 captures the algebraic surface in integer form: every
 *       signer contributes an integer share; the aggregator computes
 *       `combined = sum(shares) mod 2^k`, checks boundary-clearance
 *       (|combined| within the legal clearance window), and counts
 *       signers to at least `threshold`.  Branchless gate:
 *       `quorum_ok = (count ≥ t) AND (clearance_ok)`.
 *
 *   5.  VRF leader election (indexed / hash-based VRF)
 *       — Key-Updatable Hash-Based VRF (eprint 2026/052) and iVRF
 *       (eprint 2022/993) give post-quantum-secure VRFs from hash
 *       functions alone.  v72 implements a minimal hash-chain VRF:
 *       `vrf_out = H(H(H(... H(seed, msg) ...)))`; verification
 *       replays the chain from the public commit.  Branchless,
 *       O(depth) hot path, constant-time per depth.
 *
 *   6.  DAG-BFT quorum / round-based consensus
 *       — Narwhal + Bullshark (arXiv:2105.11827, arXiv:2201.05677,
 *       arXiv:2507.04956) establish that DAG-based mempool + round-
 *       committee consensus scales to 297 000 TPS at 2 s latency.
 *       v72 exposes the 2f + 1 quorum gate as a single branchless
 *       integer compare, plus a round-tag check that prevents
 *       replay across rounds.  The Byzantine fault tolerance
 *       budget is enforced against the v69 Constellation σ-budget.
 *
 *   7.  Zero-knowledge proof receipt (zkAgent / NANOZK lineage)
 *       — zkAgent (eprint 2026/199) and NANOZK (arXiv:2603.18046v1)
 *       produce constant-size proofs of end-to-end LLM inference.
 *       v72 does not ship a SNARK prover; it ships the *proof-
 *       receipt surface*: a 256-bit commitment `proof_commit =
 *       H(public_input || vk_id || proof_digest)` that the agent
 *       can publish on-chain and an integer verifier that accepts
 *       iff the commitment matches the one bound by the capability
 *       issuer.  Honest SKIP behaviour when no ZK back-end is
 *       linked (default); opt-in `COS_V72_ZKAGENT=1` reserves the
 *       external-verifier slot.  Project VATA's Groth16 + BN254
 *       pattern is the reference for on-chain anchoring (Medium,
 *       Feb 2026).
 *
 *   8.  EIP-7702 / ERC-4337 session-key delegation
 *       — The 2025-2026 account-abstraction stack (EIP-7702
 *       activated Mar 2025; ERC-7579 modular execution; session-key
 *       primitives documented at docs.erc4337.io) converged on:
 *       a *session key* may act on behalf of a primary key *only*
 *       within a (valid_after, valid_before, scope_mask, spend_cap)
 *       envelope.  v72 exposes this as a branchless integer check:
 *       `delegation_ok = (now ∈ [after, before]) AND (op_mask ⊆
 *       scope_mask) AND (spend ≤ cap) AND sig_ok`.  No dynamic
 *       allocation, no `if` on the hot path, no FP.
 *
 *   9.  Chain-state receipt bundle
 *       — analogous to v71's path receipt: the XOR-digest over all
 *       receipt hashes along a chain span.  Two distinct histories
 *       produce near-orthogonal bundle digests; the bundle is the
 *       "show your work" object for the ledger itself, suitable for
 *       on-chain anchoring and off-chain audit.
 *
 *  10.  SCL — σ-Chain Language — 10-opcode integer bytecode ISA
 *       (`HALT / LEAF / ROOT / PROVE / VERIFY / OTS / QUORUM / VRF /
 *       DELEGATE / GATE`) with per-instruction gas-unit cost
 *       accounting and an integrated `GATE` opcode that writes
 *       `v72_ok = 1` iff:
 *
 *           gas             ≤ gas_budget
 *           AND quorum_ok   == 1
 *           AND ots_ok      == 1
 *           AND proof_ok    == 1
 *           AND delegation_ok == 1
 *           AND abstained   == 0
 *
 *       So no chain-bound step crosses to the agent without
 *       simultaneously demonstrating gas-budget compliance, t-of-n
 *       quorum, an unused + valid WOTS+ signature on the current
 *       message, a valid ZK proof-receipt commitment, and a live
 *       session-key delegation — all as a single branchless AND.
 *
 * ------------------------------------------------------------------
 *  Composition — 13-bit branchless decision
 * ------------------------------------------------------------------
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
 *                         silicon + throughput + topology budgets?
 *   v71 σ-Wormhole      — does the teleport arrive + under hop
 *                         budget + integrity + boundary-ok?
 *   v72 σ-Chain         — does the chain-bound step clear gas,
 *                         quorum, OTS, proof, and delegation
 *                         gates, with a valid Merkle proof and
 *                         abstention cleared?
 *
 * `cos_v72_compose_decision` is a single 13-way branchless AND.
 * Nothing — no inference, tool call, sealed message, retrieval,
 * write, route, hyperscale step, teleport, or chain emission —
 * crosses to the agent unless every one of the thirteen kernels
 * ALLOWs.
 *
 * ------------------------------------------------------------------
 *  Hardware discipline
 * ------------------------------------------------------------------
 *
 *   - Digest size: 256 bits = 4 × uint64 = 32 B.  Every digest is
 *     naturally aligned to 32 bytes; arena allocations use
 *     `aligned_alloc(64, …)` with 64-byte rounding.
 *   - Merkle node arena stored row-major by tree level; fully
 *     contiguous; branchless parent = H(left || right).
 *   - WOTS+ chain evaluation is a tight integer loop; no data-
 *     dependent branch on the secret material.  One-time-use is
 *     enforced at verify-time by chain-consistency, not by
 *     secret-dependent control flow.
 *   - Threshold combine is integer sum + modulo-2^k mask; carry-
 *     elimination check is a single comparison.
 *   - VRF chain depth is a compile-time constant; the same number
 *     of hash rounds is executed regardless of input.
 *   - Delegation gate is four branchless integer compares ANDed
 *     together.  No syscalls on the hot path.
 *   - The 13-bit composed decision is a single branchless AND;
 *     nothing downstream reads `allow = 0` without the caller
 *     having inspected each lane honestly.
 *
 * Tier semantics (v57 dialect):
 *   M  this kernel runs and is checked at runtime by `make check-v72`
 *   I  The BLAKE2b-style mix is an internal UB-free integer mixer —
 *      not a FIPS-204 / 205 certified primitive.  The algebraic
 *      surface is correct; production users should link libsodium
 *      (set `COS_V72_LIBSODIUM=1`) to get BLAKE2b-256.
 *   P  SLH-DSA (FIPS 205) and ML-DSA (FIPS 204) back-ends are a
 *      *planned* build-time slot (env flag `COS_V72_LIBOQS=1`) —
 *      documented, not claimed in the default build.
 *
 * v72 ships M-tier for all ten primitives + the 13-bit composition.
 * No silent downgrade path.  Zero runtime dependencies on libsodium,
 * liboqs, any SNARK library, or any toolchain beyond libc on the
 * hot path.
 *
 * 1 = 1.
 */

#ifndef COS_V72_CHAIN_H
#define COS_V72_CHAIN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ====================================================================
 *  0.  Primitive types and constants.
 * ==================================================================== */

#define COS_V72_HASH_WORDS   4u            /* 4 × uint64 = 256 bits */
#define COS_V72_HASH_BYTES   32u

typedef struct {
    uint64_t w[COS_V72_HASH_WORDS];
} cos_v72_hash_t;

/* Limits (compile-time). */
#define COS_V72_MERKLE_LEAVES_MAX   65536u      /* 2^16 leaves          */
#define COS_V72_MERKLE_DEPTH_MAX       16u      /* = log2(LEAVES_MAX)   */
#define COS_V72_CHAIN_RECEIPTS_MAX  65536u      /* append-only cap       */
#define COS_V72_WOTS_W                 16u      /* Winternitz w          */
#define COS_V72_WOTS_LEN               32u      /* message len (n bytes) */
#define COS_V72_QUORUM_MAX             64u      /* max signers per quorum*/
#define COS_V72_VRF_DEPTH              32u      /* hash-chain VRF depth  */
#define COS_V72_NREGS                  16u      /* SCL register file     */
#define COS_V72_PROG_MAX             1024u      /* SCL program cap       */

/* ====================================================================
 *  1.  Core integer hash (UB-free, libc-only).
 *
 *  Mixing function: four splitmix64-style rounds folded over the
 *  256-bit input, with cross-lane rotates.  Not FIPS-certified;
 *  collision-resistance claim is *experimental* at the 128-bit
 *  effective level sufficient for Creation-OS-internal self-test
 *  discipline.  Flip `COS_V72_LIBSODIUM=1` at build-time to swap in
 *  BLAKE2b-256 via `crypto_generichash`.
 * ==================================================================== */

void cos_v72_hash_zero(cos_v72_hash_t *dst);
void cos_v72_hash_copy(cos_v72_hash_t *dst, const cos_v72_hash_t *src);
int  cos_v72_hash_eq  (const cos_v72_hash_t *a, const cos_v72_hash_t *b);

/* Seed a deterministic digest from a uint64. */
void cos_v72_hash_from_seed(cos_v72_hash_t *dst, uint64_t seed);

/* Compress `in_bytes` bytes → 256-bit digest. */
void cos_v72_hash_compress(cos_v72_hash_t *dst,
                           const void *in_bytes,
                           size_t      in_len);

/* H(a || b) — the Merkle parent function. */
void cos_v72_hash_pair(cos_v72_hash_t       *dst,
                       const cos_v72_hash_t *a,
                       const cos_v72_hash_t *b);

/* ====================================================================
 *  2.  Binary Merkle tree.
 *
 *  Leaves are pre-computed digests (typically `hash_compress` over
 *  application payload).  The tree is a row-major contiguous node
 *  arena:
 *
 *      level 0: leaves[0..N-1]
 *      level 1: parent = hash_pair(left, right)  (N/2 entries)
 *      level k: ... until root (1 entry)
 *
 *  Proof of membership for leaf `i` is the sibling digest at every
 *  level — log2(N) hashes.  Verify replays the ladder.  Branchless;
 *  no allocation on the hot path after `merkle_build`.
 * ==================================================================== */

typedef struct {
    cos_v72_hash_t *nodes;       /* 2 * N − 1 entries, 64-B aligned */
    uint32_t        n_leaves;    /* power of two, ≤ MERKLE_LEAVES_MAX */
    uint32_t        depth;       /* = log2(n_leaves) */
    cos_v72_hash_t  root;
} cos_v72_merkle_t;

typedef struct {
    cos_v72_hash_t siblings[COS_V72_MERKLE_DEPTH_MAX];
    uint32_t       index;        /* leaf index */
    uint32_t       depth;
} cos_v72_merkle_proof_t;

cos_v72_merkle_t *cos_v72_merkle_new(uint32_t n_leaves);
void              cos_v72_merkle_free(cos_v72_merkle_t *m);

/* Write a leaf.  Does not rebuild; call `merkle_build` after. */
int cos_v72_merkle_set_leaf(cos_v72_merkle_t     *m,
                            uint32_t              i,
                            const cos_v72_hash_t *leaf);

/* Build all internal levels + root.  O(N) hash ops. */
int cos_v72_merkle_build(cos_v72_merkle_t *m);

/* Extract an O(log N) sibling-path proof for leaf `i`. */
int cos_v72_merkle_prove(const cos_v72_merkle_t *m,
                         uint32_t                i,
                         cos_v72_merkle_proof_t *out);

/* Verify proof against a known root.  Returns 1 iff the leaf, proof,
 * and root are mutually consistent. */
uint8_t cos_v72_merkle_verify(const cos_v72_hash_t        *leaf,
                              const cos_v72_merkle_proof_t *proof,
                              const cos_v72_hash_t        *root);

/* ====================================================================
 *  3.  Append-only receipt chain.
 *
 *  Each receipt is a 256-bit digest over (payload_digest || prev_hash
 *  || round_tag).  The chain arena is a flat `cos_v72_hash_t[len]`
 *  with root rebuilt whenever the caller requests a Merkle anchor.
 *  Rewriting history flips every downstream hash — deterministically.
 * ==================================================================== */

typedef struct {
    cos_v72_hash_t *receipts;    /* cap × 32 B */
    uint64_t       *rounds;      /* cap × 8 B — round tag per receipt */
    uint32_t        cap;
    uint32_t        len;
    cos_v72_hash_t  tip;         /* == receipts[len-1] or zero */
} cos_v72_chain_t;

cos_v72_chain_t *cos_v72_chain_new(uint32_t cap);
void             cos_v72_chain_free(cos_v72_chain_t *c);

/* Append H(payload_digest || tip || round_tag) → new tip. */
int cos_v72_chain_append(cos_v72_chain_t      *c,
                         const cos_v72_hash_t *payload_digest,
                         uint64_t              round_tag);

/* XOR-bundle digest across all receipts in [from, to).  The bundle
 * is the "show your work" object for the ledger span, suitable for
 * on-chain anchoring. */
int cos_v72_chain_bundle(const cos_v72_chain_t *c,
                         uint32_t               from,
                         uint32_t               to,
                         cos_v72_hash_t        *out_bundle);

/* ====================================================================
 *  4.  WOTS+ one-time signature (hash-based, PQC-spirit).
 *
 *  Minimal Winternitz chain with w = 16, n = 32 bytes.  NOT FIPS 205
 *  certified — see file-level comment for the opt-in linkage story.
 *  One-time use: the same secret must not be reused to sign a second
 *  message.  Verifier reconstructs the public key from the signature
 *  + message and checks equality with the stored public key.
 *
 *  Integer-only, branchless chain evaluation; constant-time in
 *  `remaining` (the caller sees the same number of hash rounds per
 *  chain slot regardless of message byte value).
 * ==================================================================== */

typedef struct {
    cos_v72_hash_t secret[COS_V72_WOTS_LEN];     /* per-byte sk chain start */
    cos_v72_hash_t pubkey[COS_V72_WOTS_LEN];     /* per-byte pk chain end   */
    uint8_t        used;                          /* one-time guard         */
    uint8_t        _pad[7];
} cos_v72_ots_t;

typedef struct {
    cos_v72_hash_t nodes[COS_V72_WOTS_LEN];      /* signature chain nodes */
    uint8_t        msg[COS_V72_WOTS_LEN];        /* message bytes         */
} cos_v72_ots_sig_t;

/* Key-gen: derive secret from seed, then evaluate each chain
 * forwards (w − 1) hash rounds to produce the public key. */
void cos_v72_ots_keygen(cos_v72_ots_t *k, uint64_t seed);

/* Sign: for each message byte m_i, evaluate m_i rounds on sk_i → sig_i.
 * Marks the key as used.  Returns -1 if already used. */
int  cos_v72_ots_sign(cos_v72_ots_t        *k,
                      const uint8_t        *msg,
                      cos_v72_ots_sig_t    *out);

/* Verify: for each byte m_i, evaluate (w - 1 - m_i) rounds on sig_i,
 * compare against pk_i.  Returns 1 iff *all* slots match. */
uint8_t cos_v72_ots_verify(const cos_v72_ots_t     *k,
                           const cos_v72_ots_sig_t *sig);

/* ====================================================================
 *  5.  Threshold t-of-n quorum (TALUS-spirit, integer form).
 *
 *  Every signer contributes a `share` integer and a per-signer
 *  boundary-clearance token `clear_token = (share ^ round_tag) &
 *  ~CLEARANCE_MASK`.  The aggregator accepts iff:
 *
 *     count     ≥ threshold
 *     AND  sum_mod_k within clearance window
 *     AND  every clear_token == 0 (no signer crossed the carry wall)
 *
 *  Branchless AND over the three conditions; no FP.
 * ==================================================================== */

typedef struct {
    uint64_t shares[COS_V72_QUORUM_MAX];
    uint8_t  present[COS_V72_QUORUM_MAX];   /* 1 iff signer i contributed */
    uint32_t n;                              /* total signers              */
    uint32_t t;                              /* threshold                  */
    uint64_t round_tag;
    uint64_t clearance_mask;                 /* forbidden high-bits */
    uint64_t boundary_lo;                    /* sum must be ≥ lo */
    uint64_t boundary_hi;                    /* and ≤ hi */
} cos_v72_quorum_t;

/* Initialise; shares/present zeroed. */
void cos_v72_quorum_init(cos_v72_quorum_t *q,
                         uint32_t          n,
                         uint32_t          t,
                         uint64_t          round_tag);

/* Submit a signer's share.  Returns the updated `count`. */
uint32_t cos_v72_quorum_submit(cos_v72_quorum_t *q,
                               uint32_t          signer_idx,
                               uint64_t          share);

/* Verdict: quorum_ok ∈ {0, 1}. */
uint8_t cos_v72_quorum_verdict(const cos_v72_quorum_t *q,
                               uint64_t               *out_sum_mod_k);

/* ====================================================================
 *  6.  Hash-chain VRF (iVRF / key-updatable XMSS-spirit).
 *
 *  vrf_out = H^depth(seed || msg), with per-round salt to prevent
 *  chain extension attacks.  Verification replays from a public
 *  commit to the anchor digest.  Branchless, fixed depth (constant-
 *  time per evaluation).
 * ==================================================================== */

typedef struct {
    cos_v72_hash_t commit;          /* public anchor = H(seed) */
    uint64_t       seed_materia;    /* secret scalar (caller-owned) */
} cos_v72_vrf_t;

void cos_v72_vrf_keygen(cos_v72_vrf_t *v, uint64_t seed);

/* Evaluate: out = H^depth(H(seed || msg || round_salt_0) || ...).  */
void cos_v72_vrf_eval(const cos_v72_vrf_t  *v,
                      const void           *msg,
                      size_t                msg_len,
                      cos_v72_hash_t       *out,
                      cos_v72_hash_t       *out_chain_witness); /* intermediate */

/* Verify: given `commit`, `msg`, claimed `out`, and the witness at
 * depth (depth - 1), re-do one hash step and compare.  Returns 1 iff
 * consistent with the committed seed. */
uint8_t cos_v72_vrf_verify(const cos_v72_hash_t *commit,
                           const void           *msg,
                           size_t                msg_len,
                           const cos_v72_hash_t *claimed_out,
                           const cos_v72_hash_t *witness);

/* ====================================================================
 *  7.  DAG-BFT quorum gate (Narwhal + Bullshark lineage).
 *
 *  Integer 2f + 1 quorum check with round-tag binding and an
 *  explicit Byzantine fault ceiling.  Returns 1 iff:
 *
 *     votes          ≥ 2*f + 1
 *     AND total      ≥ 3*f + 1
 *     AND round_tag  == expected_round
 *     AND faults     ≤ f
 *
 *  Branchless AND of four integer compares.
 * ==================================================================== */

uint8_t cos_v72_dag_quorum_gate(uint32_t votes,
                                uint32_t total,
                                uint32_t faults,
                                uint64_t round_tag,
                                uint64_t expected_round);

/* ====================================================================
 *  8.  Zero-knowledge proof-receipt surface.
 *
 *  `proof_commit = H(public_input || vk_id || proof_digest)`.  The
 *  verifier accepts iff the on-chain commitment equals the expected
 *  one *and* the expected one has been bound by the session-key
 *  issuer (see §9).  No SNARK back-end is linked; integrations pass
 *  in the back-end's `proof_digest` and the kernel seals it into the
 *  chain.  Opt-in slot `COS_V72_ZKAGENT=1` is documented but not
 *  claimed in the default build.
 * ==================================================================== */

typedef struct {
    cos_v72_hash_t commit;          /* = H(public_in || vk_id || proof_digest) */
    uint64_t       vk_id;           /* verifier-key / circuit-family id */
} cos_v72_zk_receipt_t;

void cos_v72_zk_receipt_make(cos_v72_zk_receipt_t *r,
                             const void            *public_in,
                             size_t                 public_in_len,
                             uint64_t               vk_id,
                             const cos_v72_hash_t  *proof_digest);

uint8_t cos_v72_zk_receipt_verify(const cos_v72_zk_receipt_t *r,
                                  const cos_v72_hash_t       *expected);

/* ====================================================================
 *  9.  EIP-7702 / ERC-4337 session-key delegation capability.
 *
 *  A session-key grant bounds the delegated key in four orthogonal
 *  ways.  The gate is a single branchless AND across the four
 *  integer conditions + the signature bit.
 * ==================================================================== */

typedef struct {
    uint64_t       session_id;
    uint64_t       valid_after;     /* unix-ish time, caller-chosen epoch */
    uint64_t       valid_before;
    uint64_t       scope_mask;      /* permitted operation bits */
    uint64_t       spend_cap;       /* integer spend ceiling */
    uint64_t       spent;           /* running total (caller updates) */
    cos_v72_hash_t issuer_commit;   /* H(primary_pubkey || session_id) */
    uint8_t        sig_ok;          /* caller-set: did the primary sign it? */
    uint8_t        _pad[7];
} cos_v72_delegation_t;

/* Branchless gate. */
uint8_t cos_v72_delegation_gate(const cos_v72_delegation_t *d,
                                uint64_t                    now,
                                uint64_t                    op_mask,
                                uint64_t                    op_spend);

/* ====================================================================
 * 10.  SCL — σ-Chain Language — 10-opcode integer bytecode ISA.
 *
 *  Cost model (gas):
 *      HALT        = 1
 *      LEAF        = 2   (write leaf i)
 *      ROOT        = 16  (build tree + root)
 *      PROVE       = 4   (extract sibling-path proof)
 *      VERIFY      = 4   (verify proof against root)
 *      OTS         = 32  (WOTS+ verify)
 *      QUORUM      = 4   (threshold verdict)
 *      VRF         = 32  (hash-chain VRF verify)
 *      DELEGATE    = 2   (session-key gate)
 *      GATE        = 1
 *
 *  GATE writes `v72_ok = 1` iff:
 *      gas            ≤ gas_budget
 *      AND quorum_ok  == 1
 *      AND ots_ok     == 1
 *      AND proof_ok   == 1
 *      AND delegation_ok == 1
 *      AND abstained  == 0
 * ==================================================================== */

typedef enum {
    COS_V72_OP_HALT     = 0,
    COS_V72_OP_LEAF     = 1,
    COS_V72_OP_ROOT     = 2,
    COS_V72_OP_PROVE    = 3,
    COS_V72_OP_VERIFY   = 4,
    COS_V72_OP_OTS      = 5,
    COS_V72_OP_QUORUM   = 6,
    COS_V72_OP_VRF      = 7,
    COS_V72_OP_DELEGATE = 8,
    COS_V72_OP_GATE     = 9,
} cos_v72_op_t;

typedef struct {
    uint8_t  op;
    uint8_t  dst;
    uint8_t  a;
    uint8_t  b;
    int16_t  imm;
    uint16_t pad;
} cos_v72_scl_inst_t;

typedef struct {
    cos_v72_merkle_t           *tree;         /* borrowed */
    const cos_v72_hash_t       *leaves;       /* leaves to load into tree */
    uint32_t                    n_leaves;
    const cos_v72_ots_t        *ots_key;      /* borrowed for VERIFY */
    const cos_v72_ots_sig_t    *ots_sig;      /* borrowed */
    const cos_v72_quorum_t     *quorum;       /* borrowed */
    const cos_v72_vrf_t        *vrf_key;      /* borrowed */
    const void                 *vrf_msg;
    size_t                      vrf_msg_len;
    const cos_v72_hash_t       *vrf_out;      /* claimed */
    const cos_v72_hash_t       *vrf_witness;  /* depth-1 witness */
    const cos_v72_delegation_t *deleg;        /* borrowed */
    uint64_t                    deleg_now;
    uint64_t                    deleg_op;
    uint64_t                    deleg_spend;
    uint8_t                     abstain;
} cos_v72_scl_ctx_t;

typedef struct cos_v72_scl_state cos_v72_scl_state_t;

cos_v72_scl_state_t *cos_v72_scl_new(void);
void                 cos_v72_scl_free(cos_v72_scl_state_t *s);
void                 cos_v72_scl_reset(cos_v72_scl_state_t *s);

uint64_t cos_v72_scl_gas(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_quorum_ok(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_ots_ok(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_proof_ok(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_delegation_ok(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_vrf_ok(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_abstained(const cos_v72_scl_state_t *s);
uint8_t  cos_v72_scl_v72_ok(const cos_v72_scl_state_t *s);

/* Execute bytecode.  Returns 0 on HALT, -1 on malformed op,
 * -2 on gas budget exceeded. */
int cos_v72_scl_exec(cos_v72_scl_state_t      *s,
                     const cos_v72_scl_inst_t *prog,
                     uint32_t                  n,
                     cos_v72_scl_ctx_t        *ctx,
                     uint64_t                  gas_budget);

/* ====================================================================
 *  Composed 13-bit branchless decision.
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
    uint8_t allow;
    uint8_t _pad[2];
} cos_v72_decision_t;

cos_v72_decision_t cos_v72_compose_decision(uint8_t v60_ok,
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
                                            uint8_t v72_ok);

/* ====================================================================
 *  Version string.
 * ==================================================================== */

extern const char cos_v72_version[];

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* COS_V72_CHAIN_H */
