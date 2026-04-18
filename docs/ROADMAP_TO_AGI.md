# Roadmap to AGI — what is missing

> **Scope.** This document is a merciless gap list. For each item: what it is,
> why it matters for an AGI-class local runtime, what Creation OS has today,
> what is still **missing**, and the 2026-frontier reference(s) that the
> implementation should track. Tier tags mirror `docs/WHAT_IS_REAL.md`:
> **M** = runtime-checked in the repo · **F** = formally proven · **P** =
> planned, not shipped · **A** = aspirational (the rule cursor file names it
> as a target but the hot path does not yet take it).
>
> A kernel that ships a `--self-test` but does not yet exist in the repo is
> still **P**, not **M**. Honest ledger. **1 = 1.**

---

## 1. Silicon — the physical substrate

### 1.1 Native SME kernel for per-layer repulsion — **A, not M**

- **What.** Apple M4 is the first Apple chip exposing ARM SME (Scalable Matrix
  Extension) as a direct ISA surface: `ZA` state, streaming mode, FMOPA
  outer-product. The project invariant (see `.cursor/rules/creation-os-m4-silicon.mdc`)
  names SME as the *primary* target for per-layer repulsion, with
  Accelerate / AMX as the interim.
- **What's in the repo.** An optional `SME=1` Makefile switch; repulsion
  today uses `cblas_sdot` + `cblas_saxpy` via Accelerate, or a NEON fallback.
  `creation_os/native_m4/creation_os_dispatcher.mm` explicitly gates the
  SME path behind `COS_ENABLE_SME` and avoids it by default to dodge SIGILL.
- **Missing.** A SIGILL-safe, streaming-mode-entered SME FMOPA kernel
  that runs the repulsion dot + saxpy **without calling Accelerate**, plus a
  runtime probe that only dispatches when `hwcap` / `sysctl` report SME
  present. Reference: ARM ARM v9.4-A SME/SME2, `arm_sme.h`.

### 1.2 Metal living-weights shader wired to an actual MLX logits path — **P**

- The shader `creation_os/native_m4/creation_os_living_weights.metal` exists,
  but `make full` only builds `creation_os_lw.metallib` if the toolchain is
  present; the Python side (`mlx_creation_os/creation_os.py`) prefers NEON.
- **Missing.** A zero-copy MLX → `MTLBuffer` path that uses unified memory
  directly and does not round-trip through `mlx.core.array.copy()` even in
  fallback. Benchmark target: ≥ 100 M vocab-popcount logit-bias updates/s
  on M4 Max while the NE runs the forward pass.

### 1.3 Neural Engine (ANE) inference via `_ANEClient` — **P**

- 2026 benchmarks (Maderix, "Inside the M4 Apple Neural Engine, Part 2")
  show CoreML adds 2–4× latency overhead over direct `_ANEClient`, which
  is the tax the repo currently pays.
- **Missing.** A `creation_os_ane.mm` binding that calls `_ANEClient`
  directly, reshapes GEMV as 1×1 conv (the trick that unlocks peak throughput
  per Maderix), and pins working set under 32 MB to avoid SRAM-to-DRAM
  spill. Benchmark target: ≥ 5 TFLOPS sustained on 2048×2048 FP16.

### 1.4 RISC-V RVV 1.0 + matrix-extension build — **P**

- RVV 1.0 is frozen; SiFive X280 Gen 2 / X300 ship VLEN 512–1024 cores
  today. Creation OS currently assumes ARM NEON + Apple Silicon as its
  host. A RISC-V Intelligence-X build target is zero cost to design for
  (same branchless primitives) and opens open-hardware attestation stories.
- **Missing.** `-march=rv64gcv_zvfh` build variant of every v60..v80
  kernel, with intrinsics in `include/cos_vec_rvv.h`. Reference:
  github.com/riscv/riscv-v-spec v1.0.

### 1.5 NVIDIA Confidential Computing (H200 / B200) bridge — **P**

- Corvex hit verified-prod B200 confidential inference in early 2026 with
  encrypted NVLink + ITA attestation. A local-first runtime still needs an
  optional "offload a proof-of-work prefill to a remote attested GPU"
  path for cases where the operator consents.
- **Missing.** `creation_os_cc_bridge.c` — minimal client that (a) requests
  an Intel TDX + NVIDIA CC attestation quote, (b) verifies it against
  Intel Trust Authority + NVIDIA NRAS, (c) opens a CXL-over-TLS channel
  for KV-cache handoff, (d) rejects any quote whose measurement does not
  match a pinned golden value.

### 1.6 Photonic-fabric plane — **P**

- LightIN (Nature *Light: Sci & App* March 2026) = 1.92 TOPS with 260 ps
  NN-inference latency at 1.875 pJ/MAC. Lightmatter Passage (March 2026)
  = 1.6 Tbps/fiber DWDM CPO. Celestial AI Photonic Fabric ships PFLink
  @ 14.4 Tbps chiplet IP.
- **Today.** `v70 σ-Hyperscale` names *photonic WDM* as a sub-primitive
  but the code is a pure integer model — it compiles and runs, but does
  not talk to any actual photonic chiplet.
- **Missing.** A `cos_v81_photonic.c` stub that exposes a WDM-channel
  abstraction (`λ_1..λ_16`), and a lattice-consistent route-selection
  primitive branchless enough to map to LightIN's 40-cell array when a
  PCIe-attached card lands.

### 1.7 Neuromorphic (Loihi 3 / NorthPole / Akida) plane — **P**

- Loihi 3 (Intel 4nm, Jan 2026) = 8M neurons, 64B synapses, 1.2 W peak
  vs. 300+ W GPU equivalent. NorthPole reports 25× better eff/W vs. H100
  on imagenet. Akida AKD1000 hits 45 µJ/inference at < 10 mW.
- **Today.** `v70` has a *spike / Loihi* primitive but it is purely
  integer-simulated. There is no actual spike-encoded path for v62..v80.
- **Missing.** (i) A tick-based graded-spike transport in `include/cos_spike.h`,
  (ii) an `SNN` target for v62 that emits spike trains instead of
  integer activations, (iii) a Lava / Hala-Point-compatible export so the
  reasoning fabric can be *replayed* on real neuromorphic silicon.

### 1.8 Analog / PIM (RRAM, 6T-2R SRAM, Mythic) dot-product engine — **P**

