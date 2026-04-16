/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v60 — σ-Shield: runtime security kernel.
 *
 *   Capability-bitmask authorisation + σ-gated intent + TOCTOU-free
 *   argument validation + code-page integrity self-check, all in one
 *   branchless kernel.  Four-valued decision:
 *
 *     ALLOW          caps sufficient, intent coherent, args unchanged,
 *                    code integrity holds
 *     DENY_CAP       caller is missing one or more required capabilities
 *     DENY_INTENT    σ_total high AND α dominates — request is
 *                    irreducibly ambiguous; σ-Shield refuses to enforce
 *                    on a request whose *meaning* is undecidable
 *     DENY_TOCTOU    argument hash at call time ≠ hash at validate time
 *                    (time-of-check / time-of-use mismatch)
 *     DENY_INTEGRITY code-page self-hash drifted from baseline
 *
 *   ## Motivation (2026 threat landscape)
 *
 *   The Q2 2026 attack literature converges on **ambiguous payloads**:
 *
 *     - DDIPE (arXiv:2604.03081) embeds malicious logic in skill
 *       documentation; 11-33 % bypass against explicit-prompt defenses
 *       because the payload looks 99 % like legitimate usage.
 *     - Malicious Intermediary Attacks (arXiv:2604.08407) — 17 of 428
 *       audited API routers touch caller credentials in plaintext.
 *     - ClawWorm (arXiv:2603.15727) — 64.5 % self-propagating success
 *       across OpenClaw: the worm's messages look indistinguishable
 *       from legitimate agent-to-agent traffic.
 *     - ShieldNet (arXiv:2604.04426) is the first network-level
 *       guardrail (MITM + event extraction, 0.995 F1).
 *
 *   Every defense above is **policy-level or signature-level**.  None
 *   uses a σ = (ε, α) intent signal, even though every one of those
 *   attacks is *irreducibly ambiguous by construction* — they succeed
 *   precisely because a scalar confidence signal cannot separate
 *   legitimate intent from ambiguous intent.  v60 is the first
 *   capability-based security kernel to use σ-decomposition as the
 *   intent gate.
 *
 *   σ-Shield is the sibling of v59 σ-Budget: v59 refuses to *compute*
 *   on α-dominated problems; v60 refuses to *act* on α-dominated
 *   requests.  Same decomposition, different enforcement surface.
 *
 *   ## Non-claims
 *
 *   - v60 does NOT replace seL4 / Fuchsia / WASM capabilities; it
 *     composes with them.
 *   - v60 does NOT detect novel zero-days.  It gates on caller-provided
 *     σ + caller-provided required-caps + caller-provided arg-hash.
 *     Garbage σ in → garbage decision out.  See THREAT_MODEL.md.
 *   - No Frama-C proof yet.  All v60 claims are M-tier runtime checks.
 *   - No TPM / Secure Enclave integration yet (planned, P-tier).
 *
 *   ## Tier semantics (v57 dialect)
 *     M — runtime-checked deterministic self-test          (all v60)
 *     F — formally proven proof artifact                   (none yet)
 *     I — interpreted / documented                         (σ↔ cap binding)
 *     P — planned                                          (TPM / SLSA L3)
 *
 *   ## Hardware discipline (.cursorrules)
 *     1. Zero dynamic allocation on the authorise hot path.
 *     2. All scratch via `aligned_alloc(64, ⌈n·48/64⌉·64)`.
 *     3. __builtin_prefetch 16 lanes ahead in batch authorise loop.
 *     4. No `if` on the decision hot path — 0/1 masks via `& | ~`.
 *     5. No framework dependency — pure C11 + libc.
 *     6. Constant-time hash comparison (no early exit on mismatch).
 */

#ifndef CREATION_OS_V60_SIGMA_SHIELD_H
#define CREATION_OS_V60_SIGMA_SHIELD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 64-capability bitmask.  Reserve 32-63 for future growth. */
enum {
    COS_V60_CAP_NONE         = 0,
    COS_V60_CAP_FS_READ      = 1ULL <<  0,
    COS_V60_CAP_FS_WRITE     = 1ULL <<  1,
    COS_V60_CAP_NETWORK      = 1ULL <<  2,
    COS_V60_CAP_EXEC         = 1ULL <<  3,  /* fork / exec / spawn       */
    COS_V60_CAP_DLSYM        = 1ULL <<  4,  /* dynamic symbol resolve    */
    COS_V60_CAP_MMAP_EXEC    = 1ULL <<  5,  /* mmap with PROT_EXEC       */
    COS_V60_CAP_TOOL_CALL    = 1ULL <<  6,  /* generic agent tool invoke */
    COS_V60_CAP_MODEL_CALL   = 1ULL <<  7,  /* LLM forward pass          */
    COS_V60_CAP_SELF_MODIFY  = 1ULL <<  8,  /* v56 IP-TTT weight update  */
    COS_V60_CAP_SOCKET_LOCAL = 1ULL <<  9,  /* AF_UNIX only              */
    COS_V60_CAP_CREDENTIAL   = 1ULL << 10,  /* read secret material      */
    COS_V60_CAP_KV_EVICT     = 1ULL << 11,  /* v58 σ-Cache mutation      */
    COS_V60_CAP_BUDGET_EXT   = 1ULL << 12,  /* v59 σ-Budget EXPAND       */
    /* 13-31 reserved for named caps.
     * 32-63 reserved for user-defined. */
};

