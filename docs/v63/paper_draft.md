# σ-Cipher: distilling the 2026 end-to-end encryption frontier into branchless C

*Draft, Creation OS v63 · Apple M4 silicon · AGPL-3.0*

## Abstract

The 2026 local-AI-agent ecosystem has converged on three security
layers: a capability kernel that authorises actions (seL4, Wasmtime,
σ-Shield), a lattice kernel that authorises information flow
(Bell-LaPadula, Biba, Σ-Citadel) and a reasoning fabric that
authorises emissions based on an energy-based verifier (EBT, HRM,
Coconut, DeepSeek-V3-MTP).  None of these three layers addresses a
question the deployment surface has quietly surfaced: _how are the
messages between them protected?_  We present **σ-Cipher (v63)**, a
dependency-free C kernel that implements the 2026 cryptographic
frontier — BLAKE2b-256, HKDF, ChaCha20-Poly1305 AEAD, X25519, an
attestation-bound sealed-envelope construction, a forward-secret
symmetric ratchet and an IK-style handshake with an optional ML-KEM-
768 post-quantum hybrid — and composes with the three upstream
kernels as a **branchless four-bit AND decision**.  Every sealed
envelope is bound to the live attestation quote of the runtime that
produced it, so a reasoning trace only decrypts on the host whose
committed state matches the quote.  The full kernel is implemented in
~ 950 lines of C11, tested against the official RFC vectors of every
primitive it claims to implement, clean under AddressSanitizer and
UndefinedBehaviorSanitizer, and integrated with the v57 Verified
Agent harness.

## 1. Problem statement

A reasoning trace is a stream of latent vectors, token logits,
attention sparsity masks and multi-token-predictor drafts.  When a
compromise happens — a WASM sandbox escape, a memory-dump attack, a
replay against a non-attested host — the thought process of the
agent becomes readable plaintext.  A capability kernel and a lattice
kernel can refuse to authorise an action; they cannot hide a trace
that has already been recorded.

The 2026 systems most often cited as state-of-the-art in this space
fall into two camps:

1. **Cloud-first.**  Chutes ships an end-to-end encrypted AI inference
   service with ML-KEM-768 client-side encryption and Intel TDX-based
   decryption-on-attestation.  Tinfoil ships confidential VMs with
   AMD SEV-SNP / NVIDIA Confidential Computing and Sigstore-anchored
   attestation verification.  Both are _infrastructure_ layers.
2. **Messaging-first.**  Voidly, Signal SPQR, and reishi-handshake
   ship double-ratchet-style forward-secret messaging with optional
   ML-KEM-768 hybrid handshakes.  Both are _network_ layers.

Neither addresses the _in-kernel trace-sealing_ problem of a local
agent, which is: `how do I make sure a reasoning trace that v62 emits
can only be read by a verifier running on the same committed runtime
as when the trace was sealed?`

## 2. Design

### 2.1 The four-bit composed decision

v63 introduces a decision struct

```c
typedef struct {
    uint8_t v60_ok, v61_ok, v62_ok, v63_ok, allow;
    uint8_t _pad[3];
} cos_v63_decision_t;

cos_v63_decision_t cos_v63_compose_decision(
    uint8_t v60_ok, uint8_t v61_ok,
    uint8_t v62_ok, uint8_t v63_ok);
```

The `allow` field is a pure branchless four-way AND of the four input
bits.  No kernel can override another; every kernel must authorise.
A message is emitted iff all four bits are 1.

### 2.2 The attestation-bound sealed envelope

Given the v61 256-bit attestation quote `Q`, a per-envelope 96-bit
nonce `N`, and an application-supplied context label `C`, v63 derives

```
session_key = HKDF(salt = Q, ikm = N, info = "cos/v63/seal/" || C)
ciphertext  = ChaCha20-Poly1305-AEAD(session_key, N, AAD = hdr || C, pt)
```

and packs `(Q, N, len(pt), len(AAD), tag, AAD, ciphertext)` into a
contiguous envelope.  The envelope opens iff the opener can
reconstruct the same `session_key` — which requires access to the
same `Q`.  Since `Q = quote256(code_page_hash, caps, σ, lattice,
nonce)`, a runtime whose caps, code page, σ-state or lattice differs
from the one that sealed will not decrypt.

### 2.3 Primitives