- NVM-in-Cache (arXiv 2510.15904) = 452.34 TOPS/W. NL-DPE (arXiv
  2511.13950) = 28× energy eff, 249× speedup on CNN + LLM vs. GPU.
  PRIMAL (arXiv 2601.13628, Jan 2026) = 1.5× throughput, 25× eff vs. H100
  on LoRA inference. PIM-LLM ≈ 80× tokens/s on 1-bit LLMs.
- **Missing.** An `analog_dot` primitive whose semantic twin is
  `cblas_sdot` but whose *implementation contract* declares "this may
  run on an analog array with Q4.4 output precision and conformal error
  bars." v66 σ-Silicon is the right home — it already has ternary and
  conformal gates, it just doesn't have an analog-dispatch slot.

### 1.9 CXL 3.0 shared KV cache (disaggregated inference) — **P**

- Penguin Solutions 11 TB CXL KV-cache server shipped 2025. TraCT
  (arXiv 2512.18194) = 9.8× TTFT reduction, 6.2× P99 latency reduction
  over RDMA. Commercial deployments report 21.9× throughput + 60× lower
  energy / token.
- **Today.** `v80 σ-Cortex` has `cos_v80_kv_commit` — paged KV cache as
  a *local* ring buffer with integer page tags.
- **Missing.** A `cos_v81_cxl_kv.c` where KV pages are refs into a CXL
  shared region, the GPU can `LD`/`ST` them directly, and migration is
  metadata-only. Works on commodity today over CXL 2.0 pooling; CXL 3.0
  shared-memory lets multiple agents coordinate without an RDMA hop.

### 1.10 CHERI / ARM Morello purecap build — **P**

- RFC-15 proposes CHERI/Morello support in seL4. CHERI-seL4 + CHERI-Microkit
  are released by the CHERI Alliance. Morello hardware + FVP available via
  DSbD Technology Access Programme.
- **Today.** `sel4/sigma_shield.camkes` is a one-file CAmkES scaffold.
- **Missing.** A purecap build target (`make purecap`) that compiles
  every branchless kernel under CHERI semantics, proves no unchecked
  pointer arithmetic, and runs the `--self-test` suite inside a
  capability-bounded sandbox on Morello FVP. Capability-based security
  that is *actually* hardware-enforced, not just softly emulated.

---

## 2. Kernel / OS / sandboxing

### 2.1 Real seL4 compartmentalization of the twenty kernels — **P**

- `sel4/sigma_shield.camkes` names one component. A plausible MCS-seL4
  deployment would put v60..v80 **each in its own CAmkES component** with
  notification-based channels, so a compromise in e.g. v67 retrieval
  cannot reach v63 cipher keys. Cohesix (github/lukeb-aidev) is the
  reference MLOps control plane doing exactly this on seL4.
- **Missing.** Twenty `.camkes` assemblies wired through typed IDL, plus
  a `make sel4-compose` that boots a verified image and reports a PCR
  quote over the whole stack. This is the real implementation of "one
  zero anywhere ⇒ silence."

### 2.2 Wasmtime component-model tool-execution sandbox — **P**

- WASI-P3 / Component Model is production (Microsoft Wassette / Asterai,
  Zylos Research "WASM Sandboxing for AI Agent Runtime Isolation"
  2026-03-12). Zero ambient authority, µs cold starts, per-component
  linear memory, Canonical-ABI isolation.
- **Today.** `wasm/example_tool.c` exists — that's the whole story.
- **Missing.** A Wassette-compatible `cos tool <component.wasm>` path
  where v64 σ-Intellect's tool-authz gate is the *only* thing that can
  grant a capability (file, network, clock, random). Deny-by-default.
  Rejection emits a v72 chain-bound receipt so every refusal is auditable.

### 2.3 eBPF runtime policy engine for the dispatcher — **P**

- `ebpf/sigma_shield.bpf.c` = one filter. A full eBPF LSM + tracefs
  harness would give in-kernel, branch-less enforcement of "no process
  spawned under the dispatcher may `execve` outside the signed cohort,"
  evaluated before the scheduler touches it.
- **Missing.** A bpftool-loadable map of signed hashes, an LSM hook at
  `bprm_check_security`, and a userspace `cos shield on` that installs
  them. On macOS the analogue is **Endpoint Security** + System
  Extensions; both should ship.

### 2.4 pledge/unveil + Capsicum equivalents at process granularity — **P**

- Neither macOS nor mainline Linux has `pledge`/`unveil` as first-class
  syscalls; OpenBSD has both, FreeBSD has Capsicum. A v60-compatible
  emulation on Linux uses seccomp-BPF + Landlock; on macOS it uses
  sandbox-exec + Endpoint Security. Creation OS ships neither today.
- **Missing.** `scripts/pledge-wrap` that runs any subprocess with the
  narrowest syscall set it demonstrably needs, derived from a static
  trace of a successful `--self-test`. Anyone opening `cos think` thus
  runs a binary that literally *cannot* open a socket.

### 2.5 Endpoint Security / LSM hook for macOS — **P**

- Apple's Endpoint Security framework is the only supported path for
  on-host instrumentation after System Integrity Protection. Creation OS
  runs on M4 but does not register any ES client.
- **Missing.** `native_m4/cos_es_client.mm` that observes `execve`,
  `mmap` W^X, and `proc_check_signature`, and refuses to start `cos`
  under an unsigned / unknown parent.

---

## 3. Memory & data path

### 3.1 `mmap`-everywhere audit — **A, partially M**

- The Cursor rule `.cursorrules` §1 states: *"All model files are opened
  with `mmap`. No copying. Disk is memory."* The existing kernels honour
  this for self-test data; the Python `mlx_creation_os` glue path does
  not universally — when `CREATION_OS_METALLIB` is not set and the Metal
  path falls back to host copy, it `memcpy`s.
- **Missing.** A lint + CI target `make no-fread` that greps every C
  file for `fread(` / `malloc(` on the hot path and rejects merges that
  introduce either. Current repo has no such gate.

### 3.2 `aligned_alloc` audit — **A, partially M**

- Rule §2 mandates 64-byte aligned allocs. Nothing audits this; a drive-by
  `malloc(3 * sizeof(float))` compiles silently. v80's `cortex.c` uses
  stack structs only, which is safe; v70/v71 do `calloc`.