/* A single authorisation request.  64-byte aligned by layout. */
typedef struct {
    uint64_t action_id;         /* caller-chosen opaque                    */
    uint64_t required_caps;     /* bitmask of COS_V60_CAP_*                */
    uint64_t arg_hash_at_entry; /* 64-bit fold over the argument tuple     */
    uint64_t arg_hash_at_use;   /* recomputed right before validation      */
    float    epistemic;         /* reducible intent uncertainty            */
    float    aleatoric;         /* irreducible intent uncertainty          */
    uint32_t _reserved[2];
    uint64_t _pad[2];
} cos_v60_request_t;  /* sizeof = 64                                       */

/* Policy knobs — all M-tier, no learned parameters. */
typedef struct {
    float    sigma_high;       /* ε + α ≥ this → "high-σ" regime            */
    float    alpha_dominance;  /* α/(ε+α) ≥ this AND high-σ → DENY_INTENT   */
    uint64_t sticky_deny_mask; /* caps that are always denied (e.g. DLSYM)  */
    uint64_t always_caps;      /* caps granted to all holders (bootstrap)   */
    int32_t  enforce_integrity;/* 1 = check code_page_hash == baseline      */
    int32_t  _pad;
} cos_v60_policy_t;

/* Decisions — four bytes, one-hot by design. */
enum {
    COS_V60_ALLOW          = 1,
    COS_V60_DENY_CAP       = 2,
    COS_V60_DENY_INTENT    = 3,
    COS_V60_DENY_TOCTOU    = 4,
    COS_V60_DENY_INTEGRITY = 5,
};

typedef struct {
    uint64_t action_id;
    uint64_t missing_caps;     /* required_caps & ~(holder_caps|always)    */
    uint8_t  decision;
    uint8_t  reason_bits;      /* bitmask: see COS_V60_REASON_*             */
    uint8_t  _pad[6];
    float    sigma_total;
    float    alpha_fraction;
} cos_v60_result_t;

/* Reason bits are OR-able.  A single decision may have multiple reasons
 * (e.g. caps missing AND intent α-dominated).  `decision` is the
 * first-firing reason by priority order below; `reason_bits` names all
 * conditions that would have denied independently. */
enum {
    COS_V60_REASON_CAP       = 1u << 0,
    COS_V60_REASON_INTENT    = 1u << 1,
    COS_V60_REASON_TOCTOU    = 1u << 2,
    COS_V60_REASON_INTEGRITY = 1u << 3,
    COS_V60_REASON_STICKY    = 1u << 4,
};

/* Default policy: σ_high = 1.0, α_dom = 0.65, sticky-deny = {DLSYM,
 * MMAP_EXEC, SELF_MODIFY}, always-caps = CAP_NONE, enforce_integrity = 1. */
void cos_v60_policy_default(cos_v60_policy_t *out);

/* Branchless authorise.  Priority (by mask cascade):
 *   1. integrity drift           → DENY_INTEGRITY
 *   2. sticky deny overlap       → DENY_CAP (with REASON_STICKY)
 *   3. cap subset check fails    → DENY_CAP
 *   4. TOCTOU mismatch           → DENY_TOCTOU
 *   5. high-σ ∧ α-dominant       → DENY_INTENT
 *   6. else                      → ALLOW
 *
 * No `if` on the hot path.  Constant-time hash equality (XOR fold +
 * reduction — no early exit).  Code-integrity check is a single
 * equality between `code_page_hash` and `baseline` (pre-computed by
 * the caller via `cos_v60_code_hash`; v60 does not open /proc or
 * /dev/mem).
 *
 * Preconditions:  req, p, out != NULL.  Returns 0 on success, -1 on
 * NULL arg.
 */
int cos_v60_authorize(const cos_v60_request_t *req,
                      uint64_t                 holder_caps,
                      uint64_t                 code_page_hash,
                      uint64_t                 baseline_hash,
                      const cos_v60_policy_t  *p,
                      cos_v60_result_t        *out);

/* Batch authorise.  Walks `reqs[0..n)` with prefetch 16 ahead.  Each
 * result written to `results_out[i]`.  Safe no-op if any pointer is
 * NULL or n <= 0. */
void cos_v60_authorize_batch(const cos_v60_request_t *reqs,
                             int32_t                  n,
                             uint64_t                 holder_caps,
                             uint64_t                 code_page_hash,
                             uint64_t                 baseline_hash,
                             const cos_v60_policy_t  *p,
                             cos_v60_result_t        *results_out);

/* 64-bit XOR-fold hash over `bytes`.  Used both as arg_hash and as a
 * cheap code-page hash seed.  This is NOT a cryptographic hash — it is
 * a fast collision-resistant *equality* check for TOCTOU detection
 * and drift detection.  For crypto use blake3 upstream.
 *
 * Constant-time over `n`: one 64-bit load + rotate per stride; no
 * early exit on any input.
 */
uint64_t cos_v60_hash_fold(const void *bytes, size_t n, uint64_t seed);

/* Constant-time 64-bit equality.  Returns 1 on equal, 0 on different.
 * No branching.  Exactly the pattern a MAC verifier should use. */
int cos_v60_ct_equal64(uint64_t a, uint64_t b);

/* Build a decision-summary tag string.  Returns a stable pointer into
 * static storage.  Unknown bytes return "?". */
const char *cos_v60_decision_tag(uint8_t d);

/* 64-byte aligned request-buffer allocator; zero-inited.  Caller frees. */
cos_v60_request_t *cos_v60_alloc_requests(int32_t n);

/* Version triple (major, minor, patch). */
typedef struct { int32_t major, minor, patch; } cos_v60_version_t;
cos_v60_version_t cos_v60_version(void);

#ifdef __cplusplus
}
#endif
#endif /* CREATION_OS_V60_SIGMA_SHIELD_H */