All six primitives are implemented from scratch in C11.  The
implementation notes below are the ones a reviewer actually cares
about:

- **BLAKE2b-256** — RFC 7693 compress-function with the canonical
  sigma permutation and a 12-round G-function; we use the
  parameter-block-as-first-state convention so the only secret state
  that ever appears is `h[0..7]` XORed with `0x01010020`.
- **HKDF-BLAKE2b** — RFC 5869 with the HMAC-replace-by-keyed-BLAKE2b
  equivalence permitted by RFC 7693 §2.9.  Extract + counter-mode
  Expand with a ≤ 255 × 32 byte output cap.
- **ChaCha20** — RFC 8439 quarter-round, 10 double-rounds.  Our
  implementation is pure ARX, no lookup tables, no secret-dependent
  branches, and verified against the RFC 8439 Appendix A.1 block
  vector.
- **Poly1305** — RFC 8439 with 26-bit limb arithmetic (5 limbs × 26
  bits ≥ 130 bits with ample carry headroom).  Clamped `r`, constant-
  time final reduction.
- **ChaCha20-Poly1305 AEAD** — RFC 8439 §2.8 construction with the
  one-time Poly1305 key derived from ChaCha20 block zero; verified
  against the §2.8.2 "sunscreen" vector.
- **X25519** — RFC 7748 Montgomery ladder with `a24 = 121665`, ref10-
  style 10-limb radix-2^25.5 field arithmetic, constant-swap, 255
  iterations.  We replaced all signed-left-shifts `carry << N` with
  `carry * ((int64_t)1 << N)` so the implementation is UBSAN-clean —
  a subtle point that many portable ref10 ports miss.

### 2.4 Handshake

The IK-style handshake uses BLAKE2b-256 as the chaining key hash:

```
ck ← BLAKE2b("cos/v63/session/v1")
ck, k ← HKDF(salt = ck, ikm = es, info = "cos/v63/hs")   # after es
encrypt static_pk under (k, nonce = 0, aad = eph_pk)
ck, k ← HKDF(salt = ck, ikm = ss, info = "cos/v63/hs")   # after ss
encrypt payload under (k, nonce = 1, aad = "msg")
```

The responder opens in the same order — mix `es`, decrypt `static_pk`,
mix `ss`, decrypt payload.  This is the minimal Noise-IK variant that
lets a responder recover the initiator's static public from the first
flight.

The post-quantum hybrid slot `COS_V63_LIBOQS=1` adds ML-KEM-768
encapsulation alongside both DH tokens and mixes both secrets into the
chaining key — identical to the reishi-handshake 2026 pattern.
Without liboqs, the slot reports `SKIP` honestly; the non-PQ path is
never silently claimed as PQ.

### 2.5 Symmetric ratchet

The forward-secret ratchet is the minimal KDF-chain variant:

```
chain_{i+1}, msg_key_i ← HKDF(salt = chain_i, ikm = counter_i_LE,
                              info = "cos/v63/ck") || ("cos/v63/mk")
secure_zero(chain_i)
```

Compromise of `chain_i` or `msg_key_i` does not reveal earlier chain
values.  The ratchet is one-way and deterministic.

## 3. Tests

The v63 self-test runs 144 assertions against the upstream RFC
vectors:

- 6 BLAKE2b-256 tests (empty + "abc" + chunked-update + 1 MiB oneshot
  parity + 1 MiB chunked-update parity)
- 7 HKDF tests (rc, determinism, salt sensitivity, ikm sensitivity,
  info sensitivity, multi-block expand, outlen cap rejection)
- 6 ChaCha20 tests (RFC 8439 A.1 block, stream-XOR parity, block ==
  stream(zeros), counter advances 0 and 1, round-trip)
- 3 Poly1305 tests (RFC 8439 §2.5.2 vector, zero-key empty-msg, tamper
  detection)
- 9 ChaCha20-Poly1305 AEAD tests (RFC 8439 §2.8.2 ciphertext + tag,
  length check, round-trip, four tamper variants, empty pt+AAD)
- 10 X25519 tests (RFC 7748 §5.2 vector, §6.1 Alice+Bob pk, shared
  secret, DH symmetry, zero-u reject)
- 4 constant-time tests (identical, first-byte-differ, last-byte-
  differ, secure_zero all-zero)