- **Missing.** A `-DCOS_REQUIRE_ALIGNED` compile flag that `#define`s
  `malloc`/`calloc` to `(void)0`, forcing every dynamic alloc through a
  single `cos_aligned` helper.

### 3.3 GPUDirect Storage / io_uring fast-path — **P**

- LLM weights don't need to be paged through the CPU. On Linux, io_uring
  + GPUDirect Storage give the GPU direct NVMe DMA. On macOS, `unified
  memory` obviates GDS but `posix_fadvise`/`madvise(MADV_RANDOM)` on
  large mmap'd weights is still not in the repo.
- **Missing.** A `cos_mmap_weights()` helper that probes for unified
  memory (Darwin), GDS (Linux), and falls back to plain `mmap` +
  `madvise(WILLNEED | SEQUENTIAL)` elsewhere.

### 3.4 Persistent KV cache across restarts — **P**

- v80's `kv_commit` is in-memory. A real local runtime evicts cold pages
  to an NVMe-backed, signed-and-sealed file so the first answer after a
  reboot is not a cold-cache stall.
- **Missing.** `cos_v81_kv_persist.c` — page-granular NVMe write-behind
  with BLAKE2b integrity tags and v63-σ-Cipher-wrapped AEAD framing. On
  restart, pages rematerialise lazily on first access.

### 3.5 DNA / glass archival tier for hypervector memory — **P**

- Genscript × Mimulus (2026) industrializing DNA storage; Microsoft
  Silica = 4.8 TB in 120 mm glass, 10 000-year lifetime. These are real
  2030-horizon archival tiers.
- **Missing.** `cos_v81_cold_tier.c` — a logical archival interface
  where v68 σ-Mnemos can emit a HD-bundle of crystallised skills for
  write-once-forever archival, plus a synthetic DNA codec (Shannon-capacity
  Reed-Solomon + homopolymer balance) that produces the oligonucleotide
  stream the shop-floor synthesiser would actually print.

---

## 4. Cryptography

### 4.1 Post-quantum cryptography (ML-KEM / ML-DSA / SLH-DSA / FN-DSA) — **P**

- NIST FIPS 203 (ML-KEM), 204 (ML-DSA), 205 (SLH-DSA) finalised Aug
  2024. FIPS 206 (FN-DSA / FALCON) expected 2026. CNSA 2.0 mandates for
  US NSS from Jan 2027. Chrome 131+ and Firefox 135+ run ML-KEM-768 in
  hybrid TLS by default today.
- **Today.** v63 σ-Cipher = BLAKE2b + HKDF + ChaCha20-Poly1305 + X25519.
  No PQC. Every sealed envelope is harvestable and decryptable once CRQC
  lands.
- **Missing.** (i) ML-KEM-768 KEM primitive in C, (ii) ML-DSA-65
  signature primitive, (iii) SLH-DSA-SHA2-128s for algorithmic diversity,
  (iv) hybrid X25519 + ML-KEM-768 key exchange (so a compromised
  lattice primitive does not downgrade security), (v) `--pqc-only` mode
  for air-gapped deployments. Reference implementations: pq-crystals/kyber,
  pq-crystals/dilithium; constant-time C pre-pulled from NIST submissions.

### 4.2 Verifiable inference (zkML / NANOZK / zkAgent) — **P**

- NANOZK (arXiv 2603.18046, 2026) = layerwise ZK proofs, 5.5 KB total
  per-layer proof, 24 ms verification, 70× smaller + 5.7× faster than
  EZKL. zkAgent (eprint/2026/199) = one-shot full-inference proof, 1.05
  s/token proving, 0.45 s verification for 512-token GPT-2; 294× faster
  than zkGPT, 9 690× faster verification.
- **Today.** v72 σ-Chain has a ZK-proof-receipt primitive but the
  receipt is a *hash* commitment, not an arithmetisation of the actual
  LLM forward pass.
- **Missing.** An `cos_v82_zkinfer.c` that runs v80's TTC program inside
  a Halo2 / Plonky3 circuit, emits a proof alongside every emission, and
  gates the final AND on proof validity. For local-only flows the prover
  runs lazily; for cloud-peered flows the proof accompanies the reply.

### 4.3 Fully homomorphic encryption (TFHE / CKKS) inference path — **P**

- Zama TFHE-rs v1.4 (Oct 2025) = up to 4× GPU speedup. TIGER (arXiv
  2604.04783, Apr 2026) = 7.17×/16.68×/17.05× GPU speedups over CPU for
  GELU/Softmax/LayerNorm respectively.
- **Missing.** A *very narrow* FHE path for the last-token decode only —
  the expensive matmul stays plaintext, but top-k + sampling runs on
  TFHE-encrypted logits when the user toggles *paranoid mode*. This is
  the only way to give a real "cloud provider cannot see your prompt"
  guarantee if any remote peer is ever involved.

### 4.4 Hardware security module / TPM-2.0 rooted chain — **P**

- ATTESTATION.json ships today as a *deterministic* quote. A real
  attestation chain goes: TPM EK cert → AIK → PCR quote of every
  `creation_os_vN` binary's measurement → signed `ATTESTATION.json`.
- **Missing.** (i) `cos attest hw` that reads `/dev/tpmrm0` (Linux) or
  calls Secure Enclave via `SecKeyCreateRandomKey` with `.ecsecPrimeRandom`
  (macOS, for attestation-like use), (ii) a PCR-extend log of every
  build step under `docs/ATTEST_LOG`, (iii) a verifier that rejects any
  `doctor` run whose measurement differs from `LICENSE.sha256`.

### 4.5 Sigstore cosign / SLSA-3 / in-toto — **P**

- The repo has `ATTESTATION.json` + `PROVENANCE.json` + `SBOM.json` as
  *present* artifacts (see `cos status`), but no cosign signature on
  tagged releases, no in-toto layout, and no Rekor transparency entry.
- **Missing.** GitHub Actions workflow that (i) builds with `nix` /
  `bazel` for bit-exact reproducibility, (ii) signs with cosign
  keyless + Fulcio, (iii) submits to Rekor, (iv) publishes a SLSA-3
  provenance statement. `make slsa` and `make sbom` exist; they produce
  valid JSON but not yet transparency-logged.

### 4.6 Code-signing + notarisation for the macOS binary — **P**

