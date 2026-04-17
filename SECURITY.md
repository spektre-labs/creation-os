# Creation OS — Security Policy

Creation OS is an **offline, zero-telemetry, formally-verified-where-possible**
AGI kernel.  Security is not a layer; it is the architecture.  This document
states the reporting process, the supported surface, the guarantees we
actually make (and the ones we do not), and the active hardening controls.

## Supported versions

The merge-gate guarantees runtime-checked correctness (M-tier) on the
main branch, head-of-tree.  Older tags are **not** maintained for
security fixes; if you depend on an older tag, pin the commit, own the
patch.  The σ-labs (v56–v61) have no network code path by construction.

| Version | Status | Merge-gate | Security lab |
| --- | --- | --- | --- |
| `main` | supported | yes | σ-Shield (v60) + Σ-Citadel (v61) + Reasoning Fabric (v62) + σ-Cipher (v63) + σ-Intellect (v64) M-tier |
| tagged v60 and earlier | reproducible only | yes at tag time | — |

## Reporting a vulnerability

Write the repository owner; do not open a public issue.  Include:

1. A minimal reproducer (as a `make`-runnable target if possible).
2. Commit hash you reproduced on.
3. Your threat-model assumption (what capability the attacker has).
4. Impact (denial-of-service? capability escape? intent-gate bypass?).

We respond within 72 h with an acknowledgement and an advisory tier.
No bug-bounty program, no coordinated-disclosure SLA beyond that
72 h acknowledgement — we are a small kernel team.

## What is guaranteed (and what is not)

### Tier semantics (v57 dialect)

| Tier | Meaning |
| --- | --- |
| **F** — Formally proven | Frama-C / TLA+ / Coq artefact; machine-checked |
| **M** — Runtime-checked | Deterministic self-test in `make check-*` |
| **I** — Interpreted | Documented invariant, not auto-checked |
| **P** — Planned | Roadmap item; no claim today |

### Active guarantees (M-tier minimum)

- **v60 σ-Shield** (`make check-v60`, 81 tests, every decision surface).
  Branchless capability authorise + σ-gated intent + TOCTOU-free args +
  code-page integrity.  Priority cascade is exhaustive and `reason_bits`
  is multi-cause honest.
- **v61 Σ-Citadel** (`make check-v61`, 61 tests; `make chace` for the
  full composed menu).  Branchless Bell-LaPadula + Biba + MLS-compartment
  lattice; deterministic 256-bit attestation quote with constant-time
  equality; libsodium BLAKE2b-256 opt-in (`COS_V61_LIBSODIUM=1`).
  Ships seL4 CAmkES contract, Wasmtime sandbox harness, eBPF LSM
  policy, Darwin sandbox-exec profile, OpenBSD pledge stub, Nix
  reproducible recipe, distroless Dockerfile, Sigstore sign hook,
  SLSA v1.0 predicate emitter — all dispatched by `make chace` which
  PASSes present layers and **SKIPs missing ones honestly**, never
  silently downgrading.
- **v62 Reasoning Fabric** (`make check-v62`, 68 tests).  Six branchless
  C kernels distilled from the 2026 reasoning frontier — Coconut latent
  CoT, Energy-Based Transformer verifier, Hierarchical Reasoning Model
  H/L loop, Native Sparse Attention (compress + select + slide),
  DeepSeek-V3 Multi-Token Predictor draft + verify, ARKV adaptive
  ORIG/QUANT/EVICT KV manager.  Composes with v60 + v61 as a 3-bit
  branchless decision (`cos_v62_compose_decision`) so **no reasoning
  step emits unless σ-Shield, Σ-Citadel and the EBT verifier all
  ALLOW**.  ASAN clean (`make asan-v62`).  UBSAN clean (`make
  ubsan-v62`).  Hardened-build clean (`make standalone-v62-hardened`).
  Apple-tier `cos` CLI: `./cos`, `cos sigma`, `cos verify`, `cos chace`,
  `cos think <prompt>` (single C binary, no deps, NO_COLOR-respecting).
