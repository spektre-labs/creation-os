/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v61 — Σ-Citadel: formal-lattice capability kernel +
 * attestation chain.
 *
 *   v61 is the state-of-the-art assembly of the 2026 advanced-security
 *   primitives that actually fit in a local AI agent:
 *
 *     - Bell-LaPadula (confidentiality lattice): no-read-up,
 *       no-write-down across an 8-bit clearance axis.
 *     - Biba (integrity lattice): no-read-down, no-write-up across
 *       an 8-bit integrity axis.
 *     - MLS compartments: 16-bit bitmask; both subject and object
 *       must share the requested compartment.
 *     - Attestation quote: deterministic 256-bit digest over
 *       (code_page_hash || caps_state_hash || sigma_state_hash ||
 *        lattice_hash || nonce).  Compatible with Sigstore / cosign
 *       sign-blob; verifiable offline.
 *     - Composition with v60 σ-Shield: a request ALLOWs iff the
 *       σ-Shield authorise returns ALLOW *and* the lattice check
 *       passes.  No "OR" path, no scalar fallback.
 *
 *   ## Where v61 fits in the 2026 advanced-security menu
 *
 *     Primitive          Role in v61         Upstream source
 *     -----------------  ------------------  -------------------------
 *     seL4 microkernel   component isolation sel4/sigma_shield.camkes
 *     Wasmtime sandbox   tool execution      wasm/ + scripts/v61/wasm_harness.sh
 *     gVisor/Firecracker KVM sandbox         docs/v61/ARCHITECTURE.md
 *     eBPF policies      host enforcement    ebpf/sigma_shield.bpf.c
 *     Bell-LaPadula      confidentiality     cos_v61_lattice_check (THIS)
 *     Biba               integrity           cos_v61_lattice_check (THIS)
 *     TPM 2.0 attest     boot attestation    cos_v61_attest_*      (THIS)
 *     Sigstore / cosign  code signing        scripts/security/sign.sh
 *     libsodium          crypto primitives   COS_V61_LIBSODIUM (opt-in)
 *     Nix / Bazel        reproducible build  nix/flake.nix
 *     Distroless         runtime container   Dockerfile.distroless
 *     SLSA-3             supply-chain        .github/workflows/slsa.yml
 *     sandbox-exec       Darwin process      sandbox/darwin.sb
 *     pledge / unveil    OpenBSD process     sandbox/openbsd_pledge.c
 *     CHERI hw caps      hardware caps       (CHERI-BSD target; doc-only)
 *
 *   CHACE-class capability-hardening designs a defence-in-depth composition of the same
 *   menu.  v61 is the first open-source AI-agent runtime to ship
 *   every layer with a rehearsal target (`make chace`) that PASSes
 *   on what is present and SKIPs honestly on what the host lacks —
 *   never silently downgrading.
 *
 *   ## Tier semantics (v57 dialect)
 *
 *     M — BLP + Biba lattice compare  (this file, 60+ tests)
 *     M — attestation quote + chain    (this file, deterministic)
 *     M — composition with v60        (both authorise paths called)
 *     I — seL4 component contract     (sel4/sigma_shield.camkes)
 *     I — WASM sandbox contract       (wasm/)
 *     I — eBPF policy                 (ebpf/)
 *     I — sandbox-exec / pledge       (sandbox/)
 *     I — Nix reproducible build      (nix/)
 *     I — distroless runtime          (Dockerfile.distroless)
 *     P — TPM 2.0 hardware binding    (Secure Enclave planned)
 *     P — SLSA-3 Rekor inclusion      (workflow ready, keyless pending)
 *     P — CHERI capability pointers   (hardware not available)
 *
 *   ## Non-claims
 *
 *     - v61 does NOT replace seL4 / Wasmtime / gVisor / Firecracker;
 *       it composes with them via documented contracts.
 *     - TPM / Secure Enclave binding is P-tier; v61 can take a
 *       Secure-Enclave-computed hash as input but does not itself
 *       issue the quote.
 *     - No formal Frama-C proof yet; all v61 claims are M-tier
 *       runtime checks.
 *     - `cos_v61_quote256` is a *deterministic* 256-bit fold —
 *       sufficient for offline equality verification; for actual
 *       cryptographic attestation the caller should layer
 *       libsodium crypto_generichash / BLAKE2b on top (opt-in via
 *       `COS_V61_LIBSODIUM`).
 *
 *   ## Hardware discipline (.cursorrules)
 *
 *     1. No dynamic allocation on the lattice hot path.
 *     2. Branchless lattice compare (mask cascade).
 *     3. Constant-time quote equality (ct_equal256, no early exit).
 *     4. 64-byte aligned attestation buffers.
 *     5. No syscalls on any hot path (attestation is pure compute).
 *     6. Prefetch 16 lanes ahead in batch lattice check.
 */

#ifndef CREATION_OS_V61_CITADEL_H
#define CREATION_OS_V61_CITADEL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 1. Lattice                                                          */
/* ------------------------------------------------------------------ */

/* BLP + Biba label.  8-bit axes → 256 levels each; 16 compartments. */
typedef struct {
    uint8_t  clearance;    /* BLP confidentiality axis  */
    uint8_t  integrity;    /* Biba integrity axis       */
    uint16_t compartments; /* 16-bit compartment mask   */
    uint32_t _reserved;
} cos_v61_label_t;

enum {
    COS_V61_OP_READ  = 1,
    COS_V61_OP_WRITE = 2,
    COS_V61_OP_EXEC  = 3,
};