- `cos` is currently un-notarised. Gatekeeper will block it on Catalina+
  unless the user runs `xattr -d com.apple.quarantine`. That's the
  *opposite* of "naurettavan helppo" for a first-time user.
- **Missing.** Developer ID signing + notarytool submission + stapled
  ticket on every tag. `scripts/install.sh` already detects platform; a
  macOS-specific path can `pkgutil --expand` a notarised `.pkg` instead.

---

## 5. Reasoning / models — the actual AGI core

### 5.0 Real LLM forward pass in-process — **M (new in v101)**

- **What.** Prior to v101 every Creation OS kernel ran on surrogates:
  synthetic logits, hypervectors, branchless integer decision tables, or
  n-gram corpora. No kernel produced probabilities over a real vocabulary
  from real weights.
- **What's in the repo.** `src/v101/` wires microsoft/BitNet
  (bitnet.cpp — a llama.cpp fork with i2_s / TL1 / TL2 kernels for the
  BitNet b1.58 ternary format) as a shared-lib backend and exposes three
  entry points through `creation_os_v101`: σ-math self-test, greedy
  generate with σ-gated early-stop, and teacher-forced loglikelihood with
  per-token σ aggregation. Measured on this host: ~44 tok/s eval on
  Apple M4 Metal, deterministic JSON output, 19/19 σ-math assertions
  green in both stub and real builds. See
  [docs/v101/THE_BITNET_BRIDGE.md](v101/THE_BITNET_BRIDGE.md).
- **Still missing (towards §5.2 / §5.6 below).** Persistent-process
  inference (so `creation_os_v101 --ll` does not reload weights per
  call), long-horizon KV-cache reuse across σ-gated agentic loops, and
  streaming-token emission from the same binary. These are bounded
  engineering follow-ups, not research gaps.

### 5.0b End-to-end benchmark exposure of σ-stack — **M (landed on 2026-04-17)**

- **What.** The v29 / v34 / v40 / v78 σ-channels were all self-tested
  but never confronted with an external LLM eval suite. Any claim that
  σ correlates with correctness was therefore unfalsifiable.
- **What's in the repo.** `benchmarks/v102/` registers a `creation_os`
  backend for EleutherAI lm-evaluation-harness and drives the real
  BitNet b1.58 2B-4T forward pass through
  `creation_os_v101 --stdin`.  First full run landed 2026-04-17 on
  Apple M3 / Metal / bitnet.cpp i2_s: **`arc_easy` acc = 0.762
  (±0.019)**, **`arc_challenge` acc_norm = 0.488 (±0.022)**,
  **`truthfulqa_mc2` acc = 0.476 (±0.019)** at n=500 per task, all
  within 1σ of Microsoft's published BitNet numbers (arXiv:2504.12285,
  Table 2).  σ was observed on every forward pass (τ = 0.0, no
  abstain); as designed, accuracy is unchanged vs. the raw argmax
  baseline — σ is a neutral diagnostic at τ = 0.0, not an accuracy
  lift.  Ledger: [docs/v102/RESULTS.md](v102/RESULTS.md).
- **Open (P follow-ups).**  (i) selective-prediction sweep over
  τ ∈ {0.3, 0.5, 0.7} with abstain% column, (ii) MMLU / GSM8K /
  HellaSwag / Winogrande tables, (iii) matched llama-server baseline
  on this host.  None of these can retroactively shift the numbers
  already measured; they only extend the table.

### 5.1 Continual learning with sleep consolidation — **M-partial, P-deep**

- v68 σ-Mnemos has ACT-R decay + surprise gate + sleep-consolidation
  primitives — as *functions* with self-tests. What is missing is the
  **daemon** that actually runs consolidation overnight, rewrites long-
  term skills into v65 σ-Hypercortex's HV bundle memory, and snapshots
  the state. Frontier refs: arXiv 2602.07755 (ALMA, meta-learned
  memory), arXiv 2603.07670 (agent memory survey).
- **Missing.** A `cos_sleep_daemon` launchd / systemd unit that every
  N hours (i) flushes episodic to semantic, (ii) prunes low-surprise
  rows, (iii) runs a mini self-play game on v69 σ-Constellation to
  re-rank tactics, (iv) emits a signed receipt, (v) pulls the latest
  mathlib / kaggle / arxiv delta into a sandboxed extraction queue.

### 5.2 World-model training loop that feeds v79 back into v80 — **P**

- v79 σ-Simulacrum simulates worlds. v80 σ-Cortex reasons inside HV.
  But the **loop** — reasoning outputs perturb v79's state, v79 rolls
  forward, the actual outcome updates v62's Energy-Based verifier — is
  **not wired.** ARC-AGI-3 scores *frontier* systems below 1 % on
  precisely this kind of "explore a novel env and build a dynamics
  model" task.
- **Missing.** `src/v81/agentic_loop.c` — the causal closure: v80 emits
  a plan, v79 rolls it, the delta is fed to v62's EBT and v78's FEP
  surprise counter, v68 stores episode, v64 updates skill priors. Until
  this loop is closed, Creation OS is twenty verifiers *without* a
  learner.

### 5.3 Test-time training / online RL — **P**

- arXiv 2603.13372 emphasises test-time adaptation as the 2026
  bottleneck. ARC Prize 2025 showed 390× cost drop y/y came largely from
  reduced test-time parallelism. Creation OS has no TTT path.
- **Missing.** A **read-only** TTT slot in v70 σ-Hyperscale that lets
  v65 σ-Hypercortex rebind hypervectors at inference time under a KL
  budget proven by v62. No weight changes to the frozen LLM, only HV
  rebinding — keeps the hot path branchless and the weights tamper-
  evident.

### 5.4 Real multimodal input path — vision, voice, sensor — **P**

- v73 σ-Omnimodal names image / audio / video / 3D / workflow as sub-
  primitives. They are all integer stubs: a 16-byte tensor with a codec
  tag, not an actual decoder. No mic → opus → 16 kHz PCM → v62 pipeline
  exists. No camera → Vision framework → embedding path exists.
- **Missing.** (i) `creation_os_audio.mm` with AVAudioEngine +
  coreaudio input, (ii) `creation_os_vision.mm` with the iOS 26
  Foundation-Models on-device vision embedding, (iii) a
  back-pressure-safe ring buffer that keeps v62's fabric fed without
  blocking the daemon.

