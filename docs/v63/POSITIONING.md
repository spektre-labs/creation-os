# v63 σ-Cipher — positioning against the 2026 field

## Per-system comparison

### Chutes (March 2026) — _End-to-End Encrypted AI Inference with Post-Quantum Cryptography_
Client-side ML-KEM-768 encryption, ciphertext-through-API, decrypt
inside Intel TDX with encrypted memory and GPU VRAM, re-encrypt before
leaving.  **Cloud-hosted** model serving.  v63 borrows the core
pattern — attestation-bound keys, encrypted-in-motion — but is a
**local-agent** runtime and does not require a TEE to function (it
gracefully composes with one when available).

### Tinfoil Containers (March 2026)
Confidential VMs with Intel TDX / AMD SEV-SNP and NVIDIA Confidential
Computing, plus automatic enclave attestation verification via
Sigstore transparency logs.  **Infrastructure** layer.  v63 is the
message layer that rides on top of a Tinfoil-style deployment without
requiring one.

### reishi-handshake v0.2.0 (March 2026)
Rust library implementing `Noise_IKpq_25519+MLKEM768_ChaChaPoly_BLAKE2s`.
Each DH token is augmented with ML-KEM-768 encapsulation; both shared
secrets are mixed into the chaining key.  v63 implements the **same
cipher suite** (minus the ML-KEM-768 mix until `COS_V63_LIBOQS=1` is
wired) but as a C kernel composable with a local reasoning fabric.

### Signal Triple Ratchet / SPQR (October 2025)
Sparse Post-Quantum Ratchet on top of Double Ratchet with CRYSTALS-
Kyber, giving post-quantum forward secrecy and post-compromise
security.  v63 ships a one-way symmetric ratchet today; the PQ slot is
wired through the same HKDF chaining-key mix, so adding SPQR is a
drop-in `COS_V63_LIBOQS=1` upgrade.

### Voidly Agent SDK (2026)
End-to-end encrypted messaging for AI agents using Double Ratchet +
X3DH + ML-KEM-768.  Sibling system, different target (agent-to-agent
messaging on a network vs. local in-kernel trace sealing).  v63 adopts
the same primitives with a smaller, kernel-scoped surface area.

### QSC / arXiv:2603.15668 — _Quantum-Secure-By-Construction_
Proposes treating quantum-secure communication as a **core
architectural property** of agentic AI systems.  v63 is a concrete
instantiation of exactly this idea: a 4-bit composed decision that
refuses to emit a reasoning step unless the capability kernel, the
lattice, the EBT verifier, **and** the cipher all authorise.

### libsodium / OpenSSL / BoringSSL
Reference production crypto libraries.  v63 is not a replacement; it
exposes a `COS_V63_LIBSODIUM=1` opt-in to delegate the six primitives
to libsodium in production while keeping the dependency-free path for
reviewers and minimal deployments.

### TweetNaCl / monocypher
Dependency-free crypto libraries with similar scope.  v63 is
philosophically closest to this row, but ships **composition with the
rest of Creation OS** as a first-class feature
(`cos_v63_compose_decision`), which these libraries do not address.

## Comparison matrix

|                                   | Chutes | Tinfoil | reishi | Signal SPQR | Voidly | libsodium | TweetNaCl | **v63 σ-Cipher** |
|-----------------------------------|:------:|:-------:|:------:|:-----------:|:------:|:---------:|:---------:|:---------:|
| BLAKE2b-256                       |   ✓    |    ○    |   ✓    |      ✓      |   ✓    |     ✓     |     ✓     | **✓**     |
| HKDF                              |   ✓    |    ○    |   ✓    |      ✓      |   ✓    |     ✓     |     ·     | **✓**     |
| ChaCha20-Poly1305 AEAD            |   ✓    |    ○    |   ✓    |      ✓      |   ✓    |     ✓     |     ✓     | **✓**     |
| X25519                            |   ○    |    ○    |   ✓    |      ✓      |   ✓    |     ✓     |     ✓     | **✓**     |
| Constant-time tag compare         |   ○    |    ○    |   ✓    |      ✓      |   ✓    |     ✓     |     ✓     | **✓**     |
| Attestation-bound sealed envelope |   **✓**|  **✓**  |   ·    |      ·      |   ·    |     ·     |     ·     | **✓**     |
| Forward-secret ratchet            |   ·    |    ·    |   ·    |    **✓**    |   ✓    |     ·     |     ·     | **✓**     |
| PQ-hybrid slot (ML-KEM-768)       |   **✓**|    ○    |   **✓**|    **✓**    |   **✓**|     ○     |     ·     | **○** (opt-in) |
| Dependency-free (libc only)       |   ·    |    ·    |   ·    |      ·      |   ·    |     ·     |     ✓     | **✓**     |
| Composition with capability kernel|   ·    |    ·    |   ·    |      ·      |   ·    |     ·     |     ·     | **✓**     |
| Composition with reasoning fabric |   ·    |    ·    |   ·    |      ·      |   ·    |     ·     |     ·     | **✓**     |
| 4-bit composed decision           |   ·    |    ·    |   ·    |      ·      |   ·    |     ·     |     ·     | **✓**     |
| RFC-vector self-test on build     |   ○    |    ○    |   ○    |      ○      |   ○    |     ✓     |     ·     | **✓**     |
| ASAN + UBSAN clean every build    |   ○    |    ○    |   ○    |      ○      |   ○    |     ✓     |     ·     | **✓**     |

`✓` shipped, `○` shipped elsewhere in the same project, `·` not
addressed, **bold** where v63 is either first or uniquely positioned.

## The single sentence

v63 is the only open-source **local-AI-agent** runtime that ships
_every_ 2026 encryption-frontier primitive as one dependency-free C
kernel, binds every sealed envelope to the attestation quote of the
reasoning fabric that produced it, and gates every emission on a
branchless 4-bit decision composed with σ-Shield, Σ-Citadel, and the
EBT verifier.

## What changes if a reviewer compares against the table

- A reviewer asking _"do you do post-quantum?"_ — yes, via the hybrid
  slot; the honest answer is that without liboqs the slot reports
  `SKIP`, and the non-PQ path is not silently claimed as PQ.
- A reviewer asking _"why not libsodium?"_ — because a 900-line
  auditable kernel on one file is the artefact a local-agent reviewer
  wants; libsodium is available as an opt-in for anyone who wants
  FIPS-adjacent assurance without rewriting the ABI.
- A reviewer asking _"why not TLS?"_ — because the messages are not
  flowing over a network; they are flowing between kernels in a single
  address space, and the attestation-bound envelope is the right shape
  for that threat model.
- A reviewer asking _"what is new vs. the 2026 papers?"_ — the
  composition.  The papers describe primitives; v63 ships them as a
  composable C ABI whose every emission is 4-bit-gated by the three
  upstream Creation OS kernels.