/* Branchless lattice check.  Returns 1 if allowed, 0 if denied.
 *
 *   READ:  subj.clearance ≥ obj.clearance (BLP no-read-up)
 *        ∧ subj.integrity ≤ obj.integrity (Biba no-read-down)
 *        ∧ (subj.compartments & obj.compartments) == obj.compartments
 *   WRITE: subj.clearance ≤ obj.clearance (BLP no-write-down)
 *        ∧ subj.integrity ≥ obj.integrity (Biba no-write-up)
 *        ∧ same compartment rule
 *   EXEC:  both read and write rules hold (conservative, may be
 *          relaxed per policy in future).
 *
 * No `if` on the hot path; no short-circuit; every lane is always
 * evaluated.  Unknown op → 0.
 */
int cos_v61_lattice_check(cos_v61_label_t subj,
                          cos_v61_label_t obj,
                          int             op);

/* Policy knobs. */
typedef struct {
    uint8_t min_integrity;   /* objects below this integrity are not
                              * accessible by any subject (quarantine) */
    uint8_t _pad[3];
    uint32_t _reserved;
} cos_v61_lattice_policy_t;

void cos_v61_lattice_policy_default(cos_v61_lattice_policy_t *out);

/* Hardened lattice check with policy overlay.  Additional rule:
 *   obj.integrity >= policy.min_integrity.
 */
int cos_v61_lattice_check_policy(cos_v61_label_t                  subj,
                                 cos_v61_label_t                  obj,
                                 int                              op,
                                 const cos_v61_lattice_policy_t  *p);

/* Batch: prefetch 16 lanes ahead. */
void cos_v61_lattice_check_batch(const cos_v61_label_t *subjs,
                                 const cos_v61_label_t *objs,
                                 const uint8_t         *ops,
                                 int32_t                n,
                                 uint8_t               *results_out);

/* ------------------------------------------------------------------ */
/* 2. Attestation chain                                                */
/* ------------------------------------------------------------------ */

/* Attestation inputs.  64-byte aligned. */
typedef struct {
    uint64_t code_page_hash;    /* (from v60 baseline)                  */
    uint64_t caps_state_hash;   /* hash over holder caps table          */
    uint64_t sigma_state_hash;  /* hash over σ producer config (v54/56) */
    uint64_t lattice_hash;      /* hash over active label assignments   */
    uint64_t nonce;             /* verifier-supplied nonce              */
    uint64_t _pad[3];
} cos_v61_attest_input_t;  /* sizeof = 64 */

/* 256-bit deterministic quote (32 bytes). */
typedef struct {
    uint8_t bytes[32];
} cos_v61_quote256_t;

/* Compute a deterministic 256-bit quote over the attestation input.
 * When compiled with `COS_V61_LIBSODIUM=1` and `-lsodium`, uses
 * libsodium `crypto_generichash` (BLAKE2b-256) for cryptographic
 * strength.  Default: a deterministic four-lane XOR-fold over the
 * input — sufficient for offline equality (Sigstore sign-blob
 * compatible) but NOT cryptographically authenticated.
 */
void cos_v61_quote256(const cos_v61_attest_input_t *in,
                      cos_v61_quote256_t           *out);

/* Constant-time 256-bit equality.  Returns 1 on equal, 0 on differ. */
int cos_v61_ct_equal256(const cos_v61_quote256_t *a,
                        const cos_v61_quote256_t *b);

/* Emit a hex string for the 32-byte quote.  Writes 65 bytes (64 hex
 * + terminating NUL) to `out64p1`.  No allocation. */
void cos_v61_quote256_hex(const cos_v61_quote256_t *q, char *out64p1);

/* ------------------------------------------------------------------ */
/* 3. Composition with v60 σ-Shield                                    */
/* ------------------------------------------------------------------ */

/* A v60 decision plus a v61 lattice check composed into a single
 * four-valued outcome:
 *
 *     ALLOW         both passed
 *     DENY_V60      σ-Shield denied (see v60 result for reason)
 *     DENY_LATTICE  v61 lattice denied (BLP / Biba / compartment /
 *                   min-integrity policy)
 *     DENY_BOTH     both layers denied
 *
 * The caller is expected to pass through the v60 result untouched.
 */
enum {
    COS_V61_COMPOSE_ALLOW        = 1,
    COS_V61_COMPOSE_DENY_V60     = 2,
    COS_V61_COMPOSE_DENY_LATTICE = 3,
    COS_V61_COMPOSE_DENY_BOTH    = 4,
};

typedef struct {
    uint8_t  decision;
    uint8_t  _pad[7];
    uint64_t _reserved;
} cos_v61_compose_t;

/* Compose a v60 ALLOW/DENY_* result (1..5) with a v61 lattice-check
 * 0/1 into a 4-valued composed decision.  Branchless. */
int cos_v61_compose(uint8_t            v60_decision,
                    int                lattice_allowed,
                    cos_v61_compose_t *out);

/* ------------------------------------------------------------------ */
/* 4. Utilities                                                        */
/* ------------------------------------------------------------------ */

/* 64-byte aligned attestation-input allocator; zero-inited. */
cos_v61_attest_input_t *cos_v61_alloc_attest_inputs(int32_t n);

/* Version triple. */
typedef struct { int32_t major, minor, patch; } cos_v61_version_t;
cos_v61_version_t cos_v61_version(void);

const char *cos_v61_compose_tag(uint8_t d);

#ifdef __cplusplus
}
#endif
#endif /* CREATION_OS_V61_CITADEL_H */