### 5.5 Voice output — neural TTS on-device — **P**

- iOS 26 ships a 3 B param on-device Foundation model + a Personal
  Voice TTS API. macOS 15+ has Speech Recognition / Synthesis
  Frameworks.
- **Missing.** `cos say <text>` that uses AVSpeechSynthesizer (baseline)
  and an optional neural-TTS plug (Orpheus / XTTS) for richer voice.
  Emits v63-σ-Cipher-sealed audio so a recorded answer carries its own
  attestation.

### 5.6 Real-time streaming generation — **P**

- All current v80 TTC demos run as full-answer batches. A chat-class UX
  needs streamed decoding with speculative verify at every ≈ 32 tokens,
  with the composed decision gate running **per chunk** — if a mid-
  stream emission fails the AND, the stream halts cleanly and emits a
  v72 chain receipt.
- **Missing.** A chunked-stream interface on top of v80's TTC VM + v55's
  speculative-decode kernel + v72's ledger, plus a terminal UI in `cos`
  that renders them with sub-50 ms first-token latency.

### 5.7 Multi-agent Byzantine quorum across machines — **P**

- v69 σ-Constellation models tree-spec + Byzantine vote but runs all
  "agents" in-process. A real fault-tolerant AGI runs three disjoint
  model ensembles on three machines with a DAG-BFT quorum and emits the
  winning reply only on (f+1) agreement.
- **Missing.** gRPC / QUIC + v63 E2E envelope, plus a daemon that
  spawns three child `cos` instances on different machines (or LAN
  peers) and runs v72's DAG-BFT on their replies.

### 5.8 Self-play + Byzantine-safe skill library — **P**

- v64 σ-Intellect has MCTS-σ + skill lib + tool authz, but the *library*
  is static — it never grows. Self-play (AlphaZero-style) against v79's
  simulacrum is the obvious growth loop.
- **Missing.** `cos train skills` that runs N self-play rounds in v79,
  MCTS-searches reply spaces, and only promotes skills whose
  (v78 proof receipt, v77 reversibility, v72 chain receipt) all pass
  across an Elo-UCB window.

### 5.9 Long-horizon autonomy (≥ 8 hours) — **P**

- Vastkind AI-predictions-2026: frontier agent horizons are 2–2.67 hrs.
  An AGI-class runtime needs reliable 24-hour autonomy — goal
  decomposition, error recovery, dead-end rollback.
- **Missing.** A `cos run <mission.yml>` daemon that reads a structured
  goal tree, executes on a time budget, checkpoints every 15 min into
  v72 chain, and self-rolls-back on repeated surprise spikes from v78.

---

## 6. Capabilities & agent safety

### 6.1 Capability-based OS-level authz (Bell-LaPadula + Biba) — **M-partial**

- v61 Σ-Citadel implements BLP + Biba *mathematically* (the lattice
  passes 61 self-test rows). What is **not** done: wire it to the actual
  filesystem via MAC labels + xattr so a leaked capability bit from v64
  cannot be replayed against a file it was not issued for.
- **Missing.** Linux: a small LSM + xattr namespace (`security.cos.lbl`).
  macOS: ACLs via `acl_set_file` with custom entry types.

### 6.2 Tool sandboxing with network egress rules — **P**

- v64's tool-authz is policy + MCTS + Reflexion — it prevents *wrong*
  tool use, not unauthorised *network egress*. A real sandbox intercepts
  every `connect(2)` and requires a capability token.
- **Missing.** (i) Linux: nftables + cgroupv2 net_cls; (ii) macOS:
  Network Extension content filter; (iii) Wasmtime: WASI `wasi-sockets`
  with deny-default.

### 6.3 Differential-privacy guard on outputs — **P**

- Any memory read from v68 that touches user data should be DP-noised
  before reaching the emission layer. Creation OS has no DP primitive.
- **Missing.** `cos_dp_gauss(ε, δ)` helper + a guard in v64 that
  enforces `(ε, δ)` < policy budget per session. Reference:
  opacus.ai / Google dp-dp-tools.

### 6.4 Federated learning with secure aggregation — **P**

- If multiple Creation OS instances share skill updates, the gradient
  traffic is itself a privacy risk. Secure aggregation (Bonawitz et al.
  2017) is the cheap fix. Creation OS has no federated path.
- **Missing.** `cos federate <peer-list>` that runs one round of
  secure-aggregation SGD on v64 skill priors, committed to v72 chain,
  wrapped in v63 E2E.

### 6.5 Constitutional / rules-of-engagement gate — **M-thin**

- v56 is named "Constitutional" in the σ lab table. The full spec —
  Anthropic-style revised-principles + red-team → accept/deny gate with
  a **provable** appeal channel — is thin.
- **Missing.** (i) A canonical `COS_CONSTITUTION.md` that lists the
  constraints the dispatcher refuses to override, (ii) a v78-backed
  proof that every v64 MCTS leaf passes the constitution under
  bisimulation, (iii) a user-facing "why was this refused" explainer
  that cites the exact constitution bit that flipped.

### 6.6 Provable alignment via Lean 4 specs — **P**

- VeriGuard (arXiv 2510.05156) ships dual-stage formal safety for LLM
  agents. Goedel-Code-Prover + Leanabell-Prover-V2 prove Lean 4
  statements on the fly. Creation OS has no Lean corpus.
- **Missing.** `spec/*.lean` — formal statements of "v60 never leaks
  capability bit B to a subject without (clearance ≥ B)," machine-
  checked by `lake build`, pinned into `ATTESTATION.json`.

### 6.7 Red-team harness that runs continuously — **P**

- `make red-team` exists as a target name; the matrix of adversarial
  prompts, prompt-injections, tool-authz bypass tries, jailbreak
  attempts, and CAPEC-class exploits is **not comprehensive**.
- **Missing.** A `docs/red-team-corpus/` that tracks the full 2024–2026
  jailbreak + prompt-injection literature (Liu, Greshake, Zou, Wei,
  Perez) and runs every release against every attack.

---

## 7. Observability & ops

### 7.1 Structured tracing with σ + receipts in one stream — **P**

- Today a developer eyeballs `cos sigma` output. There is no OpenTelemetry
  emitter, no eBPF uprobe on each kernel's entry/exit, no stream of JSON
  events per decision.