- **v63 σ-Cipher** (`make check-v63`, 144 tests).  Dependency-free C
  kernel shipping BLAKE2b-256 (RFC 7693), HKDF-BLAKE2b (RFC 5869),
  ChaCha20-Poly1305 AEAD (RFC 8439), X25519 (RFC 7748), constant-time
  equality, secure-zero, an **attestation-bound sealed envelope**
  (key = HKDF over the v61 256-bit quote + nonce + context, so a trace
  only decrypts on a host whose committed runtime state matches), a
  forward-secret **symmetric ratchet**, and an IK-like **session
  handshake** with BLAKE2b chaining key.  Composes with v60 + v61 +
  v62 as a 4-bit branchless decision (`cos_v63_compose_decision`) so
  **no sealed message is emitted unless σ-Shield, Σ-Citadel, the EBT
  verifier _and_ the AEAD tag + quote binding all ALLOW**.  All X25519
  signed-shifts rewritten to `carry * ((int64_t)1 << N)` for UBSAN
  cleanliness.  ASAN clean (`make asan-v63`).  UBSAN clean
  (`make ubsan-v63`).  Hardened-build clean
  (`make standalone-v63-hardened`).  Apple-tier `cos` CLI surface:
  `cos seal <path> [--context CTX]`, `cos unseal <path>
  [--context CTX]`, `cos sigma` (now a four-kernel verdict).
  Optional `COS_V63_LIBSODIUM=1` delegates the six primitives to
  libsodium's Apple AArch64 assembly; optional `COS_V63_LIBOQS=1`
  reserves the ML-KEM-768 hybrid slot (Signal SPQR / reishi-handshake
  pattern).  Absent opt-ins report `SKIP` honestly; the portable path
  is never silently claimed as libsodium- or PQ-verified.
- **v64 σ-Intellect** (`make check-v64`, **260 tests**).
  Dependency-free, branchless, Q0.15 integer C kernel shipping the
  2026 agentic frontier as six composable subsystems:
  **MCTS-σ** PUCT search (Empirical-MCTS, arXiv:2602.04248;
  rStar-Math) with EBT energy prior and integer isqrt;
  **Skill library** with 32-byte σ-signature Hamming retrieval
  (EvoSkill, arXiv:2603.02766; Voyager), constant-time scan, no FP;
  **Tool authz** (Dynamic ReAct, arXiv:2509.20386) — schema + caps +
  σ + **TOCTOU-safe** arg-hash binding, branchless priority cascade,
  multi-cause honest reason bits; **Reflexion ratchet** (ERL,
  arXiv:2603.24639; ReflexiCoder, arXiv:2603.05863) — integer Δσ
  update with ratio-preserving overflow shift; **AlphaEvolve-σ** —
  BitNet-b1.58 ternary mutation (arXiv:2402.17764) with σ-gated
  accept-or-rollback; **MoD-σ** — per-token depth routing
  (arXiv:2404.02258; MoDA arXiv:2603.15619; A-MoD arXiv:2412.20875).
  Composes with v60 + v61 + v62 + v63 as a **5-bit branchless
  decision** (`cos_v64_compose_decision`) so **no tool call or
  reasoning step emits unless σ-Shield, Σ-Citadel, the EBT verifier,
  the AEAD tag + quote binding, _and_ the agentic intellect all
  ALLOW**.  Measured on M-series: ~674 k MCTS iters/s, ~1.4 M skill
  retrieves/s, **~517 M tool-authz decisions/s**, ~5.1 GB/s MoD-σ
  routing.  ASAN clean (`make asan-v64`).  UBSAN clean (`make
  ubsan-v64`).  Hardened-build clean (`make standalone-v64-hardened`).
  Apple-tier `cos` CLI surface: `cos mcts`, `cos decide v60 v61 v62
  v63 v64`, `cos sigma` (now a five-kernel verdict).  Zero optional
  dependencies on the hot path — the kernel is libc-only.
- **v47 Frama-C architecture** (F-tier, where active).
- **v48 red-team harness** (M-tier; 0/342 bypasses at last run).
- **v49 DO-178C DAL-A artefacts** (I-tier; generated on demand).
- **Hardened build profile** (`make harden`): OpenSSF 2026 hardening
  flags + `-mbranch-protection=standard` + PIE.
- **Sanitizer matrix** (`make sanitize`): ASAN on v58 / v59 / v60 / v61 / v62 / v63 / v64 +
  UBSAN on v60 / v61 / v62 / v63 / v64, all passing their own self-tests under sanitizer.
- **Layered secret scan** (`make security-scan`): gitleaks when
  installed, grep-only fallback always; allowlist in `.gitleaks.toml`.
- **SBOM** (`make sbom` → `SBOM.json`): CycloneDX-lite 1.5 per
  component (`src/v*/`), SHA-256 pinned.

### Explicit non-guarantees

- **Not a sandbox in itself.**  v60 σ-Shield is a capability *gate*.
  The process still runs in its host address space; for sandbox-grade
  isolation compose with seL4 / WASM / Fuchsia upstream — v61 ships
  contracts (CAmkES, Wasmtime harness, sandbox-exec, pledge) but the
  external toolchains are not re-implemented inside this repo.
- **Default v61 quote is not a MAC.**  The XOR-fold-256 fallback is
  deterministic and equality-sensitive, nothing more.  Enable
  `COS_V61_LIBSODIUM=1` for BLAKE2b-256 via libsodium `crypto_generichash`.
- **Not a zero-day detector.**  σ-Shield gates on **caller-provided
  σ, caller-provided required-caps, caller-provided arg-hash**.  A
  compromised σ producer can still issue an ALLOW; that is why v56
  VPRM and v54 σ-proconductor exist — producing σ with independent
  evidence lines.