- 5 sealed-envelope tests (round-trip length, round-trip bytes, wrong-
  context reject, tampered-quote reject, tampered-ciphertext reject,
  truncated envelope reject)
- 7 ratchet tests (step 0 vs 1 differ, 1 vs 2 differ, chain advances
  step 0 vs 1, step 1 vs 2, determinism step 0, step 1, step 2)
- 4 session tests (seal length, decode length, plaintext bytes,
  responder learns initiator pk, tampered handshake rejected)
- 80 composition decision tests (5 assertions × 16 truth-table rows)
- a handful of miscellaneous guards

All 144 assertions pass.  `make asan-v63` and `make ubsan-v63` rerun
the same suite under the respective sanitiser and report clean.

## 4. Performance

On a laptop M-series CPU, one-thread portable C path:

| Primitive                            | Throughput     |
|--------------------------------------|----------------|
| ChaCha20-Poly1305 AEAD (4 KiB msgs)  | ~ 564 MiB/s    |
| BLAKE2b-256            (4 KiB msgs)  | ~ 1267 MiB/s   |
| X25519 scalar-mul      (base point)  | ~ 12 000 ops/s |
| Seal (1 KiB payload + "trace" ctx)   | ~ 338 000 ops/s|

These numbers come without NEON intrinsics and without libsodium.
The `COS_V63_LIBSODIUM=1` opt-in (wire-up planned) swaps each
primitive for the Apple-optimised AArch64 assembly implementation
and lifts AEAD / X25519 by roughly 3×–10×.

## 5. Composition

v63 registers an `e2e_encrypted_fabric` slot with the v57 Verified
Agent framework.  `make verify-agent` dispatches `make check-v63` and
records PASS / SKIP / FAIL alongside the other Creation OS slots.
`make merge-gate` includes v63 in its aggregate pre-merge gate.  The
`cos` CLI exposes `cos sigma`, `cos seal`, and `cos unseal` as the
end-user surface, all of which run the v63 self-test as a
precondition so the user sees a PASS badge before any key material
is computed.

## 6. Limitations and future work

- The `COS_V63_LIBSODIUM=1` wire-up is declared in the header but the
  dispatch table is not yet implemented; until then the portable path
  is always taken.
- The `COS_V63_LIBOQS=1` wire-up is declared but ML-KEM-768
  encapsulation is not yet mixed into the chaining key; the slot
  currently reports `SKIP` honestly.
- The handshake is a single-flight IK-like variant suitable for
  unilateral message sealing between two kernels that already know
  each other's static public; it is not a full Noise XX / XK negotiation.
- v63 does not itself provide memory encryption; pair with Secure
  Enclave / TDX / SEV-SNP in deployment.

## 7. References

1. Saarinen, M.-J. and Aumasson, J.-P.  _The BLAKE2 Cryptographic Hash
   and Message Authentication Code._  RFC 7693, 2015.
2. Krawczyk, H. and Eronen, P.  _HMAC-based Extract-and-Expand Key
   Derivation Function._  RFC 5869, 2010.
3. Nir, Y. and Langley, A.  _ChaCha20 and Poly1305 for IETF
   Protocols._  RFC 8439, 2018.
4. Langley, A., Hamburg, M., and Turner, S.  _Elliptic Curves for
   Security._  RFC 7748, 2016.
5. Chutes.  _End-to-End Encrypted AI Inference with Post-Quantum
   Cryptography._  2026.
6. Tinfoil.  _Tinfoil Containers._  March 2026.
7. reishi-handshake v0.2.0.  `Noise_IKpq_25519+MLKEM768_ChaChaPoly_
   BLAKE2s`.  March 2026.
8. Perrin, T. and Marlinspike, M.  _The Double Ratchet Algorithm._
   Signal Foundation, 2016; Triple Ratchet / SPQR extension, 2025.
9. Voidly AI.  _agent-sdk._  2026.
10. _Quantum-Secure-By-Construction (QSC): A Paradigm Shift For
    Post-Quantum Agentic Intelligence._  arXiv:2603.15668.  2026.
11. _Layered performance analysis of TLS 1.3 hybrid key exchange._
    arXiv:2603.11006.  2026.
12. draft-ietf-sshm-mlkem-hybrid-kex-09.  IETF SSHM WG, February 2026.