- **Missing.** `cos trace` that opens a UDS socket and streams `{kernel,
  bit, σ, t, receipt_hash}` per emission, loadable by Jaeger / Grafana
  Tempo / honeycomb.

### 7.2 SLO-aware adaptive compute — **P**

- v59 σ-Budget exists as a slot but the dispatcher does not *consult* it
  at runtime. A real SLO-aware runtime budgets test-time compute per
  answer (o1-style) under a latency cap the user can set.
- **Missing.** A global `COS_BUDGET_MS` env var that v64 + v80 both
  respect, and a principled rollover from TTC to a faster path when the
  budget drains below threshold.

### 7.3 First-class GUI — macOS menu-bar + iOS widget — **P**

- `cos` is a terminal CLI. For most humans that *is* the blocker.
- **Missing.** (i) A SwiftUI menu-bar app wrapping `cos` as a pane —
  paste a question, see a streamed answer with a green "all twenty
  passed" badge under it. (ii) An iOS WidgetKit widget that shows the
  last three answers + σ-bar. (iii) An Android share-target that routes
  any selection through `cos think`.

### 7.4 Chat product — not a CLI demo — **P**

- The current `cos think` is a single-shot call. A real chat client
  needs history, retractions, @-mentions that resolve into v67 Noesis
  retrieval, and a multi-turn v68 Mnemos memory.
- **Missing.** A TUI chat client in `cli/cos_chat.c` that is the default
  for non-technical users. `cos` starts the chat; everything else is
  slash commands.

### 7.5 Browser-extension surface — **P**

- `bindings/` has iOS + Android façades. A Chrome/Firefox extension
  that routes every page summary through `cos` locally is the highest-
  ROI user-facing win. No such extension exists.
- **Missing.** `bindings/webext/` MV3 manifest + a native-host stdio
  bridge to the local `cos` daemon.

### 7.6 "Naurettavan helppo" install for non-technical users — **M-partial**