- **v63 is not a network transport.**  σ-Cipher seals messages
  between kernels in a single address space (or on the same filesystem
  if an operator chooses to persist an envelope); it is _not_ TLS and
  does not authenticate endpoints over a network.  For network
  delivery, wrap the envelope in a reviewed transport (WireGuard,
  Noise-XX, etc.).
- **v63 does not guarantee memory encryption.**  Plaintext is visible
  in process memory while a message is being constructed or consumed;
  pair with Secure Enclave / TDX / SEV-SNP for RAM-level confidentiality.
- **v63 PQ-hybrid is opt-in.**  Without `COS_V63_LIBOQS=1`, the PQ
  slot reports `SKIP` — the non-PQ path is never silently claimed as
  post-quantum.
- **v64 is not a language model.**  σ-Intellect is the control plane
  *around* a language model: it decides *when* to plan, *which*
  skill to retrieve, *whether* a tool call is safe, *how much* depth
  to spend on a token.  It does not generate tokens.  Wire it above
  an MLX / Core ML inference path for end-to-end operation.
- **v64 is not a training loop.**  AlphaEvolve-σ accepts ternary
  mutations only when σ and energy both strictly improve.  This is a
  per-deployment adaptation slot, not a gradient step.
- **v64 skill library is flat.**  The default 1024-skill arena is
  constant-time in size but not hierarchical.  A mmap-backed,
  hierarchical 10⁶-skill library is designed (P-tier) but not shipped.
- **No TPM / Secure Enclave** integration yet (P-tier).  Code-page
  baseline hashes are caller-provided.
- **No formal proof of σ-Shield** yet (F-tier).  Frama-C annotations
  are a planned follow-up.
- **No network code path** on any σ-lab (v56-v61).  If a future lab
  needs network, it will ship with ShieldNet-style MITM + σ gating as
  a separate module.
- **CHERI capability pointers** are documented as a target but not
  active: Apple M4 has no CHERI path.  When Morello-class hardware is
  available the v61 lattice is designed to lift onto CHERI caps.
- **SLSA-3** requires Sigstore Rekor inclusion via OIDC; that is
  wired in `.github/workflows/slsa.yml` but **`make slsa` locally**
  only emits a SLSA v1.0-shaped predicate stub for inspection.

## Threat model

See [`THREAT_MODEL.md`](THREAT_MODEL.md) for the STRIDE decomposition
against Q2 2026 attack classes (DDIPE, ClawWorm, Malicious
Intermediary).  Summary: v60 refuses α-dominated intent *regardless*
of capability, closing the class of "looks-legitimate" attacks that
beat signature defenses.

## Local-dev hardening (developer quick-start)

```
make harden            # OpenSSF 2026 flags + ARM64 branch-protection (v57..v61)
make sanitize          # ASAN + UBSAN matrix runs v58/v59/v60/v61 self-tests
make hardening-check   # verify PIE / canaries / fortify on harden build
make security-scan     # gitleaks + grep-fallback + hardcoded URL check
make sbom              # emit SBOM.json (CycloneDX 1.5)
make reproducible-build  # double-build digest compare

# v61 CHACE composition — one command for the whole defence-in-depth menu
make chace             # seL4 + Wasmtime + eBPF + sandbox-exec + hardening
                       # + sanitizer + SBOM + scan + repro + attest + sign
                       # + slsa + distroless.  PASS / honest-SKIP / FAIL.
make attest            # ATTESTATION.json (v61 quote; optional cosign sign)
make slsa              # PROVENANCE.json (SLSA v1.0 predicate stub)
```

Pre-commit hooks:

```
pip install pre-commit
pre-commit install
pre-commit run --all-files
```

The hook runs `scripts/security/scan.sh`, a minimal whitespace / EOL
sweep, and a reject-`.env` guard.

## Supply-chain posture

- **AGPL-3.0-or-later** on every source file (SPDX headers).
- **No runtime dependencies** on any v56-v61 lab (pure C11 + libc;
  libsodium is opt-in for v61 attestation strength).
- **SBOM** ships per-component in `SBOM.json` (CycloneDX 1.5).
- **Reproducible builds**: `make reproducible-build` double-builds and
  compares SHA-256.  Nix-based reproducible build recipe in
  `nix/v61.nix`.  SLSA-3 provenance ships via
  `.github/workflows/slsa.yml` using `slsa-github-generator` + keyless
  OIDC Sigstore signing on release.
- **Distroless runtime**: `Dockerfile.distroless` copies the single
  static binary into `gcr.io/distroless/cc-debian12:nonroot` — no
  shell, no package manager.
- **.gitleaks.toml** allowlists only explicit docs / fixtures; any
  accidental secret blocks the pre-commit hook.
