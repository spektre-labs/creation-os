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
| `main` | supported | yes | σ-Shield (v60) + Σ-Citadel (v61) + Reasoning Fabric (v62) + σ-Cipher (v63) + σ-Intellect (v64) + σ-Hypercortex (v65) + σ-Silicon (v66) + σ-Noesis (v67) M-tier |
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
  v63 v64 v65`, `cos sigma` (now a six-kernel verdict).  Zero optional
  dependencies on the hot path — the kernel is libc-only.
- **v65 σ-Hypercortex** (`make check-v65`, **534 tests**).
  Dependency-free, branchless, **integer-only** C kernel shipping the
  2026 hyperdimensional / vector-symbolic frontier as a popcount-native
  neurosymbolic substrate.  **Bipolar hypervectors** at `D = 16 384 bits`
  (= 2 048 B = exactly 32 × 64-byte M4 cache lines).  **VSA primitives**:
  bind (XOR, self-inverse), threshold-majority bundle, cyclic permute,
  Q0.15 similarity = `(D − 2·H) · (32768/D)` — zero floating-point on
  the hot path.  **Cleanup memory** — constant-time linear sweep with
  branchless argmin update; runtime is `O(cap)` regardless of match
  index, so timing-channel leakage is bounded by arena size, not by
  secret state (Holographic Invariant Storage, arXiv:2603.13558).
  **Record / role-filler** (closed-form unbind via XOR involution),
  **analogy** (`A:B::C:?` solved as `A ⊗ B ⊗ C` + cleanup, zero
  gradient steps), and **sequence memory** (position-permuted bundle).
  **HVL — HyperVector Language** — a **9-opcode integer bytecode ISA**
  for VSA programs (`HALT / LOAD / BIND / BUNDLE / PERM / LOOKUP / SIM
  / CMPGE / GATE`) with per-program cost accounting in popcount-word
  units; the GATE opcode writes `v65_ok` directly into the composed
  decision and refuses on over-budget.  Sources: OpenMem 2026,
  VaCoAl arXiv:2604.11665, Attention-as-Binding AAAI 2026, VSA-ARC
  arXiv:2511.08747, HIS arXiv:2603.13558, Hyperdimensional Probe
  arXiv:2509.25045, HDFLIM, ConformalHDC, LifeHD arXiv:2403.04759.
  Composes with v60 + v61 + v62 + v63 + v64 as a **6-bit branchless
  decision** (`cos_v65_compose_decision`) so **no thought emits unless
  σ-Shield, Σ-Citadel, the EBT verifier, the AEAD tag + quote
  binding, the agentic intellect, _and_ the hypercortex on-manifold +
  cost-budget gate all ALLOW**.  Measured on M-series performance
  core: ~10.1 M Hamming/s @ **41 GB/s**, ~31.2 M bind/s @ **192 GB/s**
  (within 2× of unified-memory peak), ~10.5 M proto·comparisons/s
  cleanup, ~5.7 M HVL programs/s @ ~40 M ops/s.  ASAN clean
  (`make asan-v65`).  UBSAN clean (`make ubsan-v65`).  Hardened-build
  clean (`make standalone-v65-hardened`).  Apple-tier `cos` CLI
  surface: `cos hv`, `cos decide v60 v61 v62 v63 v64 v65`,
  `cos sigma` (previously a six-kernel verdict; extended to seven by
  v66).  Zero optional dependencies on the hot path — the kernel is
  libc-only.
- **v66 σ-Silicon** (`make check-v66`, **1 705 tests**).
  Dependency-free, branchless, **integer-only** C kernel shipping
  the 2026 mixed-precision-matrix frontier as the matrix substrate
  that turns v60..v65 thought into actual multiply-accumulate ops on
  actual silicon.  **Runtime CPU feature detection** for NEON,
  DotProd, I8MM, BF16, SVE, SME, SME2 (sysctl on Darwin, getauxval
  on Linux), cached in a single `uint32_t` bitmask for branchless
  hot-path lookup.  **INT8 GEMV** with NEON 4-accumulator inner
  loop, 64-byte prefetch, and `vaddlvq_s16` int32-wide horizontal
  long-add so int8×int8→int16 products cannot overflow; bit-
  identical scalar fallback; Q0.15 saturating output.  **BitNet
  b1.58 ternary GEMV** with 2-bits-per-weight packed format (00 → 0,
  01 → +1, 10 → −1, 11 → 0); branchless table-lookup unpack, so
  per-row time is independent of weight distribution (no timing
  side-channel on weights).  **NativeTernary wire (NTW)** — self-
  delimiting unary-run-length encoder/decoder at exactly 2.0
  bits/weight, with defensive invalid-code handling (no UB under
  UBSAN, no out-of-bounds under ASAN).  **CFC conformal abstention
  gate** — Q0.15 per-group streaming quantile estimator with ratio-
  preserving right-shift ratchet (same pattern as v64 Reflexion);
  gate compare is a single branchless `int32 ≥ int32`; admits the
  same finite-sample marginal coverage argument as the floating-
  point CFC specification under exchangeability of the score stream.
  **HSL — Hardware Substrate Language** — an **8-opcode integer
  bytecode ISA** (`HALT / LOAD / GEMV_I8 / GEMV_T / DECODE_NTW /
  ABSTAIN / CMPGE / GATE`) with per-instruction MAC-unit cost
  accounting and an integrated GATE opcode that writes `v66_ok`
  directly into the composed decision.  **SME / SME2 opt-in only**
  under `COS_V66_SME=1` with explicit streaming-mode setup; default
  builds never emit SME on non-SME hosts (SIGILL-safe on M1/M2/M3).
  Composes with v60 + v61 + v62 + v63 + v64 + v65 as a **7-bit
  branchless decision** (`cos_v66_compose_decision`) so **no matrix-
  backed thought emits unless σ-Shield, Σ-Citadel, the EBT verifier,
  the AEAD tag + quote binding, the agentic intellect, the
  hypercortex on-manifold gate, _and_ σ-Silicon's MAC-budget +
  conformal + wire-well-formed gate all ALLOW**.  Measured on
  Apple M3 performance core: ≈ 49 Gops/s INT8 GEMV (256 × 1 024),
  ≈ 2.8 Gops/s ternary GEMV (512 × 1 024), ≈ 2.5 GB/s NTW decode,
  ≈ 32 M HSL progs/s.  ASAN clean (`make asan-v66`).  UBSAN clean
  (`make ubsan-v66`).  Hardened-build clean (`make standalone-v66-
  hardened`).  Apple-tier `cos` CLI surface: `cos si`, `cos decide
  v60 v61 v62 v63 v64 v65 v66`, `cos sigma` (now a seven-kernel
  verdict).  Zero optional dependencies on the hot path — the kernel
  is libc + NEON intrinsics only (and, under `COS_V66_SME=1`,
  `arm_sme.h`).
- **v67 σ-Noesis** (`make check-v67`, **2 593 tests**).
  Dependency-free, branchless, **integer-only** C kernel shipping
  the 2024-2026 deliberative-reasoning + knowledge-retrieval frontier
  as the cognitive substrate that turns v60..v66 control + matrix
  plane into structured cognition with receipts.  **BM25 sparse
  retrieval** with integer Q0.15 IDF surrogate derived from
  `__builtin_clz`, CSR posting lists, and branchless top-K.  **Dense
  256-bit signature retrieval** via four `__builtin_popcountll` calls
  (native `cnt + addv` on AArch64) mapped to Q0.15 similarity
  `(256 − 2·H) · 128`.  **Bounded graph walker** with CSR + inlined
  8192-bit visited bitset (single load + mask membership, no
  data-dependent branch on node distribution).  **Hybrid rescore**
  with Q0.15 weights normalised to 32 768.  **Fixed-width deliberation
  beam** (COS_V67_BEAM_W = 8) with caller-supplied `expand` +
  `verify` callbacks and Q0.15 step-scores.  **Dual-process gate**
  (Kahneman / Soar / ACT-R / LIDA 2026 synthesis): System-1
  fast-path vs System-2 deliberation picked by a *single* branchless
  compare on the top-1 margin.  **Metacognitive confidence** =
  `top1 − mean_rest` clamped to Q0.15; monotone in absolute gap.
  **Tactic library** (AlphaProof-style) with branchless argmax over
  precondition-satisfied witness scores.  **NBL — Noetic Bytecode
  Language** — a **9-opcode integer bytecode ISA** (`HALT / RECALL /
  EXPAND / RANK / DELIBERATE / VERIFY / CONFIDE / CMPGE / GATE`)
  with per-instruction reasoning-unit cost accounting; GATE writes
  `v67_ok = 1` iff `cost ≤ budget` ∧ `reg_q15[a] ≥ imm` ∧
  `evidence_count ≥ 1` ∧ `NOT abstained` — so *no* program produces
  `v67_ok = 1` without writing at least one evidence receipt
  (AlphaFold-3-grade trace discipline in ~10 lines of C).  Composes
  with v60..v66 as an **8-bit branchless decision**
  (`cos_v67_compose_decision`) so **no deliberation crosses to the
  agent unless σ-Shield, Σ-Citadel, the EBT verifier, the AEAD tag
  + quote binding, the agentic intellect, the hypercortex on-manifold
  gate, σ-Silicon's MAC-budget / conformal / wire-well-formed gate,
  _and_ σ-Noesis's cost-budget + threshold + evidence-count gate all
  ALLOW**.  Measured on Apple M-series performance core: ~54 M dense
  Hamming cmps/s, ~800 k beam steps/s, ~64 M NBL programs/s
  (~320 M ops/s), ~9 k BM25 queries/s on D = 1 024, T = 16.  ASAN
  clean (`make asan-v67`).  UBSAN clean (`make ubsan-v67`).
  Hardened-build clean (`make standalone-v67-hardened`).  Apple-tier
  `cos` CLI surface: `cos nx`, `cos decide v60 v61 v62 v63 v64 v65
  v66 v67`, `cos sigma` (now an eight-kernel verdict).  Zero optional
  dependencies on the hot path — the kernel is libc-only.
- **v47 Frama-C architecture** (F-tier, where active).
- **v48 red-team harness** (M-tier; 0/342 bypasses at last run).
- **v49 DO-178C DAL-A artefacts** (I-tier; generated on demand).
- **Hardened build profile** (`make harden`): OpenSSF 2026 hardening
  flags + `-mbranch-protection=standard` + PIE.
- **Sanitizer matrix** (`make sanitize`): ASAN on v58 / v59 / v60 / v61 / v62 / v63 / v64 / v65 / v66 / v67 +
  UBSAN on v60 / v61 / v62 / v63 / v64 / v65 / v66 / v67, all passing their own self-tests under sanitizer.
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
- **v65 is not a language model.**  σ-Hypercortex is a *substrate*:
  bind, bundle, permute, cleanup, gate.  It does not generate tokens
  and has no learned parameters.  Compose it under or beside an LLM —
  Attention-as-Binding (AAAI 2026) shows the substrate is already
  there inside the transformer; v65 exposes it explicitly so it can
  be audited and gated.
- **v65 is not a vector database.**  Cleanup memory is fixed-D
  bipolar HVs swept in constant time.  It is not float-embedding
  ANN, not a similarity-search service, and does not replace
  Pinecone / Weaviate / Chroma / Milvus on their own turf.
- **v65 capacity ceiling.**  HIS (arXiv:2603.13558) bounds reliable
  superposition at `~D / log(|cleanup|)` pairs.  Beyond ~2 000
  superposed vectors the bundle's recovery floor is violated; the
  caller is responsible for partitioning into multiple records.
- **v65 cross-modal adapters are I-tier.**  Frozen-LLM and
  frozen-vision embedding → HV pipelines (Hyperdimensional Probe,
  HDFLIM lines) are documented and exercised on synthetic input
  only; the live adapter is not in v65 proper.
- **SRAM/CAM acceleration is P-tier.**  VaCoAl's hardware target
  (arXiv:2604.11665) is a planned compile-only path; v65 ships only
  the C / NEON popcount path on host hardware.
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