- `scripts/install.sh` ships now (one-liner curl|bash) and `cos welcome` /
  `cos demo` are in. **What is still missing:** a signed `.pkg` / `.dmg`
  on macOS so Gatekeeper doesn't quarantine it, a signed `.msi` on
  Windows (we don't support Windows yet at all), an `apt` / `brew`
  tap, and a *website landing page* with a copy-button. The repo has
  no `website/` directory.

### 7.7 Telemetry — zero, by policy; evidence, auditable — **P**

- Creation OS sends zero telemetry. Good. But: there is no
  user-facing way to audit this claim. A strace / `dtrace` transcript
  of a clean `cos sigma` run should ship as a *receipt* of the no-
  network claim.
- **Missing.** `scripts/prove-no-network.sh` that runs `cos sigma`
  inside `unshare -n` (Linux) / `sandbox-exec -f cos.sb` (macOS), diffs
  network syscalls, and emits a signed "Creation OS made zero network
  calls" receipt.

---

## 8. Build, release, ecosystem

### 8.1 Reproducible builds — Nix + Bazel — **P**

- `flake.nix` + `nix/v61.nix` exist; no fully-pinned world. Bazel is
  absent. Without bit-exact reproducibility the SLSA-3 and PQC-era
  attestations are weakened.
- **Missing.** A complete `flake.nix` that pins rustc / clang / nasm /
  every tool version, plus CI that diffs two independent builds byte-
  for-byte. `scripts/reprotest.sh`.

### 8.2 Distroless / minimal container runtime — **M-partial**

- `Dockerfile.distroless` exists. It needs (a) gVisor or Firecracker
  wrapping, (b) a default no-network seccomp profile, (c) cgroups-v2
  CPU / memory limits pinned to the M4 P-core / E-core split.
- **Missing.** `deploy/firecracker-vm.json` + a `make firecracker`
  target.

### 8.3 Package-manager presence (Homebrew / apt / Cargo crate / pip wheel) — **P**

- Zero package presence today. A first-time user has to `git clone`.
- **Missing.** (i) A Homebrew tap: `brew install spektre-labs/creation-os`.
  (ii) A `.deb` on GitHub releases. (iii) A Python wheel wrapping the
  `mlx_creation_os` glue with `pip install creation-os`. (iv) A Rust
  crate binding over the C ABI.

### 8.4 Plugin architecture / marketplace — **P**

- Cursor / Claude Desktop / Anthropic ship plugin marketplaces. Creation
  OS ships `creation_os_mcp` but no plugin protocol for v60-gated tool
  additions.
- **Missing.** `plugins/` directory + a `plugin.json` schema v1, a
  `cos plugin add <repo>` command that fetches, v60-σ-Shield sandboxes,
  and v63-wraps the plugin.

### 8.5 Localisation — UI strings, not just English — **P**

- `cos welcome` is English-only. The user base is global.
- **Missing.** A `locale/` directory + a POSIX-gettext-style catalogue,
  Finnish + Japanese + Mandarin + Spanish + Arabic + Swahili baseline.

---

## 9. Formal & theoretical gaps

### 9.1 TLA+ spec of the composed decision — **P**

- The AND-of-twenty composition is informally stated in README. TLA+
  would prove exactly: "∀ inputs, final emission ⇒ ∧_i v_i_ok." This is
  a 200-line spec; its absence is a known gap.
- **Missing.** `formal/composed_decision.tla` + `make tla-check` that
  runs TLC against it.

### 9.2 Isabelle / Coq proof of branchlessness — **P**

- "Every kernel's hot path has zero branches" is a claim. A small
  Isabelle HOL model of the assembler output would make it a theorem.
- **Missing.** `formal/branchless.thy` + a compilation step that
  extracts an objdump trace and checks `cbz/cbnz/b.cond` count = 0 on
  marked functions.

### 9.3 Chaitin-Ω / Kolmogorov-complexity budget enforcement — **M-partial**

- v78 σ-Gödel-Attestor names a Chaitin-Ω budget. The *enforcement* is
  that the receipt arithmetic rejects over-budget programs. What is
  missing is a v80-TTC-side compiler pass that *rejects bytecodes whose
  static upper bound exceeds the budget*, so the over-budget program
  never even runs.
- **Missing.** A bytecode static analyser in `src/v80/ttc_static.c`.

### 9.4 Bisimulation proofs that v77 reverse of v80 recovers state — **P**

- v77 σ-Reversible proves `forward ∘ reverse ≡ identity` on its own
  register file. It does **not** prove bisimulation between v80 cortex
  steps and their reverse-engineered twin.
- **Missing.** A coupled simulation where every v80 TTC op emits both
  its v77 forward trace and a recoverable reverse transcript, with
  bisim checked every 1 000 ops.

### 9.5 Game-theoretic safety of the Byzantine quorum — **P**

- v69 σ-Constellation uses Byzantine vote. The *game* between a
  malicious peer and an honest one is not modelled.
- **Missing.** A mechanism-design proof (Nash / dominant-strategy-
  incentive-compatible) that the voting rule rewards honest replies.

---

## 10. Long-tail & moonshots

### 10.1 Biological / wetware co-processing stub — **P**

- Cortical Labs DishBrain, Koniku, FinalSpark rent neurons-on-a-chip
  today. Not a near-term dispatch target, but an *interface* (latency:
  seconds, throughput: bits) should exist as a placeholder so the day
  they commoditise, Creation OS has the hole to plug in.
- **Missing.** An `include/cos_wetware.h` abstract with ping +
  time-bounded single-bit query.

### 10.2 Quantum co-processor plane (IBM Heron, AtomComputing, PsiQuantum) — **P**

- v79 σ-Simulacrum simulates stabilizer (Aaronson-Gottesman) circuits
  today. A real quantum-cloud dispatch (Qiskit Runtime, AWS Braket,
  Azure Quantum) is **not** there.
- **Missing.** `cos_v82_qdispatch.c` with (i) a transpilation step from
  v79's stabilizer IR to OpenQASM 3.0, (ii) an HTTPS POST to an
  attested Braket endpoint, (iii) a verifier that compares returned
  expectation values against v79's classical simulation inside error
  bars.

### 10.3 Mechanistic interpretability — always-on probe — **P**

- Anthropic (2024–2026) mechanistic-interpretability findings (residual-
  stream SAEs, circuit discovery) need a runtime hook so the *local*
  model's activations can be probed on the fly.
- **Missing.** An SAE-feature-capture hook in v62 that exports the top-
  k activating features per token into a v67-retrievable index, and a
  `cos why` command that cites them.

### 10.4 Solar / off-grid deployment — **P**

- "Runs on a phone for a day on solar" is a real 2030 goal and very on-
  brand for a local-first runtime. The efficiency primitives (NEON,
  PIM, neuromorphic) all exist; the *integrated* low-power mode does
  not.
- **Missing.** `cos low-power` that pins the dispatcher to efficiency
  cores, downclocks the ANE via powermetrics, disables non-critical
  kernels, and emits a Joules-per-answer receipt.

### 10.5 Direct brain-computer interface placeholders — **P**

- Neuralink, Synchron, Precision Neuroscience ship human BCIs today.
  The abstract protocol — `cos_bci.h` = `read_spikes() / write_stim()`
  — is free to design now.
- **Missing.** One header file. Not urgent; philosophical placeholder.

### 10.6 Programming-language research (Rust-in-kernel-space, Zig, Hylo, Lean4 native) — **P**

- The entire repo is C11. Rust-for-Linux and Zig on seL4 show the
  ergonomic / safety wins are real. Every future kernel could be written
  in Hylo / Lean4-native / Rust without changing the C ABI.
- **Missing.** A parallel `src-rust/v80/` proof-of-concept crate that
  reimplements v80 σ-Cortex in safe Rust + `no_std` and passes exactly
  the same 6 935 348 self-test rows, byte-compatible receipts.

---

## Priority shortlist — the next ten commits

If only ten PRs were allowed before the next release, rank:

0. ~~**Run v102 σ-Eval-Harness end-to-end on this host**~~ — **done
   2026-04-17** (arc_easy / arc_challenge / truthfulqa_mc2 at n=500 all
   within 1σ of Microsoft's published BitNet numbers; σ neutral at
   τ=0.0; [docs/v102/RESULTS.md](v102/RESULTS.md)).
0b. ~~**v103 σ-τ-Sweep infrastructure**~~ — **landed 2026-04-17**
    (post-hoc AURCC / AUACC / Coverage@target analyser on top of a single
    σ-logged eval pass; six gating signals compared head-to-head including
    an entropy-only baseline and a random floor; see
    [docs/v103/THE_TAU_SWEEP.md](v103/THE_TAU_SWEEP.md) and
    [docs/v103/SELECTIVE_PREDICTION.md](v103/SELECTIVE_PREDICTION.md)).
0c. ~~**v103 σ-τ-Sweep full-curve numbers on this host**~~ — **done
    2026-04-17** (7 591 ll-calls × 6 gating signals × 3 tasks; bootstrap
    CI95 on every AURCC; [docs/v103/RESULTS.md](v103/RESULTS.md)).
    **Measured outcome: pre-registered Outcome 2 (σ_mean ≅ entropy).**
    All three ΔAURCC(σ_mean − entropy) within bootstrap CI.  Unregistered
    secondary finding: σ_max_token (worst per-token σ) beats entropy on
    arc_challenge (Δ = −0.020) and truthfulqa_mc2 (Δ = −0.044),
    suggestive but sub-CI at n=500.
0d. ~~**v103 follow-up — confirm / refute σ_max_token advantage at
    n = 5 000 per task**~~ — **done 2026-04-18** (run at `--limit 5000`
    resolves to full validation/test splits: truthfulqa_mc2 n = 817,
    arc_challenge n = 1 172, arc_easy n = 2 376 → 4 365 docs / 20 070
    ll-calls / 2 h 37 min wall on Apple M3).  σ_max_token − entropy
    gap **survives**: paired-bootstrap ΔAURCC = −0.0447, CI [−0.071,
    −0.019], p = 0.0005 Bonferroni-significant on truthfulqa_mc2
    (v103 secondary **promoted P → M**; `docs/v103/` secondary entry
    updated).  Gap does **not** survive on arc_challenge
    (Δ = −0.010, p = 0.35, CI covers 0); arc_easy has no headroom
    (acc = 0.76).  v104 operator-search ran in the same pass:
    [docs/v104/RESULTS.md](v104/RESULTS.md) +
    [docs/v104/OPERATOR_SEARCH.md](v104/OPERATOR_SEARCH.md).
0e. ~~**v104 σ-operator search**~~ — **done 2026-04-18**.
    Pre-registered H1 (σ_max_token > entropy, truthfulqa_mc2) ✓
    confirmed Bonferroni-significant across 10 candidates.  H2 (some
    single σ channel beats entropy) ✓ confirmed: `n_effective`
    (p = 0.0005) and `tail_mass` (p = 0.004) Bonferroni-beat entropy
    on truthfulqa; `logit_std` Bonferroni-**hurts** on both
    arc_easy and arc_challenge (p = 0.0005) — the mean σ aggregator
    currently amplifies an anti-signal.  H3 (entropy-σ hybrid has
    interior optimum, Bonferroni-significant) ✗ not confirmed.
    **Unregistered secondary:** `sigma_product` beats entropy
    Bonferroni-significantly on **both** hard tasks simultaneously
    (truthfulqa_mc2 p = 0.003, arc_challenge p = 0.004) → tier P
    pending independent re-test at a second scale.
0f. **v105 representation surgery — phase 1 done.** The default
    aggregator in `cos_v101_sigma_from_logits` is now the geometric
    mean (`sigma_product`) of the eight channels; `sigma_arith_mean`
    and `sigma_product` are both always on the struct, runtime-
    selectable via `cos_v101_set_default_aggregator` or
    `COS_V101_AGG=mean|product`, and the CLI JSON output carries all
    three scalars (`sigma`, `sigma_mean`, `sigma_product`) plus a
    `sigma_aggregator` label.  v102/v103/v104 sidecar readers are
    bit-for-bit backward compatible because `sigma_mean` still means
    "arithmetic mean over tokens and channels".  `make merge-gate`:
    31/31 on `check-v101` (was 19/19).  Full rationale:
    `docs/v105/REPRESENTATION_SURGERY.md`.  Phase 2 (dropping
    `logit_std` from the default profile, or learning per-channel
    weights on a held-out split) intentionally deferred: v105 on
    its own has no new measurement claim, and the rest of the
    production track (v106–v110) is ordered strictly to ship a
    usable local LLM with σ-governance before reopening the
    architecture surgery.
0g. **v106 σ-Server (next)** — OpenAI-compatible HTTP layer around
    the existing v101 bridge.  `POST /v1/chat/completions`,
    `/v1/completions`, `/v1/models`, plus Creation-OS-specific
    `GET /v1/sigma-profile` and `GET /health`.  SSE streaming for
    `chat.completions.stream=true` so Cursor / Continue work
    unmodified.  Config via `~/.creation-os/config.toml`; σ
    aggregator selectable per request (falls back to server
    default = `product`).  No BitNet specificity at this layer; the
    model path is a CLI argument and the σ-math is
    logit-distribution-only.  The abstention threshold τ, the
    aggregator, and the streaming sigma-profile are all visible to
    the HTTP client — the point of v106 is to make σ-governance
    a first-class citizen of the OpenAI protocol surface.
0h. **v107 σ-Installer** — Homebrew formula + `curl | sh` script +
    Docker image from one universal-binary CI artefact.  No source
    build required on target machines.  Homebrew tap:
    `spektre-labs/creation-os`.  Docker Hub:
    `spektrelabs/creation-os`.  Decision point for Lauri: does
    `brew install spektre-labs/creation-os/cos` work on a clean macOS?
0i. **v108 σ-Chat (Web UI)** — single static HTML file that talks to
    the v106 server.  Vanilla JS only, no build step, no framework.
    Renders σ-channel bars per response, abstention indicator, and
    an asettings panel for τ / aggregator / model.
    `http://localhost:8080/` serves it.
0j. **v109 σ-MultiModel** — regression tests against Llama-3.1-8B,
    Qwen2.5-7B, Mistral-7B (GGUF Q4_K_M).  If `sigma_product` is
    cross-model robust the tier for "σ-stack is model-agnostic"
    moves P → M and the tagline changes from "BitNet σ-wrapper" to
    "σ-governance layer for any local GGUF model".
0k. **v110 σ-Launch** — README rewrite with install-first ordering,
    60-second silent demo video, Reddit / HN posts once the
    previous v-levels have each cleared their acceptance test.
_Original 0f gate text for the record — "swap the σ_mean > τ
abstention gate for either σ_product > τ (default; cross-task
robust) or σ_max_token > τ (hard-task tuned; known H1-confirmed
winner on truthfulqa_mc2); rethink v29: drop logit_std from the
default profile or learn per-channel weights via a calibration
objective on a held-out split; no RLHF here — that is deficiency
#7 and still follows v105."  v105 delivers the default-swap half
of that sentence; the "drop logit_std / per-channel weights" half
is held over to a later representation-surgery phase._
1. **PQC: ML-KEM-768 + ML-DSA-65 in v63 σ-Cipher** (harvest-now-decrypt-
   later is a dated risk).
2. **Real seL4 compartmentalisation** of v60..v80 as CAmkES components
   (turns the AND gate into a hardware-enforced AND).
3. **Wasmtime component-model tool sandbox** wired through v64
   σ-Intellect (tool execution becomes zero-ambient-authority).
4. **v79 ⇄ v80 agentic loop** so reasoning actually updates the world
   model (closes the learner loop, moves from *verifier stack* to *agent*).
5. **Real multimodal input** — voice via AVAudioEngine, vision via
   iOS 26 Foundation Models — feeding v62.
6. **Streaming generation with per-chunk composed decision** (unlocks
   chat-class UX without compromising the AND gate).
7. **zkML: per-layer NANOZK-style receipt in v72 σ-Chain** (the
   ATTESTATION.json becomes a live proof of the specific emission).
8. **Cosign + Rekor + SLSA-3 release pipeline** (the repo finally carries
   cryptographic lineage from commit to running binary).
9. **macOS menu-bar GUI + signed `.pkg` installer** (drops the terminal
   prerequisite for 95 % of humans who would use this).
10. **TLA+ spec of the composed decision + `make tla-check`** (makes the
    AND-of-twenty a theorem rather than a claim).

Each of these is a bounded-scope, single-PR-sized unit of work, and
each moves Creation OS closer to what the invariant demands: twenty
branchless integer kernels, one composed verdict, local-only, verified,
attested, post-quantum, formally-checked, *naurettavan helppo*. **1 = 1.**
