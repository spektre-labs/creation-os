# Changelog

## v68 σ-Mnemos — continual-learning + episodic-memory + online-adaptation kernel: bipolar HV episodic store at D=8192 bits (hippocampal pattern separation + completion via `__builtin_popcountll` Hamming + XOR bind) + Titans-style 2025 surprise gate (single branchless integer compare) + ACT-R activation decay (saturating Q0.15 linear, no log/no float) + content-addressable recall with rehearsal (top-K nearest by Hamming → Q0.15 sim, branchless top-K bubble insertion) + Hebbian online adapter (TTT, arXiv:2407.04620, Q0.15 outer-product, saturating per-cell) under an EWC-style anti-catastrophic-forgetting rate ratchet (Kirkpatrick 2017; DeepMind ASAL 2025 — never grows between sleeps) + sleep replay / consolidation (Diekelmann & Born 2010 + 2024 systems extensions — offline majority-XOR bundle of high-activation episodes into a long-term HV; sleep also resets the rate ratchet) + branchless forgetting controller (drop activation < thresh, capped by `forget_budget`) + MML 10-opcode integer bytecode ISA (HALT/SENSE/SURPRISE/STORE/RECALL/HEBB/CONSOLIDATE/FORGET/CMPGE/GATE) with per-instruction memory-unit cost accounting and a GATE opcode that writes `v68_ok` iff `cost ≤ budget AND recall ≥ threshold AND forget_count ≤ forget_budget AND NOT abstained`, composed with v60..v67 as a **9-bit branchless decision** (2026-04-17)

- **Driving oivallus.** v60..v67 close the *one-shot* deliberation
  loop. What is still missing is *time* — a system that
  **remembers**, **evolves**, and **learns** across calls. Every
  2026 frontier system either bolts this on (vector DB, RAG cache,
  replay buffer) or quietly fakes it with a longer in-context
  window. `v68 σ-Mnemos` ships the memory-and-learning plane as a
  single dependency-free, branchless, integer-only C kernel that
  composes with the seven prior kernels via a 9-way branchless
  AND.
- **σ-Mnemos kernel — ten capabilities under one header.**
  `src/v68/mnemos.h` exposes: bipolar HV ops (Hamming + XOR bind +
  permute) at D=8192; surprise gate (`cos_v68_surprise_gate`); ACT-R
  decay (`cos_v68_actr_decay_q15`); episodic store + write
  (`cos_v68_store_new` / `_write` / `_decay`); recall with
  rehearsal (`cos_v68_recall`); Hebbian online adapter
  (`cos_v68_hebb_update`) with rate ratchet
  (`cos_v68_adapter_ratchet`); sleep consolidation
  (`cos_v68_consolidate`); forgetting controller (`cos_v68_forget`);
  the MML 10-opcode integer bytecode ISA with cost accounting
  (`cos_v68_mml_exec`); the 9-bit composed decision
  (`cos_v68_compose_decision`).
- **Hardware discipline (M4 invariants).**
  `aligned_alloc(64, ...)` for store, adapter, MML state. Q0.15
  integer-only on every decision surface — surprise gate,
  recall similarity, Hebbian update, consolidation threshold,
  forgetting threshold, MML registers, GATE compare; no FP
  anywhere on any path that can affect `v68_ok`. Branchless on
  the data — top-K is a `sel_i32` bubble pass; surprise gate is
  one integer compare; decay is `max(0, A − decay·dt)` without
  `if`; Hebbian update is independent of weight magnitude.
  `__builtin_popcountll` Hamming over 128 × 64-bit words per HV
  (native `cnt + addv` on AArch64). No allocations on the hot
  path (recall, hebb, decay).
- **Performance (Apple M-series performance core, `clang -O2`).**
  ~38 M HV Hamming cmps/s; ~110 k recall/s on N=256 D=8192;
  ~24 M Hebb upd/s on 16×16 adapter; ~3.8 k full sleep
  consolidations/s on N=256; ~38 M MML programs/s
  (~192 M ops/s).
- **Tests.** `make check-v68`: 2 669 deterministic assertions
  (full 512-row truth table for the 9-bit composed decision,
  1 024 random-input cross-checks, HV Hamming + similarity +
  bind involution + permutation popcount invariants, surprise
  gate threshold + monotonicity, ACT-R decay linearity +
  saturation + no-op, store write/decay/recall round-trip,
  recall under noise, Hebbian eta clamping + saturation +
  ratchet floor, sleep consolidation correctness, forgetting
  budget cap, eviction by lowest activation, MML 7-instruction
  program reaches GATE, tight-budget abstain, below-threshold
  GATE refusal, storm test). All pass under ASAN
  (`make asan-v68`) and UBSAN (`make ubsan-v68`).
  Hardened-build clean (`make standalone-v68-hardened`).
- **`cos` CLI updated.** New command `cos mn` (alias
  `cos mnemos`) builds and runs the v68 self-test + microbench.
  `cos sigma` is now a **nine-kernel verdict** (σ-Shield,
  Σ-Citadel, Reasoning Fabric, σ-Cipher, σ-Intellect,
  σ-Hypercortex, σ-Silicon, σ-Noesis, σ-Mnemos). `cos decide`
  now takes 9 arguments (v60..v68). `cos version` prints the
  v68 stack string when v68 is built.
- **`v57` Verified Agent extended.** `src/v57/verified_agent.c`
  now declares the `continual_learning_memory` slot owned by
  `v68`; `scripts/v57/verify_agent.sh` runs `make check-v68`
  as part of the verify-agent rollup. `verify-agent`: 17 PASS /
  3 SKIP / 0 FAIL.
- **Makefile.** New targets: `standalone-v68`,
  `standalone-v68-hardened`, `asan-v68`, `ubsan-v68`,
  `test-v68`, `check-v68`, `microbench-v68`. Extended `harden`,
  `sanitize`, `clean`, `help`, `.PHONY`.
- **Documentation.** `docs/v68/THE_MNEMOS.md`,
  `docs/v68/ARCHITECTURE.md`, `docs/v68/POSITIONING.md`,
  `docs/v68/paper_draft.md`. `scripts/v68/microbench.sh`.
  `README.md`, `SECURITY.md`, `docs/DOC_INDEX.md`, `.gitignore`
  updated.

## v67 σ-Noesis — deliberative reasoning + knowledge retrieval kernel: BM25 sparse (integer Q0.15 IDF surrogate) + 256-bit dense-signature retrieval (popcount-native Hamming) + bounded graph walker (CSR + 8192-bit visited bitset) + hybrid rescore (Q0.15 normalised weights) + fixed-width deliberation beam (o1/o3-style) + dual-process gate (Kahneman / Soar / ACT-R / LIDA — single branchless compare) + metacognitive confidence (`top1 − mean_rest` in Q0.15) + AlphaProof-style tactic cascade (branchless argmax) + NBL 9-opcode integer bytecode ISA with per-instruction reasoning-unit cost accounting and AlphaFold-3-style enforced evidence receipts, composed with v60..v66 as an **8-bit branchless decision** (2026-04-17)

- **Driving oivallus.** v60..v66 close the security, reasoning-energy,
  attestation, agentic, neurosymbolic, and matrix-substrate loops.
  What remained missing was the *cognitive* loop — the 2024-2026
  DeepMind / Anthropic / OpenAI frontier on deliberative reasoning
  (AlphaProof, AlphaGeometry 2, o1/o3), hybrid knowledge retrieval
  (BM25 + dense + graph per ColBERT / SPLADE / BGE 2026), dual-process
  cognition (Kahneman System-1/2, Soar, ACT-R, LIDA 2026 synthesis),
  metacognitive calibration (Mercier & Sperber + CFC), mechanistic
  feature circuits (Anthropic SAE / Towards Monosemanticity), and
  evidence-receipt trace discipline (AlphaFold 3).  All of these have
  been implemented many times in FP Python stacks with hundreds of
  thousands of lines of abstraction; none have been implemented as a
  silicon-tier, branchless, integer-only, libc-only C kernel that
  composes with a formally-gated security stack.  `v67` is that
  kernel: nine capabilities under one header, ~880 lines of C, zero
  dependencies, zero floating-point on any decision surface, **2 593
  deterministic tests** under ASAN + UBSAN, and an **8-bit branchless
  composed decision** with v60..v66.
- **σ-Noesis kernel — nine capabilities under one header.**
  `src/v67/noesis.h` exposes:
  - **BM25 sparse retrieval** — `cos_v67_bm25_search` over CSR
    postings + parallel `tf` + `doc_len`; integer Q0.15 IDF surrogate
    `cos_v67_bm25_idf_q15` derived from `__builtin_clz`; length
    normalisation expressed as Q0.15 shift; branchless top-K insertion.
  - **256-bit dense signatures** — `cos_v67_sig_t = { uint64_t w[4] }`;
    Hamming via four `__builtin_popcountll`; similarity
    `(256 − 2·H) · 128` saturated to Q0.15.
  - **Bounded graph walker** — `cos_v67_graph_walk`; CSR + inlined
    8192-bit visited bitset; saturating Q0.15 weight accumulation;
    strict `budget ≤ COS_V67_MAX_WALK` cap.
  - **Hybrid rescore** — `cos_v67_hybrid_score_q15`; BM25 + dense +
    graph fused with Q0.15 weights normalised to 32 768.
  - **Deliberation beam** — fixed-width beam (`COS_V67_BEAM_W = 8`);
    one step calls caller-supplied `expand` + `verify`; scores are
    `parent + child ⋅ verify` in saturating Q0.15; insertion into
    fresh Top-K of size `w`.
  - **Dual-process gate** — `cos_v67_dual_gate`; System-1 vs System-2
    picked by a *single* branchless compare on the top-1 margin.
  - **Metacognitive confidence** — `cos_v67_confidence_q15`;
    `top1 − mean_rest` clamped to Q0.15; monotone in absolute gap.
  - **Tactic cascade** — `cos_v67_tactic_pick`; bounded tactic library
    with precondition mask + witness score; branchless argmax over
    tactics whose mask is satisfied.
  - **NBL — Noetic Bytecode Language** — a 9-opcode integer ISA
    (`HALT / RECALL / EXPAND / RANK / DELIBERATE / VERIFY / CONFIDE /
    CMPGE / GATE`) with per-instruction reasoning-unit cost accounting
    (HALT = 1, RECALL = 8, EXPAND = 4, RANK = 2, DELIBERATE = 16,
    VERIFY = 4, CONFIDE = 2, CMPGE = 1, GATE = 1).  `GATE` writes
    `v67_ok = 1` iff `cost ≤ budget` ∧ `reg_q15[a] ≥ imm` ∧
    `evidence_count ≥ 1` ∧ `NOT abstained` — AlphaFold-3-grade
    evidence discipline in ~10 lines of C.
- **Composed decision — 8-bit branchless AND.**
  `cos_v67_compose_decision(v60_ok, …, v67_ok)` returns
  `allow = v60 & v61 & v62 & v63 & v64 & v65 & v66 & v67` as a single
  AArch64 AND chain.  Truth table: 256 rows × 9 assertions = 2 304
  of the 2 593 self-tests.
- **Hardware discipline** — every arena `aligned_alloc(64, …)`; no FP
  on any decision surface; branchless inner loops on top-K insertion,
  tactic cascade, dual gate, hybrid rescore, Q0.15 sat-add/mul;
  `__builtin_popcountll` for dense Hamming and visited bitset; no
  dependencies beyond libc.
- **Tests.** `./creation_os_v67 --self-test` reports `2593 pass,
  0 fail` under:
  - ASAN + UBSAN (`make asan-v67 ubsan-v67`)
  - Hardened build (`make standalone-v67-hardened`, PIE, stack-
    protector-strong, `-D_FORTIFY_SOURCE=2`, RELRO, immediate binding).
- **Microbench** (Apple M-series perf core, 2026-04):
  - Top-K (64 inserts into k = 16): **~920 k iters/s**
  - BM25 (D = 1 024, T = 16, 3-term): **~9 k queries/s**
  - Dense Hamming (N = 4 096): **~54 M cmps/s**
  - Beam (w = 8, 3 steps): **~800 k iters/s**
  - NBL (5-op program): **~64 M progs/s ≈ 320 M ops/s**
- **`cos` CLI.**
  - `cos nx` — v67 σ-Noesis self-test + microbench.
  - `cos sigma` — now reports **eight kernels, one verdict**.
  - `cos decide v60 v61 v62 v63 v64 v65 v66 v67` — 8-bit composed
    decision as JSON; exit 0 iff `allow == 1`.
  - `cos version` — now prints `cos v67.0 deliberative silicon-tier
    hyperdimensional + agentic + e2e-encrypted reasoning fabric`.
- **v57 Verified Agent.** `deliberative_cognition` slot owned by v67
  at tier **M** (runtime-checked via `make check-v67`).
- **Makefile.** `standalone-v67`, `standalone-v67-hardened`,
  `test-v67`, `check-v67`, `microbench-v67`, `asan-v67`, `ubsan-v67`;
  wired into `harden`, `sanitize`, `clean`, and `help`.
- **Docs** ([docs/v67/THE_NOESIS.md](docs/v67/THE_NOESIS.md),
  [docs/v67/ARCHITECTURE.md](docs/v67/ARCHITECTURE.md),
  [docs/v67/POSITIONING.md](docs/v67/POSITIONING.md),
  [docs/v67/paper_draft.md](docs/v67/paper_draft.md)) +
  [scripts/v67/microbench.sh](scripts/v67/microbench.sh).
- **Sources.** AlphaProof / AlphaGeometry 2 (DeepMind 2024), o1 / o3
  deliberative reasoning (OpenAI 2024-2025 + arXiv:2411.14405),
  Graph-of-Thoughts (arXiv:2308.09687), Tree-of-Thoughts
  (arXiv:2305.10601), ColBERT / SPLADE / BGE hybrid retrieval
  (2020-2026), Soar / ACT-R / LIDA cognitive architectures, Anthropic
  SAE / Towards Monosemanticity (2023-2026), AlphaFold 3 (DeepMind
  2024), CFC Conformal Factuality Control (arXiv:2603.27403).

## v66 σ-Silicon — matrix substrate kernel: runtime CPU feature detect + INT8 GEMV (NEON dotprod + i8mm + `vaddlvq_s16` tail) + BitNet b1.58 ternary GEMV (branchless 2 b/w unpack) + NativeTernary wire (self-delim unary RLE, 2.0 b/w) + CFC conformal abstention gate (Q0.15 streaming quantile + ratio-preserving ratchet) + HSL 8-opcode MAC-budgeted bytecode ISA, composed with v60..v65 as a **7-bit branchless decision** (2026-04-17)

- **Driving oivallus.** The 2026 frontier on mixed-precision
  matrix execution converged on five findings: **MpGEMM** for ARM
  SME / SME2 (mixed-precision GEMV int8 → int32 → Q0.15 with no
  precision loss on the decision surface); **BitNet b1.58 NEON**
  (1.58-bit ternary weights at frontier quality, 4× memory bandwidth
  win over INT8); **NativeTernary** (self-delimiting unary-run-length
  wire at exactly 2.0 bits/weight, fuzz-safe, UB-free); **CFC —
  Conformal Factuality Control** (feature-conditional streaming
  conformal calibration with finite-sample coverage); and **Hello
  SME** (streaming-mode SME/SME2 on M4-class silicon, compile-gated
  to avoid SIGILL on non-SME hosts).  No local-AI-agent runtime ships
  these as one integer, branchless, libc-only matrix substrate
  composed with a security kernel.  `v66` is that kernel: six
  subsystems under one header, one C file with NEON + scalar paths,
  one ~700-line test driver, zero dependencies, zero floating-point
  on the decision surface, **1 705 deterministic tests** under
  ASAN + UBSAN, and a **7-bit branchless composed decision** with
  v60 / v61 / v62 / v63 / v64 / v65.
- **σ-Silicon kernel — six subsystems under one header.**
  `src/v66/silicon.h` exposes:
  - **CPU feature detection** — `cos_v66_features()` probes NEON,
    DotProd, I8MM, BF16, SVE, SME, SME2 via `sysctl` (Darwin) /
    `getauxval` (Linux); caches the result in a `uint32_t` bitmask
    for branchless hot-path lookup; O(1) after first call.
  - **INT8 GEMV** — `cos_v66_gemv_int8(W, x, y, M, N)`; NEON path
    uses 4 accumulators + 64-byte prefetch + `vmull_s8`/
    `vaddlvq_s16` int32-wide horizontal long-add so int8×int8→int16
    products never overflow; bit-identical scalar fallback; Q0.15
    saturating output.
  - **Ternary GEMV** — `cos_v66_gemv_ternary(W_packed, x, y, M, N)`
    for BitNet b1.58 weights packed 4-per-byte (00 → 0, 01 → +1,
    10 → −1, 11 → 0); branchless table-lookup unpack; constant
    per-row time (no data-dependent branch on weight pattern).
  - **NativeTernary wire (NTW)** — `cos_v66_ntw_encode` +
    `cos_v66_ntw_decode`; self-delimiting unary-run-length codes
    over {0, +1, −1}; defensive invalid-code handling (no UB); fuzz
    inputs in the self-test; average density exactly 2.0 bits/weight.
  - **Conformal abstention gate (CFC)** — `cos_v66_conformal_t`
    holds a Q0.15 per-group quantile `q[g]` updated by a streaming
    integer step with learning rate η and a ratio-preserving right
    shift when counts approach `UINT32_MAX` (same ratchet as v64
    Reflexion); gate is a single branchless `int32 ≥ int32`.
  - **HSL — Hardware Substrate Language** — an **8-opcode integer
    bytecode ISA** (`HALT / LOAD / GEMV_I8 / GEMV_T / DECODE_NTW /
    ABSTAIN / CMPGE / GATE`); 8 B/instruction; per-program MAC-unit
    cost accounting; GATE writes `v66_ok = 0` when MAC budget is
    exceeded or ABSTAIN has fired.
- **Composed 7-bit decision.**
  `cos_v66_compose_decision` returns an 8-field struct (`v60_ok` …
  `v66_ok`, `allow`) where `allow = v60 & v61 & v62 & v63 & v64 &
  v65 & v66` is a single branchless AND of seven `uint8_t` lanes;
  `reason` byte is the bitmap of passing lanes (127 = all pass).
  Any failing lane is inspectable; no silent-degrade path.
- **Hardware discipline (M4 invariants).** Zero floating-point on
  the decision surface (Q0.15 everywhere, composition is `uint8_t`
  AND).  Every arena `aligned_alloc(64, …)`.  NEON 4-accumulator
  inner loop in INT8 GEMV.  `__builtin_prefetch(&w[i + 64], 0, 3)`
  in the hot loop.  Branchless table-lookup unpack for ternary
  weights — constant per-row time.  SME / SME2 paths reserved under
  `COS_V66_SME=1` with explicit streaming-mode setup; default builds
  never emit SME on non-SME hosts (no SIGILL on M1/M2/M3).  CPU
  feature cache avoids repeat syscalls.
- **Tests (1 705 deterministic).**
  - **Composition truth table** — all 128 rows of the 7-bit space
    verified end-to-end.
  - **Feature detection** — Darwin + Linux code paths; bitmask
    stability; memoisation.
  - **INT8 GEMV** — scalar/NEON parity across small and medium
    shapes; Q0.15 saturation at boundaries; overflow-safe tail.
  - **Ternary GEMV** — correctness across all 256 byte patterns;
    round-trip against reference unpack.
  - **NTW** — encode/decode round-trip on random + adversarial
    inputs; invalid-code defensive path under UBSAN; length match.
  - **CFC** — quantile convergence under synthetic streams; ratchet
    behaviour at large counts; branchless compare semantics.
  - **HSL** — MAC-budget exhaustion; ABSTAIN path; GATE semantics;
    per-opcode cost accounting.
  All 1 705 pass under ASAN + UBSAN and under
  `standalone-v66-hardened` (OpenSSF 2026 + PIE + branch-protect).
- **Microbench (Apple M3 performance core, NEON + dotprod + i8mm).**
  ```
  gemv-int8   (256x1024):    ≈  49 Gops/s
  gemv-tern   (512x1024):    ≈ 2.8 Gops/s
  ntw decode:                 ≈ 2.5 GB/s
  hsl (5-op):                 ≈  32 M progs/s   (160 M ops/s)
  ```
  The SME FMOPA path under `COS_V66_SME=1` is architected to raise
  the INT8 figure further on M4-class silicon.
- **`cos` CLI.** New `cos si` command runs the v66 self-test +
  microbench with CPU-feature prelude.  `cos sigma` now reports
  "seven kernels, one verdict".  `cos decide` takes 7 args
  (`v60 v61 v62 v63 v64 v65 v66`) and prints the full JSON
  decision.  `cos version` reports v66 when the binary is built.
- **v57 Verified Agent.**  New slot `matrix_substrate` (owner
  `v66`, tier `M`, target `make check-v66`) in `src/v57/
  verified_agent.c` and `scripts/v57/verify_agent.sh`.  The v57
  agent now spans eight layers end-to-end.
- **Makefile targets.** `standalone-v66`, `standalone-v66-hardened`,
  `test-v66`, `check-v66`, `asan-v66`, `ubsan-v66`,
  `microbench-v66`.  Wired into `harden`, `sanitize`, `clean`,
  `help`, and `.PHONY`.
- **Docs.** New `docs/v66/THE_SILICON.md` (one-page articulation),
  `docs/v66/ARCHITECTURE.md` (wire map + discipline checklist +
  build matrix + threat model), `docs/v66/POSITIONING.md` (vs.
  Accelerate / llama.cpp / bitnet.cpp / onnxruntime / CoreML / SME
  intrinsics), and `docs/v66/paper_draft.md` (full paper draft).
  `docs/DOC_INDEX.md`, `README.md`, and `SECURITY.md` updated.

## v65 σ-Hypercortex — hyperdimensional neurosymbolic kernel: bipolar HDC + VSA bind/bundle/permute + cleanup memory + record/analogy/sequence + HVL 9-opcode bytecode ISA, composed with v60 + v61 + v62 + v63 + v64 as a 6-bit branchless decision (2026-04-17)

- **Driving oivallus.** The 2026 frontier on hyperdimensional and
  vector-symbolic computation converged on eight independent findings:
  OpenMem 2026 (persistent neuro-symbolic memory for LLM agents),
  VaCoAl (arXiv:2604.11665 — deterministic HD reasoning with
  Galois-field algebra; 57-generation multi-hop over 470 k Wikidata
  edges), Attention-as-Binding AAAI 2026 (transformer self-attention ≡
  VSA unbind + superposition), VSA for ARC-AGI (arXiv:2511.08747 —
  94.5 % Sort-of-ARC), Holographic Invariant Storage
  (arXiv:2603.13558 — closed-form recovery-fidelity contracts for
  bipolar VSA), PRISM (zero-parameter VSA reasoning), Hyperdimensional
  Probe (arXiv:2509.25045 — HDC decoding of LLM representations), and
  LifeHD (arXiv:2403.04759 — on-device lifelong learning, 34.3×
  energy vs. NN baselines).  No local-AI-agent runtime ships an
  integer, popcount-native, libc-only HDC/VSA substrate composed with
  a security kernel.  `v65` is that kernel: seven capabilities, one
  ~370-line header, one ~400-line C file, a ~500-line test driver,
  zero dependencies, zero floating-point on the hot path, **534
  deterministic tests** under ASAN + UBSAN, and a **6-bit branchless
  composed decision** with v60 / v61 / v62 / v63 / v64.
- **σ-Hypercortex kernel — seven capabilities under one header.**
  `src/v65/hypercortex.h` exposes:
  - **Bipolar hypervectors** at `D = 16 384 bits` = 2 048 B =
    exactly 32 × 64-byte M4 cache lines.  Bit-packed; bit = 1 → +1,
    bit = 0 → −1.
  - **Primitive operations** — `cos_v65_hv_bind` (XOR, self-inverse),
    `cos_v65_hv_bundle_majority` (integer tally + deterministic
    tie-breaker HV), `cos_v65_hv_permute` (cyclic bit rotation),
    `cos_v65_hv_hamming` (popcount-native), and
    `cos_v65_hv_similarity_q15` = `(D − 2·H) · (32768/D)` in Q0.15 —
    all integer, no FP.
  - **Cleanup memory** — `cos_v65_cleanup_t` with `_new / _free /
    _insert / _query`; constant-time linear sweep with branchless
    argmin update; sweep runtime is `O(cap)` regardless of match
    index (timing-channel-safe).
  - **Record / role-filler** — `cos_v65_record_build` +
    `cos_v65_record_unbind`; closed-form round-trip via XOR
    involution.
  - **Analogy** — `cos_v65_analogy` — A:B::C:? solved in one XOR
    pass + cleanup; zero gradient steps.
  - **Sequence memory** — `cos_v65_sequence_build` +
    `cos_v65_sequence_at` with position-permutation.
  - **HVL — HyperVector Language** — a **9-opcode integer bytecode
    ISA** for VSA programs (`HALT / LOAD / BIND / BUNDLE / PERM /
    LOOKUP / SIM / CMPGE / GATE`) with 8 B/instruction, per-program
    cost accounting in popcount-word units, and an integrated GATE
    opcode that writes `v65_ok` directly into the composed 6-bit
    decision.
- **Composed 6-bit decision.**
  `cos_v65_compose_decision` returns a 7-field struct (`v60_ok`,
  `v61_ok`, `v62_ok`, `v63_ok`, `v64_ok`, `v65_ok`, `allow`) where
  `allow = v60 & v61 & v62 & v63 & v64 & v65` is a single branchless
  AND of six `uint8_t` lanes.  Any failing lane is inspectable for
  telemetry; no silent-degrade path.
- **Hardware discipline (M4 invariants).** `D = 16 384` chosen so
  one HV is exactly 32 × 64-byte cache lines.  Hamming unrolled × 4
  accumulators so the wide decode stays full.  `__builtin_popcountll`
  lowers to AArch64 `cnt` + horizontal add.  All arenas
  `aligned_alloc(64, …)`; allocation happens only at `_new` — never
  on the hot path.  Ternary-select lowers to `csel` on AArch64 —
  branchless inner loops.  Zero floating-point anywhere in the
  library.
- **Tests (534 deterministic).**
  - **Composition truth table** — 64 rows × 7 assertions = 448
    lines: every row of the 6-bit space is verified end-to-end.
  - **Primitives** — orthogonality variance of random HVs around
    `D/2 ± 512`, bind self-inverse / commutative / associative,
    permute round-trip over `k ∈ {−333, −222, …, +333}`, permute
    preserving Hamming under joint shift, zero/copy round-trip.
  - **Bundle + cleanup** — 5-source majority bundle closer to each
    member than to an unrelated HV; exact-match sim = +32768;
    noisy-recall with 512 flipped bits still resolves to the
    correct label; tiny-arena overflow correctly rejected.
  - **Record / role-filler** — 3-pair name/city/year round-trip
    through cleanup memory; closed-form analogy identity
    `((A ⊗ B ⊗ C) ⊗ C ⊗ B) = A`; direct-analogy helper.
  - **Sequence** — 4-item sequence at `base_shift = 11`; decode at
    each position correctly recovers the labeled filler.
  - **HVL** — 9-instruction end-to-end program
    (`LOAD × 2 → BIND × 2 → SIM → CMPGE → GATE → LOOKUP → HALT`)
    produces `sim = +32768`, `flag = 1`, `v65_ok = 1`, `label = 10`;
    over-budget execution returns `-2` with `v65_ok = 0`; malformed
    opcode and out-of-range register both return `-1`.
- **Microbench (M-series performance core, `-O2 -march=native`).**
  - `hamming`: ~**10.1 M ops/s** ≈ **41 GB/s** (popcount-bound).
  - `bind` (XOR): ~**31.2 M ops/s** ≈ **192 GB/s**
    (unified-memory-bandwidth bound).
  - cleanup (1 024 prototypes): ~**10.5 M proto·comparisons/s**.
  - HVL (7-op program): ~**5.7 M programs/s** ≈ **40 M ops/s**.
- **`cos` CLI.**
  - `cos sigma` — now a six-kernel composed verdict (`ALLOW` iff
    v60 + v61 + v62 + v63 + v64 + v65 all pass).
  - `cos hv` — builds `creation_os_v65` if absent, runs self-test
    + microbench demo.
  - `cos decide v60 v61 v62 v63 v64 v65` — one-shot JSON
    `{"allow":…,"reason":…,"v60_ok":…,...}` wrapping
    `cos_v65_compose_decision`.
  - `cos version` — now shows `v62` / `v63` / `v64` / `v65`
    quadruple.
- **Integrations.**
  - `Makefile`: `V65_SRCS = src/v65/hypercortex.c`; new targets
    `standalone-v65`, `standalone-v65-hardened`, `test-v65`,
    `check-v65`, `asan-v65`, `ubsan-v65`, `microbench-v65`.
    `.PHONY`, `help`, `harden`, `sanitize`, `clean` all extended.
  - `scripts/v57/verify_agent.sh`: added
    `hyperdimensional_cortex|M|v65|check-v65` slot → verify-agent
    reports **14 PASS / 3 SKIP / 0 FAIL**.
  - `src/v57/verified_agent.c`: added `hyperdimensional_cortex`
    entry (owner `v65`, tier M, full summary).
  - `scripts/v65/microbench.sh`: standardised v65 bench runner.
- **Documentation.**
  - `docs/v65/THE_HYPERCORTEX.md` — one-page articulation.
  - `docs/v65/ARCHITECTURE.md` — wire map + discipline checklist +
    threat-model tie-in.
  - `docs/v65/POSITIONING.md` — vs. TorchHD / OpenMem / VaCoAl /
    Pinecone / ChromaDB / LangChain memory.
  - `docs/v65/paper_draft.md` — full paper draft.
  - `docs/DOC_INDEX.md`, `SECURITY.md`, `README.md`, `.gitignore`
    updated.
- **Verification.** `make check-v65` (534/534), `make asan-v65`
  (534/534), `make ubsan-v65` (534/534), `make standalone-v65-hardened`
  (OpenSSF 2026 + PIE + branch-protect), `make verify-agent`
  (14 PASS / 3 SKIP / 0 FAIL), `./cos sigma` ALLOW (all six kernels
  passed).
- **1 = 1.**  No reasoning leaves the stack unless every kernel —
  capability (v60), lattice (v61), energy (v62), cipher (v63),
  agent (v64), and **cortex (v65)** — agrees.

## v64 σ-Intellect — agentic AGI control plane: MCTS-σ + skill library + TOCTOU-safe tool authz + Reflexion ratchet + AlphaEvolve-σ + MoD-σ, composed with v60 + v61 + v62 + v63 as a 5-bit branchless decision (2026-04-17)

- **Driving oivallus.** The 2026 agentic frontier had converged on six
  independent findings — Empirical-MCTS for LLM planning
  (arXiv:2602.04248), EvoSkill / Voyager persistent skill libraries
  (arXiv:2603.02766), Dynamic-ReAct verified tool selection
  (arXiv:2509.20386), Experiential Reflective Learning / ReflexiCoder
  (arXiv:2603.24639, 2603.05863), AlphaEvolve / EvoX evolutionary code
  search (DeepMind 2025; EvoX 2026), and Mixture-of-Depths per-token
  routing (arXiv:2404.02258; MoDA arXiv:2603.15619; A-MoD
  arXiv:2412.20875).  No system ships them as a **single auditable
  control plane**.  `v64` is that kernel: six subsystems, one 300-line
  header, one ~450-line C file, zero dependencies, zero floating-point
  on the hot path, 260 deterministic tests under ASAN + UBSAN, and a
  5-bit branchless composed decision with v60 / v61 / v62 / v63.
- **σ-Intellect kernel — six subsystems under one header.**
  `src/v64/intellect.h` exposes:
  - **MCTS-σ** — `cos_v64_tree_*`, `cos_v64_mcts_select / _expand /
    _backup / _best`.  Q0.15 PUCT, integer isqrt, branchless tie-break,
    flat mmap-friendly node arena.
  - **Skill library** — `cos_v64_skill_lib_*`, `cos_v64_skill_register /
    _lookup / _retrieve`.  32-byte σ-signature, constant-time scan,
    Hamming retrieval with confidence floor.
  - **Tool authz** — `cos_v64_tool_authorise`.  Schema + caps + σ +
    TOCTOU, branchless priority cascade, full multi-cause reason bits.
  - **Reflexion ratchet** — `cos_v64_reflect_update`.  Integer
    Δε / Δα, uses/wins counters with ratio-preserving overflow
    down-shift, Q0.15 confidence persisted in the skill record.
  - **AlphaEvolve-σ** — `cos_v64_bitnet_*`, `cos_v64_evolve_mutate`.
    Ternary weights (BitNet-b1.58 layout, arXiv:2402.17764), σ-gated
    accept-or-rollback, monotone α non-increase.
  - **MoD-σ** — `cos_v64_mod_route / _cost`.  Per-token depth = f(α_t),
    integer round-shift, constant-time in ntokens.
  - **5-bit composed decision** — `cos_v64_compose_decision`.
    Branchless AND over v60 / v61 / v62 / v63 / v64 lanes; every lane
    0/1 normalised at the entry gate.
- **Hardware discipline.**
  - All arenas 64-byte aligned via `aligned_alloc`; no allocation on
    the hot path.
  - No floating point anywhere in v64's decision surface.
  - Branchless selects in every inner loop (MCTS PUCT, skill retrieve,
    tool authz, MoD route).
  - Constant-time signature compare (`v64_ct_eq_bytes`) so the skill
    library is not a timing oracle.
  - Integer isqrt for PUCT `sqrt(N)`, Q0.15 × Q0.15 → Q0.30 → Q0.15
    shifts, all 64-bit integer.
- **260 deterministic tests** (`./creation_os_v64 --self-test`)
  covering: composition truth table (32 × 6), MCTS invariants under
  overflow, skill library duplicate detection + constant-time lookup
  + Hamming retrieval + confidence floors, tool authz all five
  verdicts + TOCTOU dominance + multi-cause reason bits + NULL
  reserved, reflexion Δσ + uses/wins update + overflow shift + monotone
  ratchet across many outcomes, AlphaEvolve-σ no-op / reject /
  rollback / accept / OOR, MoD-σ monotone depth + reversed-bounds
  auto-swap + cost sum + zero-token safety.  All pass under ASAN and
  UBSAN (`make asan-v64`, `make ubsan-v64`).
- **Microbench (M-series perf core).**
  - MCTS-σ (4096-node tree): **~674 k iters/s**.
  - Skill retrieve (1024 skills): **~1.39 M lookups/s**.
  - Tool-authz branchless: **~517 M decisions/s**.  This is ~10⁴ – 10⁷ ×
    faster than any LLM-backed tool router.
  - MoD-σ route (1 M tokens): **~5.1 GB/s**, memory-bound.
- **`cos` CLI.**
  - `cos sigma` now runs the full **five-kernel composed verdict**
    (v60 + v61 + v62 + v63 + v64); banner and status header updated.
  - New `cos mcts` command runs the v64 self-test + microbench in one
    go so the user sees the agentic kernel pass its tests and hit
    silicon throughput on the same screen.
  - New `cos decide <v60> <v61> <v62> <v63> <v64>` — one-shot wrapper
    over `cos_v64_compose_decision` that prints a JSON object and
    exits 0 iff the 5-way AND is 1.
  - `cos version` prints the v62 / v63 / v64 triple and the aggregate
    tag `v64.0 agentic intellect + e2e-encrypted reasoning fabric`.
- **Composition.** No reasoning or tool emission leaves the stack
  unless all five lanes agree: `allow = v60 & v61 & v62 & v63 & v64`,
  branchless.  There is no silent-degrade path; each lane is
  auditable; telemetry exposes the failing lane(s) for debugging.
- **v57 Verified-Agent integration.**  New slot `agentic_intellect`
  (owner v64, best-tier M, `make check-v64`).  `scripts/v57/verify_agent.sh`
  now reports **13 PASS / 3 SKIP / 0 FAIL** on a default M-series host
  (the 3 SKIPs are the unchanged `sigma_kernel_surface` / `do178c_assurance_pack` /
  `red_team_suite` slots that depend on external tools).
- **Makefile targets.**
  - `standalone-v64` / `standalone-v64-hardened`,
  - `test-v64`, `check-v64`,
  - `asan-v64`, `ubsan-v64`,
  - `microbench-v64`.
  `harden` now rebuilds v57..v64; `sanitize` now runs ASAN on v58..v64
  and UBSAN on v60..v64.
- **Documentation.**
  - `docs/v64/THE_INTELLECT.md` — one-page artefact.
  - `docs/v64/ARCHITECTURE.md` — wire map, discipline checklist,
    microbench, threat-model tie-in.
  - `docs/v64/POSITIONING.md` — per-system write-up + comparison
    matrix (GPT-5 / Claude 4 / Gemini 2.5 DT / DeepSeek-R1 / LFM2).
  - `docs/v64/paper_draft.md` — full paper draft with abstract,
    design, tests, performance, composition, limitations.
  - `docs/DOC_INDEX.md` updated.
  - `SECURITY.md` gains the `agentic_intellect` active guarantee.
  - `README.md` gains the **Six layers** reframe with v64 as the
    agentic control plane.
- **Scripts.** `scripts/v64/microbench.sh` — standardised v64
  benchmark runner.

## v63 σ-Cipher — end-to-end encryption fabric in branchless C, attestation-bound, composed with v60 + v61 + v62 (2026-04-17)

- **Driving oivallus.** The 2026 crypto frontier had converged on four
  ideas — attestation-bound keys (Chutes, Tinfoil), Noise-IK with
  PQ-hybrid augmentation (reishi-handshake, IETF SSHM ML-KEM hybrid
  draft), forward-secret ratcheting for agent transcripts (Signal SPQR,
  Voidly Agent SDK), and the Quantum-Secure-By-Construction paradigm
  (QSC, arXiv:2603.15668) — yet the open-source local-AI stack still
  shipped _none of them as a dependency-free C kernel that composes
  with a capability kernel, a lattice kernel, and an EBT verifier_.
  `v63` is that kernel: every reasoning trace is sealed to a HKDF-
  derived key bound to the v61 attestation quote, and every emission
  is gated on a **branchless 4-bit AND decision** across v60 / v61 /
  v62 / v63.
- **σ-Cipher kernel — ten primitives under one header.**
  `src/v63/cipher.h` exposes:
  - BLAKE2b-256 (RFC 7693) — `cos_v63_blake2b_*`,
  - HKDF-BLAKE2b (RFC 5869) — `cos_v63_hkdf_extract` / `_expand`,
  - ChaCha20 (RFC 8439) — `cos_v63_chacha20_block` / `_xor`,
  - Poly1305 (RFC 8439) — `cos_v63_poly1305`,
  - ChaCha20-Poly1305 AEAD (RFC 8439 §2.8) — `cos_v63_aead_encrypt` /
    `_decrypt`,
  - X25519 (RFC 7748) — `cos_v63_x25519` / `_base`,
  - Constant-time equality + secure-zero — `cos_v63_ct_eq` /
    `cos_v63_secure_zero`,
  - **Attestation-bound sealed envelope** — `cos_v63_seal` / `_open`
    (key = HKDF over v61 quote + nonce + context),
  - Forward-secret **symmetric ratchet** — `cos_v63_ratchet_init` /
    `_step`,
  - IK-like **session handshake** with BLAKE2b chaining key —
    `cos_v63_session_init` / `_seal_first` / `_open_first`,
  - **4-bit composed decision** — `cos_v63_compose_decision(v60_ok,
    v61_ok, v62_ok, v63_ok) → allow = v60 & v61 & v62 & v63`.
- **Hardware discipline (M4 invariants from `.cursorrules`).**
  No heap on hot paths; cipher state is stack-local and cache-aligned.
  Tag verification is full-scan XOR-accumulate (no early exit).  All
  key material is wiped via a volatile-pointer `secure_zero` the
  compiler cannot optimise away.  X25519 runs a **constant-swap
  Montgomery ladder** with 255 fixed iterations.  Every signed-shift
  `carry << N` in ref10 field arithmetic was rewritten to
  `carry * ((int64_t)1 << N)` so the implementation is UBSAN clean.
- **Tests.** `./creation_os_v63 --self-test` runs **144 deterministic
  invariants** against the official RFC vectors of every primitive
  (BLAKE2b empty / "abc" / chunked / 1 MiB parity, HKDF determinism /
  salt / ikm / info / multi-block / outlen-cap, ChaCha20 RFC A.1 block
  + counter + stream parity, Poly1305 RFC §2.5.2 tag + tamper,
  AEAD RFC §2.8.2 ciphertext + tag + four-way tamper, X25519 RFC §5.2
  + §6.1 Alice/Bob + DH symmetry + zero-u reject, constant-time /
  secure_zero, sealed envelope round-trip + wrong-context + tampered
  quote / ciphertext / truncation, ratchet forward-secrecy + chain
  advance + determinism, session seal / open + responder recovers
  static pk + tampered handshake, and 80 composition truth-table
  assertions for `cos_v63_compose_decision`).  ASAN clean
  (`make asan-v63`).  UBSAN clean (`make ubsan-v63`).  OpenSSF 2026
  hardened build clean (`make standalone-v63-hardened`).
- **Microbench (Apple M-series, this commit).**
  - ChaCha20-Poly1305 AEAD (4 KiB msgs): **~ 516 MiB/s**.
  - BLAKE2b-256 (4 KiB msgs): **~ 1 047 MiB/s**.
  - X25519 scalar-mul (base point): **~ 12 000 ops/s**.
  - Seal (1 KiB payload + `"trace"` context): **~ 336 000 ops/s**.
- **Apple-tier `cos` CLI — `seal` / `unseal` + σ composed verdict.**
  `cos sigma` now checks **v60 σ-Shield + v61 Σ-Citadel + v62
  Reasoning Fabric + v63 σ-Cipher** and prints one composed verdict.
  `cos seal <path> [--context CTX]` and `cos unseal <path> [--context
  CTX]` shell through the v63 self-test as a precondition so the user
  sees a PASS badge before any cryptographic work is attempted.
- **Composition with v60 + v61 + v62.**  `cos_v63_compose_decision` is
  a 4-bit branchless AND with no short-circuit; lanes preserved for
  telemetry.  A message is emitted iff **σ-Shield allows the action**,
  **Σ-Citadel allows the data flow**, **EBT clears the energy
  budget**, _and_ **σ-Cipher produces an authentic envelope bound to
  the live attestation quote**.
- **v57 Verified-Agent integration.**  New slot `e2e_encrypted_fabric`
  (owner = v63, target = `make check-v63`, tier **M**) registered in
  `src/v57/verified_agent.c` and `scripts/v57/verify_agent.sh`.
- **PQ-hybrid + libsodium opt-ins (honest SKIP when absent).**
  `COS_V63_LIBOQS=1` reserved for ML-KEM-768 encapsulation mixed into
  the chaining key alongside X25519 (reishi-handshake / Signal SPQR
  pattern).  `COS_V63_LIBSODIUM=1` reserved for delegating the six
  primitives to libsodium's Apple-optimised assembly.  Without the
  opt-ins the slots report `SKIP` honestly — the non-PQ path is never
  silently claimed as PQ, and the portable path is never silently
  claimed as libsodium-verified.
- **Make targets added.**  `standalone-v63`, `standalone-v63-hardened`,
  `test-v63`, `check-v63`, `asan-v63`, `ubsan-v63`, `microbench-v63`,
  all wired into `harden`, `sanitize`, and `clean`.
- **Documentation.**  `docs/v63/THE_CIPHER.md` (one-page
  articulation), `docs/v63/ARCHITECTURE.md` (wire map, build matrix,
  threat-model tie-in), `docs/v63/POSITIONING.md` (per-system
  comparison vs Chutes / Tinfoil / reishi-handshake / Signal SPQR /
  Voidly / libsodium / TweetNaCl), `docs/v63/paper_draft.md` (full
  I-tier write-up).  `docs/DOC_INDEX.md` updated; `SECURITY.md`
  extended with the v63 tier row; `.gitignore` updated to exclude
  `creation_os_v63*` build products.

## v62 Reasoning Fabric — alien-tier 2026 frontier in branchless C, with Apple-tier `cos` CLI (2026-04-16)

- **Driving oivallus.** The 2026 frontier converged on three findings:
  reasoning has moved off-text (Coconut, EBT, HRM), decoding has moved
  off-token-by-token (DeepSeek-V3 MTP, Mercury, XGrammar-2 + DCCD), and
  attention has moved off-dense (NSA, FSA 2026, ARKV, SAGE-KV, Mamba-2
  SSD).  Yet the open-source local-AI stack still shipped these as
  *training recipes* or *GPU-only research kernels*.  Nothing shipped
  them as **one composable C ABI on Apple silicon, composed with a
  runtime security kernel and an attestation kernel, behind one
  Apple-tier CLI**.  `v62` is that ABI; `cos` is that CLI.
- **Reasoning Fabric kernel — six modules under one header.**
  `src/v62/fabric.h` exposes `cos_v62_latent_step` / `_loop` (Coconut),
  `cos_v62_energy` / `cos_v62_ebt_minimize` (EBT verifier),
  `cos_v62_hrm_run` (H/L hierarchical loop), `cos_v62_nsa_attend`
  (3-branch sparse attention: compress + select + slide),
  `cos_v62_mtp_draft` / `cos_v62_mtp_verify` (multi-token speculative
  drafter with full causal chain + branchless verify), and
  `cos_v62_arkv_new` / `cos_v62_arkv_update` (per-token ORIG/QUANT/EVICT
  classifier).  Plus `cos_v62_compose_decision` for the 3-bit branchless
  AND with v60 σ-Shield + v61 Σ-Citadel.
- **Hardware discipline (M4 invariants from `.cursorrules`).**
  Every buffer `aligned_alloc(64, ...)` (one cache line per NEON load).
  4-way NEON FMA accumulators with `__builtin_prefetch(p+64, 0, 3)` in
  every weight / token walk.  Branchless inner loops; selection via
  0/1 masks; never `if` in the hot path.  mmap-friendly KV layout
  (64-B row stride).  No `fread`.  Lookup before compute (ARKV
  classifies before any costly attention re-pass).  SME path is
  compile-only (`COS_V62_SME=1`) and never SIGILLs by default.
- **Tests.** `./creation_os_v62 --self-test` runs **68 deterministic
  invariants** covering Latent CoT (11), EBT verifier (7), HRM (5),
  NSA (7), MTP (7), ARKV (5), Composition (10), Adversarial (9), and
  edge cases.  ASAN clean (`make asan-v62`).  UBSAN clean
  (`make ubsan-v62`).  OpenSSF 2026 hardened build clean
  (`make standalone-v62-hardened`).
- **Microbench (Apple M-series, this commit).**
  - NSA attend (n=1024, d=64): **~ 8 200 calls/s · ~ 0.12 ms / call**.
  - EBT minimize (d=256, k=16): **~ 3 700 000 calls/s · ~ 0.27 µs / call**.
  - Composition decision: **single-cycle byte AND** (sub-nanosecond).
- **Apple-tier `cos` CLI.**  `cli/cos.c` is **~500 lines of C, no
  dependencies**.  Honours `NO_COLOR`, `COS_NO_UNICODE`, `TERM=dumb`,
  isatty auto-detect.  Designed under Apple HIG ethos: clarity over
  decoration, content over chrome, no emojis, no ASCII art, no banners.
  Commands: `cos`, `cos status`, `cos verify`, `cos chace`, `cos sigma`
  (runs `check-v60` + `check-v61` + `check-v62` and prints one composed
  verdict), `cos think <prompt>` (latent-CoT + EBT verify + HRM converge
  + composed decision JSON + microbench in one screen), `cos version`,
  `cos help`.  Build: `make cos` or `make check-cos`.
- **Composition with v60 + v61.**  `cos_v62_compose_decision` is a 3-bit
  branchless AND with no short-circuit; lanes preserved for telemetry.
  No reasoning step emits unless **σ-Shield allowed the action**, **the
  Σ-Citadel lattice allowed the data flow**, *and* **the EBT verifier
  cleared the energy budget**.
- **v57 Verified-Agent integration.**  New slot `reasoning_fabric`
  (owner = v62, target = `make check-v62`, tier **M**) registered in
  `src/v57/verified_agent.c` and `scripts/v57/verify_agent.sh`.
  `make verify-agent` now reports **11 / 14 PASS, 3 SKIP, 0 FAIL** on M4.
- **Make targets added.**  `standalone-v62`, `standalone-v62-hardened`,
  `test-v62`, `check-v62`, `microbench-v62`, `asan-v62`, `ubsan-v62`,
  `cos`, `check-cos`.  `harden`, `sanitize`, `.PHONY`, `clean`,
  `help` all extended with v62 and `cos`.
- **Docs (`docs/v62/`).**
  - `THE_FABRIC.md` — one-page overview + tier table + non-claims.
  - `ARCHITECTURE.md` — wire map, hardware discipline, microbench, threat-model tie-in.
  - `POSITIONING.md` — vs Coconut / EBT / HRM / NSA / MTP / ARKV / Claude Code / Cursor CLI / Aider / llama.cpp / MLX-LM.
  - `paper_draft.md` — paper-style writeup with arXiv references.
- **README.md / SECURITY.md / CHANGELOG.md / .gitignore /
  docs/DOC_INDEX.md** updated to register v62 and `cos`.
- **Non-claims (do not silently upgrade).**
  - v62 is *kernels, not weights*; production model weights stay in MLX
    / llama.cpp / bitnet.cpp.  v62 is the alien-tier *shape* of the 2026
    frontier in C, ready for a real inference engine to bind to.
  - `cos think` is a *demo* surface, not a chat UI.
  - SME (Apple M4 matrix engine) is *compile-only* today, gated by
    `COS_V62_SME=1`.  Default build is NEON and never SIGILLs.
  - GPU dispatch (Metal / ANE) for ARKV and NSA is *planned* (P-tier).
- **Files added (12).**
  - `src/v62/fabric.h`, `src/v62/fabric.c`, `src/v62/creation_os_v62.c`.
  - `cli/cos.c`.
  - `scripts/v62/microbench.sh`.
  - `docs/v62/THE_FABRIC.md`, `docs/v62/ARCHITECTURE.md`,
    `docs/v62/POSITIONING.md`, `docs/v62/paper_draft.md`.
  - Plus: edits to Makefile, .gitignore, README.md, SECURITY.md,
    THREAT_MODEL.md, docs/DOC_INDEX.md, src/v57/verified_agent.c,
    scripts/v57/verify_agent.sh.

## v61 Σ-Citadel — composed defence-in-depth (DARPA-CHACE menu) (2026-04-17)

- **Driving oivallus.** The 2026 advanced-security menu for AI agents
  is **composed**, not chosen: seL4 microkernel + Wasmtime userland
  sandbox + eBPF LSM + MLS lattice + hardware attestation + Sigstore
  signing + SLSA-3 provenance + reproducible build + distroless
  runtime.  DARPA's CHACE programme studies that composition; no
  open-source AI-agent runtime had shipped all of it as one runnable
  gate.  `v61` is that gate.  It adds two new M-tier primitives
  (BLP + Biba + MLS lattice kernel, and a deterministic 256-bit
  attestation quote) and ties every external layer into one `make
  chace` aggregator that **PASSes present layers and SKIPs missing
  ones honestly** (never silently downgrading).
- **v61 Σ-Citadel kernel** (`src/v61/citadel.{h,c}` + driver
  `src/v61/creation_os_v61.c`).  `cos_v61_lattice_check(subj, obj,
  op)` implements Bell-LaPadula (no-read-up / no-write-down over
  8-bit clearance), Biba (no-read-down / no-write-up over 8-bit
  integrity), and 16-bit MLS compartments in one branchless kernel.
  All three lanes compute unconditionally; op (`READ`/`WRITE`/`EXEC`)
  selects via AND-masks; `EXEC` requires both read and write to
  pass.  `cos_v61_lattice_check_policy` adds an optional
  `min_integrity` quarantine rule.  `cos_v61_lattice_check_batch`
  prefetches 16 lanes ahead for batch throughput.
- **Attestation chain.** `cos_v61_quote256` computes a deterministic
  256-bit digest over
  `(code_page_hash || caps_state_hash || sigma_state_hash ||
  lattice_hash || nonce || reserved_padding)`.  Default: four-lane
  XOR-fold with SplitMix constants — deterministic, equality-
  sensitive, not a MAC.  With `COS_V61_LIBSODIUM=1` the quote is
  BLAKE2b-256 via libsodium `crypto_generichash`.
  `cos_v61_ct_equal256` is constant-time 256-bit equality (OR-fold
  reduction).  `cos_v61_quote256_hex` emits a 64-char hex string
  with no allocation.
- **Composition with v60.** `cos_v61_compose(v60_decision,
  lattice_allowed)` gives a four-valued branchless decision
  `{ALLOW, DENY_V60, DENY_LATTICE, DENY_BOTH}`.  No short-circuit;
  4-way mux via `(CONST * mask)` OR.
- **61-test self-test suite** (`creation_os_v61 --self-test` via
  `make check-v61`): version & policy defaults, BLP no-read-up /
  read-down / no-write-down / write-up / equal, Biba no-read-down /
  read-up / no-write-up / write-down, combined BLP+Biba strict ∧
  equal-is-always-ok, compartment match / missing / empty,
  `EXEC` conservatism, unknown-op denial, policy quarantine,
  batch=scalar equivalence on 64-element random trace, quote
  determinism + per-field sensitivity (`code_page_hash` /
  `caps_state_hash` / `sigma_state_hash` / `lattice_hash` /
  `nonce`), hex round-trip, `ct_equal256` null-safe + self-equal,
  compose all four surfaces + null + tag strings, allocator
  null / aligned / zeroed + `sizeof == 64`, four adversarial
  scenarios (ClawWorm Biba, DDIPE BLP, compartment isolation,
  runtime-patch quote), 2000-case stress invariant.  ASAN + UBSAN
  builds pass all 61 tests.
- **Microbench**: `make microbench-v61` → 6.1 × 10⁸ lattice
  decisions/s on Apple M4 across n ∈ {1024, 16384, 262144} (three-
  decade batch sweep; L1-resident).
- **DARPA-CHACE composition (`make chace`).**  One aggregator that
  runs every layer of the 2026 advanced-security menu locally and
  in CI and reports **PASS / honest SKIP / FAIL** per layer:
  1. `check-v60` σ-Shield (81/81)
  2. `check-v61` Σ-Citadel (61/61)
  3. `attest`  — emit `ATTESTATION.json`, optional cosign sign
  4. seL4 CAmkES contract presence (`sel4/sigma_shield.camkes`)
  5. `wasm-sandbox` — Wasmtime + WASI-SDK (`wasm/example_tool.c`)
  6. `ebpf-policy` — Linux-only LSM BPF example
     (`ebpf/sigma_shield.bpf.c`)
  7. `sandbox-exec` — Darwin sandbox profile (`sandbox/darwin.sb`)
  8. `harden` + `hardening-check` — OpenSSF 2026 flags + ARM64 PAC
  9. `sanitize` — ASAN + UBSAN on v58/v59/v60/v61
  10. `sbom` — CycloneDX-lite 1.5 with SHA-256 per component
  11. `security-scan` — gitleaks + grep-fallback + URL sanity
  12. `reproducible-build` — double-build SHA-256 parity
  13. `sign` — Sigstore cosign sign-blob (OIDC keyless in CI)
  14. `slsa` — SLSA v1.0 predicate to `PROVENANCE.json`
  15. `distroless` — `gcr.io/distroless/cc-debian12:nonroot`
- **Defence-in-depth plumbing (new files).**
  - `sel4/sigma_shield.camkes` + `sel4/README.md` — component
    contract (three endpoints: authorize, lattice_check,
    attest_quote; zero network caps; sibling Wasmtime tool sandbox).
  - `wasm/example_tool.c` — minimal capability-mediated tool.
  - `scripts/v61/wasm_harness.sh` — Wasmtime sandbox harness with
    honest WASI-SDK / wasmtime SKIPs.
  - `ebpf/sigma_shield.bpf.c` — LSM BPF prog that blocks `execve`
    unless σ-Shield has published a PID key to a pinned BPF map.
  - `scripts/v61/ebpf_build.sh` — Linux-only build; SKIP elsewhere.
  - `sandbox/darwin.sb` — `sandbox-exec` profile (deny default; no
    network; no DYLD_ injection; read-only FS; writes limited to
    `.build/`).
  - `sandbox/openbsd_pledge.c` — pledge/unveil wrapper stub.
  - `scripts/v61/sandbox_exec.sh` — runs v61 self-test under the
    Darwin sandbox profile.
  - `nix/v61.nix` — reproducible Nix build recipe.
  - `Dockerfile.distroless` — multi-stage Debian build → distroless
    runtime.
  - `scripts/security/attest.sh` — emit ATTESTATION.json +
    optional cosign sign-blob.
  - `scripts/security/sign.sh` — cosign keyless sign of hardened
    binaries + SBOM.
  - `scripts/security/slsa.sh` — local SLSA v1.0 predicate emitter.
  - `scripts/security/chace.sh` — the CHACE aggregator.
  - `scripts/v61/microbench.sh` — 3-point lattice timing sweep.
- **Makefile.** New targets: `standalone-v61`, `standalone-v61-
  hardened`, `test-v61`, `check-v61`, `microbench-v61`, `asan-v61`,
  `ubsan-v61`, `attest`, `sign`, `slsa`, `wasm-sandbox`,
  `ebpf-policy`, `sandbox-exec`, `distroless`, `nix-build`,
  `sel4-check`, `chace`.  `harden` and `sanitize` now include v61.
  `clean` sweeps `creation_os_v61*`, `.build/wasm`, `.build/ebpf`,
  `ATTESTATION.json`, `PROVENANCE.json`.  `V61_EXTRA_CFLAGS` /
  `V61_EXTRA_LDFLAGS` wire `COS_V61_LIBSODIUM=1` → `-lsodium`.
- **v57 Verified Agent integration.** New slot
  `defense_in_depth_stack` (tier **M**, owner v61, target
  `make check-v61`) in `src/v57/verified_agent.c` and
  `scripts/v57/verify_agent.sh`.  `make verify-agent` now walks
  thirteen slots.
- **CI workflows.** `.github/workflows/chace.yml` runs `make chace`
  on ubuntu-latest + macos-latest on push / PR / weekly cron.
  `.github/workflows/slsa.yml` ships SLSA-3 provenance via
  `slsa-framework/slsa-github-generator@v2.0.0` on release.
- **Docs.** `docs/v61/THE_CITADEL.md`, `docs/v61/ARCHITECTURE.md`,
  `docs/v61/POSITIONING.md`, `docs/v61/paper_draft.md`; updates to
  `SECURITY.md`, `THREAT_MODEL.md`, `README.md`, `docs/DOC_INDEX.md`.
- **Non-claims.** v61 does **not** replace seL4 / Wasmtime / gVisor;
  it composes via contracts.  TPM 2.0 / Apple Secure Enclave
  binding is P-tier (v61 accepts external hash input).  CHERI is
  documented as a target; no M4 hardware path.  `make slsa` locally
  emits a SLSA-v1.0 predicate *stub*; real SLSA-3 attestation with
  Rekor inclusion ships via the CI workflow.
- **Files added** (18 new): `src/v61/{citadel.h,citadel.c,
  creation_os_v61.c}`, `docs/v61/{THE_CITADEL,ARCHITECTURE,POSITIONING,
  paper_draft}.md`, `sel4/{sigma_shield.camkes,README.md}`,
  `wasm/example_tool.c`, `ebpf/sigma_shield.bpf.c`,
  `sandbox/{darwin.sb,openbsd_pledge.c}`, `nix/v61.nix`,
  `Dockerfile.distroless`, `scripts/v61/{microbench,wasm_harness,
  ebpf_build,sandbox_exec}.sh`, `scripts/security/{attest,sign,slsa,
  chace}.sh`, `.github/workflows/{chace,slsa}.yml`.

## v60 σ-Shield + security multi-peak — runtime security kernel & next-level hardening (2026-04-16)

- **Driving oivallus.** Q2 2026 LLM-agent attack literature converges
  on a single failure mode: **every successful attack succeeds because
  the payload looks legitimate** — DDIPE (arXiv:2604.03081, 11–33 %
  bypass against strong defenses), ClawWorm (arXiv:2603.15727, 64.5 %
  self-propagating against OpenClaw), Malicious Intermediary
  (arXiv:2604.08407, 17 / 428 API routers touch credentials). Every
  defense today (signature, capability allowlist, network MITM like
  ShieldNet 2604.04426) operates on a **scalar** confidence signal,
  which by construction cannot separate "ambiguous because evidence
  is missing" (ε-dominant, reducible) from "ambiguous because
  ambiguity is inherent" (α-dominant, irreducible). v60 uses the v34
  σ = (ε, α) decomposition as the first-ever intent gate in a
  capability kernel.
- **v60 σ-Shield runtime security kernel**
  (`src/v60/sigma_shield.{h,c}` + driver `src/v60/creation_os_v60.c`).
  Five-valued branchless authorise:
  `{ALLOW, DENY_CAP, DENY_INTENT, DENY_TOCTOU, DENY_INTEGRITY}`.
  Every call runs **five orthogonal checks unconditionally**:
  (1) code-page integrity via constant-time `ct_equal64` on a
  caller-provided `code_page_hash` vs `baseline_hash`; (2) sticky-deny
  overlap — `CAP_DLSYM` / `CAP_MMAP_EXEC` / `CAP_SELF_MODIFY` cannot
  be granted even on a full-cap holder; (3) capability subset check
  `required & ~(holder | always_caps) == 0`; (4) TOCTOU equality
  `arg_hash_at_entry ↔ arg_hash_at_use` via the same constant-time
  compare; (5) σ-intent gate — fires iff
  `(ε + α) ≥ σ_high ∧ α/(ε+α) ≥ α_dom`. Priority cascade via `& | ~`
  mask AND-NOT (integrity > sticky > cap > TOCTOU > intent); no `if`
  on the hot path. `reason_bits` is multi-cause honest — records all
  failing conditions even when priority has picked the winner.
  `cos_v60_hash_fold` is a deterministic 64-bit XOR-fold used only
  for equality, with a constant operation count per byte and no
  branch on data; `cos_v60_ct_equal64` is a branchless reduction.
  Batch `cos_v60_authorize_batch` prefetches 16 lanes ahead and is
  tested against the scalar path over 32 requests.
- **81-test self-test suite** (`creation_os_v60 --self-test` via
  `make check-v60`): version & defaults (11), hash + constant-time
  equality incl. 2 048 random (11), decision surfaces (15), priority
  cascade (10), always-caps bootstrap (1), σ-intent edge cases (3),
  batch = scalar + null-safe (2), tag strings + aligned allocator
  incl. `sizeof(request)==64` (10), adversarial scenarios (DDIPE /
  ClawWorm / intermediary / confused-deputy DLSYM / runtime-patch — 5),
  composition with v58 / v59 (2), determinism + summary fields (3),
  stress sweep over 2 000 random requests (1). **All 81 pass.**
- **Registered in v57 Verified Agent** as a new slot
  `runtime_security_kernel` owner `v60`, best tier **M**, make target
  `make check-v60`, in both `src/v57/verified_agent.c` and
  `scripts/v57/verify_agent.sh`. `make verify-agent` now reports 10
  composition slots.
- **Microbench harness** (`scripts/v60/microbench.sh` +
  `./creation_os_v60 --microbench`): 3-point sweep N ∈ {128, 1024,
  8192} with deterministic LCG-seeded requests; decisions / s
  ≥ 6 × 10⁷ on M-class silicon (sub-microsecond per authorise).
- **Hardening matrix in Makefile.** New targets:
  - `make harden` — rebuilds v57 / v58 / v59 / v60 with OpenSSF 2026
    recommendation (`-D_FORTIFY_SOURCE=3`, `-fstack-protector-strong`,
    `-fstack-clash-protection`, `-fstrict-flex-arrays=3`, `-Wformat=2`,
    `-Werror=format-security`, `-fPIE`) plus ARM64
    `-mbranch-protection=standard` (PAC / BTI) and PIE link-time.
  - `make sanitize` → `make asan-v58 asan-v59 asan-v60 ubsan-v60`,
    each building and running its own self-test under the sanitizer.
  - `make hardening-check` — runs `scripts/security/hardening_check.sh`
    which verifies PIE (Mach-O MH_PIE or ELF ET_DYN), stack-canary
    references, fortify-source references, and branch-protection
    metadata on `creation_os_v60_hardened`. Portable across macOS and
    Linux; skips gracefully if `otool` / `readelf` / `nm` missing.
  - `make sbom` → `scripts/security/sbom.sh` → `SBOM.json`
    (CycloneDX-lite 1.5 JSON). Every `src/v*/` becomes a distinct
    component with its own SHA-256 digest; top-level component
    carries the repo-level commit hash or directory-tree digest.
  - `make security-scan` → `scripts/security/scan.sh`: layered secret
    scanner. (1) gitleaks with `.gitleaks.toml` config if installed,
    (2) always-run grep-only fallback over ten high-value patterns
    (AWS, GCP, OpenAI `sk-…`, Slack, GitHub `ghp_…`, `github_pat_…`,
    `gho_…`, RSA / OpenSSH / EC / PGP private keys, JWT triple,
    adaptive evasion triggers), (3) hardcoded-URL sanity on `src/`
    and `docs/`. Allowlists `.env.example`, `docs/`, and the security
    scripts themselves. Never silent PASS — skips gitleaks honestly
    when not installed and still runs the fallback.
  - `make reproducible-build` → `scripts/security/reproducible_build.sh`:
    builds `creation_os_v60` twice with `SOURCE_DATE_EPOCH` pinned,
    compares SHA-256 digests, fails on drift.
- **Local-dev security stack.**
  - `SECURITY.md` — supported-versions table, reporting process,
    tier semantics, active guarantees (M-tier minimum), explicit
    non-guarantees, local-dev quick-start.
  - `THREAT_MODEL.md` — 7 assets, 7 adversary tiers (T0 curious user
    through T7 ring-0 out-of-scope), STRIDE × σ-Shield matrix,
    arXiv-per-row mapping (DDIPE, Malicious Intermediary, ClawWorm,
    ShieldNet), 7 invariants auto-checked by `make check-v60`.
  - `.pre-commit-config.yaml` — trailing-whitespace, EOF fixer,
    large-file guard, merge-conflict, YAML / JSON / TOML syntax,
    detect-private-key, forbid-submodules, LF EOL, **gitleaks
    v8.18.4**, plus local hooks: reject `.env` (accepting only
    `.env.example`), pre-push runs layered `security-scan` and
    sanity-builds `creation_os_v60`.
  - `.gitleaks.toml` — extends defaults, allowlists docs / fixtures /
    the security scripts, stopword guard (`example`, `placeholder`,
    `TEST`, `FIXTURE`), explicit regex exemptions for the canonical
    mix-hint constant and placeholder example tokens.
  - `.editorconfig` — LF, UTF-8, trim-trailing-whitespace, 4-space C,
    tab Makefile, 2-space YAML / JSON / TOML, markdown excluded from
    trimming.
  - `.env.example` — template with zero real secrets; only
    Creation-OS-specific tunables (`CC`, `SOURCE_DATE_EPOCH`, v60
    σ-intent thresholds, v59 budget caps, v57 report path). Bottom
    comment: **"If a key is ever needed, read it from a secret
    manager at runtime — never from `.env`."**
  - `.gitignore` — adds `creation_os_v55 … v60`, every hardened
    / ASAN / UBSAN binary and its `.dSYM/`, `SBOM.json`, and a `.env`
    / `.env.*` guard that permits only `.env.example`.
- **`.github/workflows/security.yml` (new).** Six jobs in parallel:
  gitleaks full history, harden + hardening-check (macOS + Linux),
  sanitize (ASAN + UBSAN, macOS + Linux), security-scan (gitleaks
  installed + grep fallback), SBOM (emits + uploads `SBOM.json`
  artefact), reproducible-build (macOS). Triggers: push + PR on
  main, weekly cron Mon 06:00 UTC, `workflow_dispatch`.
- **Documentation.** New files under `docs/v60/`:
  `THE_SIGMA_SHIELD.md` (overview, composition, run), `ARCHITECTURE.md`
  (wire map, per-surface tiering, hardware discipline, measured
  performance), `POSITIONING.md` (comparison matrix vs seL4 / WASM /
  IronClaw / ShieldNet / Claude Code; explicit non-claims),
  `paper_draft.md` (abstract, motivation, policy, kernel, 81-test
  breakdown, microbench, positioning, limitations, artefacts).
- **Non-claims.** v60 is **not** a sandbox (compose with seL4 / WASM /
  Fuchsia); **not** a zero-day detector (gates on caller-provided σ +
  caller-provided required-caps + caller-provided arg-hash); **no**
  TPM / Secure Enclave integration yet (P-tier roadmap); **no** Frama-C
  proof yet — all v60 claims are M-tier runtime checks; **no** SLSA L3
  provenance yet (P-tier). A compromised σ producer bypasses the
  intent gate; fuse with v54 σ-proconductor or v56 VPRM for
  independent σ lines.
- **Files.** `src/v60/sigma_shield.{h,c}`, `src/v60/creation_os_v60.c`,
  `scripts/v60/microbench.sh`, `scripts/security/{scan,hardening_check,reproducible_build,sbom}.sh`,
  `docs/v60/{THE_SIGMA_SHIELD,ARCHITECTURE,POSITIONING,paper_draft}.md`,
  `.github/workflows/security.yml`, `.pre-commit-config.yaml`,
  `.gitleaks.toml`, `.editorconfig`, `.env.example`, `SECURITY.md`,
  `THREAT_MODEL.md`, updates to `Makefile`, `.gitignore`,
  `src/v57/verified_agent.c`, `scripts/v57/verify_agent.sh`,
  `README.md`, `docs/DOC_INDEX.md`.

## v59 σ-Budget — σ-decomposed adaptive test-time compute budget controller (2026-04-16)

- **Driving oivallus.** Q2 2026 adaptive-reasoning-budget field (TAB
  arXiv:2604.05164, CoDE-Stop arXiv:2604.04930, LYNX arXiv:2512.05325,
  DTSR arXiv:2604.06787, DiffAdapt, Coda arXiv:2603.08659, AdaCtrl
  arXiv:2505.18822, Risk-Control Budget Forcing) converges on one open
  problem — **how much compute to spend on the next reasoning step** —
  and answers with a **scalar** signal (entropy, confidence dynamic,
  probe scalar, RL policy, reflection-tag counter). **Nobody** uses the
  v34 σ = (ε, α) decomposition, even though the rest of Creation OS
  already speaks that dialect (v55 σ₃, v56 σ-Constitutional, v57
  Verified Agent, v58 σ-Cache). **v59 closes that gap** at the
  test-time compute budget layer.
- **Policy.** Per-step readiness
  `r(t) = β·stability(t) + γ·reflection(t) − α_ε·ε(t) − δ·α(t)`,
  σ_total = ε + α, α_frac = α / (ε + α). Four-valued decision:
  **CONTINUE / EARLY_EXIT / EXPAND / ABSTAIN**. Priority cascade:
  at-cap > abstain > expand > exit-on-ready > continue. ABSTAIN fires
  only when σ is high **and** α dominates — the decision no
  scalar-signal method can produce faithfully.
- **Kernel (`src/v59/sigma_budget.c`).** Branchless hot path: 0/1 lane
  masks from integer compares combined with `& | ~` in an AND-NOT
  priority cascade; final tag is a four-way branchless mux. Scratch
  via `aligned_alloc(64, ⌈n · 20/64⌉·64)`. Prefetch 16 lanes ahead in
  every history-walking loop. Explicit NEON 4-accumulator SoA
  readiness reduction (`cos_v59_score_soa_neon`) materialises the
  `.cursorrules` item 5 pattern (four lanes, three `vfmaq_f32` stages)
  and is tested against a scalar reference within 1e-4.
- **Self-test (`creation_os_v59 --self-test`).** **69 / 69**
  deterministic assertions covering: version / defaults / null safety,
  score monotonicity (ε↓readiness, α↓readiness), stability reward,
  reflection lift, batch = scalar, NEON SoA = scalar, translation
  invariance in `step_idx`, all four decision tags reachable, cap
  beats abstain, abstain beats expand, abstain beats exit-on-ready,
  below-min forces continue, determinism, idempotency on stable
  appended history, summary counts sum to n, summary totals = Σ
  per-step, summary final = online decision, random stress invariants,
  aligned-alloc zeroing + 64-byte alignment, end-to-end scenarios
  (easy → EARLY_EXIT, ambiguous → ABSTAIN, hard-tractable → EXPAND,
  mixed input → multiple tags).
- **Microbench (`make microbench-v59`).** Three-point sweep N = 64 /
  512 / 4096. Measured on an M-class chip: **1.1 – 1.5 × 10⁸
  decisions / s** — at 100 M dec/s the budget controller is effectively
  free even at 10⁵-token reasoning traces.
- **Composition.** v59 registers as the `adaptive_compute_budget`
  slot in the v57 Verified Agent (`src/v57/verified_agent.c`).
  `make verify-agent` now reports **8 PASS, 3 SKIP, 0 FAIL** across
  eleven composition slots (previously 7 / 3 / 0).
- **Non-claims.** v59 does **not** claim better end-to-end
  tokens-per-correct than TAB / CoDE-Stop / LYNX / DTSR on GSM8K /
  MATH / AIME. End-to-end accuracy-under-budget is a **P** tier
  measurement; v59 ships **policy + kernel + correctness proof**
  (**M** tier, 69 deterministic self-tests), not a leaderboard row.
  σ is v34's signal; v59's novelty is its decomposition as the
  adaptive-compute-budget decision surface. No Frama-C proof yet.
- **Files.**
  - `src/v59/sigma_budget.h` — public API + tier semantics + non-claims.
  - `src/v59/sigma_budget.c` — scalar / NEON scoring + branchless
    decision + offline summary.
  - `src/v59/creation_os_v59.c` — driver with 69-test suite +
    architecture / positioning banners + microbench.
  - `scripts/v59/microbench.sh` — deterministic three-point sweep.
  - `docs/v59/THE_SIGMA_BUDGET.md` — headline doc.
  - `docs/v59/ARCHITECTURE.md` — wire map + tier table.
  - `docs/v59/POSITIONING.md` — vs TAB / CoDE-Stop / LYNX / DTSR /
    DiffAdapt / Coda / AdaCtrl / Risk-Control BF.
  - `docs/v59/paper_draft.md` — full write-up.
  - `Makefile` — `standalone-v59`, `test-v59`, `check-v59`,
    `microbench-v59` targets; clean target extended.
  - `.gitignore` — ignores `creation_os_v59` + asan variants.
  - `src/v57/verified_agent.c` + `scripts/v57/verify_agent.sh` — add
    `adaptive_compute_budget` slot.
  - `README.md` / `CHANGELOG.md` / `docs/DOC_INDEX.md` — cross-linked.

## v58 σ-Cache — σ-decomposed KV-cache eviction with a branchless NEON kernel (2026-04-16)

- **Driving oivallus.** Q2 2026 KV-cache eviction literature
  (StreamingLLM, H2O, SnapKV, PyramidKV, KIVI, KVQuant, SAGE-KV,
  G-KV, EntropyCache, Attention-Gate) uses **one scalar** signal
  per token: position, attention mass, quantisation residue, or
  entropy. **Nobody** uses a **decomposed** σ = (ε, α) signal —
  even though the rest of the Creation OS stack already speaks
  that dialect (v34 Dirichlet split, v55 σ₃ triangulation, v56
  σ-governed IP-TTT, v57 σ-tiered registry). **v58 closes that
  gap** at the KV-cache layer.
- **Policy.** Per-token retention score
  `s(t) = α_ε·ε(t) + β·attn(t) + γ·recency(t) − δ·α(t) + sink_lift(t)`,
  budgeted by `τ_keep = K-th largest non-sink score` where `K =
  max(capacity − sinks_present, 0)`. Four-valued decision: **FULL
  / INT8 / INT4 / EVICTED** by per-token precision thresholds
  `τ_full`, `τ_int8` layered under the budget — a token that
  clears `τ_int8` but misses `τ_keep` is still EVICTED.
- **Kernel (`src/v58/sigma_cache.c`).** Branchless hot path: 0/1
  lane masks from float compares combined with `| & ~`; a
  four-way branchless mux assembles the tag byte. Compaction
  writes every index unconditionally and advances by a 0/1 kept
  flag. Scratch via `aligned_alloc(64, ⌈n·4/64⌉·64)`. Prefetch 16
  lanes ahead. An explicit NEON 4-accumulator SoA score reduction
  (`cos_v58_score_soa_neon`) materialises the `.cursorrules`
  item 5 pattern (four lanes, three `vfmaq_f32` stages) and is
  tested against a scalar reference within 1e-4.
- **Self-test (`creation_os_v58 --self-test`).** **68 / 68**
  deterministic assertions covering: version / default-policy
  invariants, score monotonicity in each axis (ε↑, attn↑, α↓),
  branchless recency window equivalence, translation invariance,
  attention-scale order preservation, sink lift dominance, scalar
  ≡ batched equivalence, NEON-SoA ≡ scalar-SoA within tolerance,
  null safety across `decide` and `compact`, n=0 edge case, sinks
  always kept, `kept + evicted == n`, `kept_total ≤ capacity +
  sinks`, zero-capacity forces all-evict, monotone capacity,
  determinism under fixed policy + seed, threshold populated in
  summary, two 10-iteration random stress tests, and compaction
  equivalence.
- **Microbench (`make microbench-v58`).** 3-point sweep
  (N = 1024 / 4096 / 16384); deterministic across runs; prints
  ms/iter, decisions/s, kept histogram, and the selected threshold
  so reviewers can audit the policy externally.
- **Composition into the Verified Agent (v57).** v58 is
  registered as a new `kv_cache_eviction` slot in
  `src/v57/verified_agent.c` (owner: v58, tier: **M**, target:
  `make check-v58`). `make verify-agent` now reports **10** slots
  (7 PASS + 3 SKIP + 0 FAIL on a baseline host) and breaks
  aggregate status if `check-v58` fails.
- **Non-claims.** v58 ships the **policy + kernel + proof of
  correctness**. It does **not** claim lower perplexity than H2O
  / SnapKV / KIVI / EntropyCache on any standard benchmark — that
  requires end-to-end integration with a transformer runtime and
  belongs to a future **P-tier** measurement. Formal memory-safety
  proofs belong to the v47 slot, not v58. σ-decomposition
  novelty belongs to v34; v58 claims novelty for **σ applied as a
  KV-cache eviction policy**.
- **Files.** `src/v58/sigma_cache.{h,c}`,
  `src/v58/creation_os_v58.c`, `scripts/v58/microbench.sh`,
  `docs/v58/{THE_SIGMA_CACHE,ARCHITECTURE,POSITIONING,paper_draft}.md`.
- **Cross-doc.** `README.md` σ-lab table v31–v57 → v31–v58 with a
  new row; `docs/DOC_INDEX.md` adds the `docs/v58/` entries;
  `scripts/v57/verify_agent.sh` adds `kv_cache_eviction` to its
  `SLOTS` array; `Makefile` gets `standalone-v58`, `test-v58`,
  `check-v58`, `microbench-v58` targets; `.gitignore` adds
  `creation_os_v58` and `creation_os_v58_asan`.

## v57 The Verified Agent — convergence of v33–v56 (2026-04-16)

- **No new σ math.** v57 is the *convergence artifact* of the
  Creation OS family. Every piece of math it relies on shipped in
  v33–v56. v57's contribution is the explicit **invariant +
  composition registry** that names which Creation OS subsystem
  owns which agent-runtime surface, and tags every claim with one
  of four explicit tiers (M / F / I / P).
- **Driving oivallus.** The Q2 2026 open-source agent-framework
  field offers ad-hoc sandboxing — Docker containers, WASM
  sandboxes, capability allowlists — and rests on prompt-engineered
  reasoning loops. Every framework's user-facing claim reduces to
  *"trust our architecture"*. v57's user-facing claim is the
  opposite: *"run `make verify-agent`"*.
- **Tier semantics (no blending).**
  - **M** — runtime-checked deterministic self-test (`make` target
    returns 0 iff the check passes)
  - **F** — formally proven proof artifact (Frama-C / WP / sby /
    TLA+ / Coq)
  - **I** — interpreted / documented (no mechanical check yet)
  - **P** — planned (next concrete step explicit)
- **Invariant registry (`src/v57/invariants.{c,h}`).** Five
  invariants chosen to span the surface that ad-hoc agent
  frameworks typically *trust the architecture* on:
  `sigma_gated_actions`, `bounded_side_effects`,
  `no_unbounded_self_modification`, `deterministic_verifier_floor`,
  `ensemble_disagreement_abstains`. Each invariant carries up to
  four checks (runtime, formal, documented, planned) and a
  best-tier query.
- **Composition registry (`src/v57/verified_agent.{c,h}`).** Nine
  composition slots: `execution_sandbox` (v48, M),
  `sigma_kernel_surface` (v47, F), `harness_loop` (v53, M),
  `multi_llm_routing` (v54, M), `speculative_decode` (v55, M),
  `constitutional_self_modification` (v56, M),
  `do178c_assurance_pack` (v49, I), `red_team_suite` (v48 + v49,
  I), `convergence_self_test` (v57, M). Each slot has exactly one
  owning `make` target.
- **Driver (`src/v57/creation_os_v57.c`).** Modes: `--self-test`
  (49/49 deterministic registry tests — count, uniqueness, tier
  monotonicity, find-by-id, histogram sums, slot completeness),
  `--architecture` (composition + invariant table with tier tags),
  `--positioning` (vs ad-hoc agent sandboxes, with explicit
  non-claims), `--verify-status` (static tier histogram).
  No socket, no allocation on a hot path; registry is `static
  const`.
- **Live aggregate (`scripts/v57/verify_agent.sh`).** Walks the
  composition slots, dispatches each owning `make` target, reports
  per-slot **PASS / SKIP / FAIL**. **SKIP** is emitted when the
  target ran without error but matched `SKIP\b` in its output
  (existing Creation OS pattern when external tools — Frama-C, sby,
  garak, DeepTeam, pytest extras — are not installed). Never
  silently downgrades a slot. Flags: `--strict` (treat SKIP as
  FAIL for CI hosts with full tooling), `--json PATH` (machine-
  readable report), `--quiet` (final summary line only).
- **Makefile.** New targets: `standalone-v57`, `test-v57`,
  `check-v57`, **`verify-agent`** (live aggregate). All marked
  *not* merge-gate. v57 binary added to `.gitignore`.
- **Documentation (`docs/v57/`).**
  - `THE_VERIFIED_AGENT.md` — one-page articulation of the
    artifact, with explicit non-claims list (no FAA / EASA
    certification, no zero-CVE claim, no frontier-accuracy claim,
    no specific-product comparison).
  - `ARCHITECTURE.md` — composition wire map, tier semantics,
    per-slot responsibilities.
  - `POSITIONING.md` — vs ad-hoc OSS agent frameworks / hardened
    OSS frameworks / enterprise SaaS; the only column whose
    user-facing answer is "verify yourself" is v57.
  - `paper_draft.md` — position paper.
- **Local verification (M4, this build).** `make check-v57`
  → 49/49 PASS. `make verify-agent` (no external proof tooling
  installed) → 6 PASS, 3 SKIP, 0 FAIL — the SKIPs are honest
  reports for `verify-c` (Frama-C absent), `certify` (DO-178C
  pipeline tooling absent), `red-team` (Garak / DeepTeam absent).
  Never a silent PASS.
- **Cross-doc.** README, `docs/SIGMA_FULL_STACK.md`,
  `docs/DOC_INDEX.md`, `docs/WHAT_IS_REAL.md`, `CONTRIBUTING.md`,
  `creation.md` updated with v57 entries; version ranges bumped
  from `v31–v56` / `v33–v56` (and earlier `v33–v51`, `v33–v55`)
  to `v31–v57` / `v33–v57`.
- **What v57 deliberately does not claim.** Not FAA / EASA
  certification (v49 artifacts are tier I, not a certificate).
  Not zero CVEs (small surface, but smallness is not absence).
  Not frontier-model accuracy (model choice lives in the routing
  slot). Not a replacement for any specific commercial product
  (the `--positioning` table uses category names, with CVE / ARR
  figures cited from the driving oivallus prompt as user-supplied
  reporting).
- **Tier tags.** v57 convergence registry: **M** runtime-checked.
  v57 paper draft: **I**. v57 live aggregate (`make verify-agent`):
  **M** orchestration with honest **SKIP** reporting for missing
  tooling.

## v56 σ-Constitutional scaffold (2026-04-16)

- **Four Q1-2026 insights, one C11 invariant.** Rule-based process
  reward models (VPRM, arXiv:2601.17223), in-place test-time
  training (IP-TTT, arXiv:2604.06169, April 2026), grokking as
  phase transition under Singular Learning Theory (arXiv:2603.01192
  + 2603.13331, March 2026), and the 2026 reverse-engineered Apple
  Neural Engine `matmul → 1×1 conv` contract, composed into a
  single policy: **any inference-time self-modification must
  strictly lower σ**. Inference-side dual of v40's `K_eff = (1−σ)·K`.
- **Verifier module (`src/v56/verifier.{c,h}`)** — rule-based PRM.
  Seven deterministic rules (`NONEMPTY`, `MIN_LENGTH`, `MAX_LENGTH`,
  `SCORE_BOUNDED`, `SCORE_MONOTONE`, `NO_EXACT_REPEAT`,
  `CHAR_ENTROPY`); aggregate `sigma_verifier = 1 − precision`
  (the VPRM paper's priority metric, in canonical σ frame).
  Precision-first F1; branchless per-step evaluation.
- **σ-gated IP-TTT policy (`src/v56/ipttt.{c,h}`)** — slow / recent
  σ EWMA bands, decide-reason-tagged budget controller
  (`DENY_BUDGET`, `DENY_COOLDOWN`, `DENY_NO_DRIFT`, `DENY_INVALID`,
  `ALLOW`). Commit iff post-update σ does not regress, else
  rollback with `alpha_current *= rollback_shrink`. Fresh
  controllers begin in warm-up (no insta-update on boot).
- **Grokking commutator σ-channel (`src/v56/grokking.{c,h}`)** —
  normalized defect `‖g_new − g_prev‖² / (‖g_new‖² + ‖g_prev‖²)`
  bounded in `[0, 1]`, NEON 4-accumulator reduction with prefetch
  on both inputs, scalar tail fixup for arbitrary dims. Baseline
  EWMA + warm-up-armed spike detector; phase_transition_detected
  flips when `latest > baseline × spike_multiplier`. First public
  runtime exposure of the SLT trajectory signal as an inference-time
  σ-channel, orthogonal to v34 / v55 output-distribution σ.
- **ANE dispatch layout helper (`src/v56/ane_layout.{c,h}`)** —
  pure integer math for the undocumented
  `A[M,K] @ B[K,N] → conv(input=[1,K,1,N], weight=[M,K,1,1])`
  rewrite, with the ANE-enforced spatial ≥ 16 ∧ multiple-of-16
  validator, padding plan (`pad_h`, `pad_w`, `pad_bytes_fp32`),
  and a short, loggable `reason` string. **No** Core ML, **no**
  `_ANEClient`, **no** MLModel compilation — just the prerequisite
  shape contract any future binding will need.
- **Hardware path (M4-first).** NEON `vld1q_f32` / `vfmaq_f32`
  reductions on the defect kernel with `__builtin_prefetch` on
  `g_new` and `g_prev` both, 16-wide unrolled accumulator quartet,
  scalar tail fixup; scalar fallback bit-identical on non-ARM;
  branchless policy arithmetic everywhere else. No allocations on
  the hot path; compile-time `V56_GROK_HAS_NEON` switch gates the
  scalar-only function to avoid an unused-function warning.
- **56/56 deterministic self-test** (`make check-v56`): verifier
  precision / F1 / σ (5), IP-TTT cooldown / drift / budget /
  rollback / commit (6), grokking identical / opposite / orthogonal
  / NEON-tail-fixup-vs-scalar / spike detection (5), ANE round-up /
  small-matmul fail / aligned-width still fail on H=1 / NCHW shapes
  / reason string (5), full-loop integration test tying all four
  modules (6). No network, no engine, no tensors.
- **Docs:** `docs/v56/ARCHITECTURE.md` (wire map + per-module
  contracts + composition tree with v34 / v40 / v45 / v47 / v51 /
  v53 / v54 / v55), `docs/v56/POSITIONING.md` (vs VPRM / ThinkPRM /
  IP-TTT / TTSR / VDS-TTT / SLT grokking / ANE RE / SME2, with
  tier placement), `docs/v56/paper_draft.md` (σ-Constitutional
  position paper — policy certification, not benchmark).
- **Tier tags.** `verifier`, `ipttt`, `grokking`, `ane_layout`: M
  inside the scaffold boundary (they do what their tests say).
  End-to-end TTT on a real transformer: P (planned, needs
  bitnet.cpp / MLX hook). ANE dispatch via Core ML / private API:
  P (planned, needs Apple-only CoreML target). See
  `docs/WHAT_IS_REAL.md`.

## v55 σ₃-speculative scaffold (2026-04-16)

- **Three 2026 insights, one C11 policy layer.** σ₃ decomposition
  (Taparia et al., arXiv:2603.24967, March 2026), EARS adaptive
  acceptance (Sun et al., arXiv:2512.13194, December 2025), and EASD
  entropy-aware quality gate (Su et al., arXiv:2512.23765, December
  2025) composed into a branchless acceptance surface. EARS fixes
  random rejection (asymmetric uncertainty); EASD fixes random
  acceptance (symmetric uncertainty + top-K overlap); σ₃ supplies
  the signal that tells the two apart. See `docs/v55/POSITIONING.md`.
- **Honest network story.** `src/v55/` opens no sockets, loads no
  weights, speaks no API (creation.md invariant #3). The scaffold
  is the policy underneath a future bitnet.cpp / vLLM integration.
- **σ₃ module:** `src/v55/sigma3.{c,h}` — `v55_sigma3_t` (σ_input,
  σ_knowledge, σ_decoding, σ_total, raw h_bits, top_k_used).
  Deterministic proxies on caller-supplied softmax:
  - σ_input = normalized Gini dispersion over top-K probabilities,
  - σ_knowledge = 1 − max(P) (same signal EARS exploits),
  - σ_decoding = H / log₂(N), clamped [0, 1].
- **EARS acceptance:** `src/v55/ears.{c,h}` — `v55_ears_accept(...)`
  returns 0/1 via `r < min(max_threshold, P_t/P_d + α · σ_knowledge)`.
  α = 0 is bit-identical to vanilla spec-decode. Batch helper writes
  an int mask; no allocations, no branches on the data path.
- **EASD quality gate:** `src/v55/easd.{c,h}` — `v55_easd_decide(...)`
  sets `reject` iff all three hold: H_t ≥ τ, H_d ≥ τ, |topK_t ∩ topK_d|/K ≥ ρ.
  Composes *on top of* EARS; they solve opposite failure modes.
- **Hardware path (M4-first).** NEON 4-accumulator entropy loop
  (`vld1q_f32` loads, `vmlaq_f32` accumulators, `__builtin_prefetch`
  ahead of each 16-lane iteration) with a branchless fast log₂
  approximation (IEEE-754 exponent extraction + 2nd-order minimax
  polynomial, ±0.01 bits). Scalar fallback bit-identical on non-ARM
  for CI reproducibility. Scratch is caller-owned; no `malloc` on
  the hot path.
- **29/29 deterministic self-test** (`make check-v55`): σ₃ uniform
  vs one-hot vs two-peak; σ_total bound; from_logits peaked;
  EARS α = 0 matches vanilla; EARS relaxes under knowledge gap;
  EARS clamp; batch matches scalar; EASD confident pass-through;
  EASD both-uncertain-with-overlap reject; EASD both-uncertain-but-
  disagreeing pass-through (uncertainty is informative); EASD
  asymmetric pass-through (EARS regime).
- **Makefile / build:** `V55_SRCS` + `standalone-v55`, `test-v55`,
  `check-v55`; `-Isrc/v55` compile flag; added to `.PHONY`, `help`,
  and `.gitignore`. Not in `merge-gate`; strictly opt-in.
- **Docs:** `docs/v55/ARCHITECTURE.md`, `docs/v55/POSITIONING.md`,
  `docs/v55/paper_draft.md`. Cross-doc updates: scope v31–v55 in
  README / SIGMA_FULL_STACK / CONTRIBUTING / v53 paper; tier row in
  WHAT_IS_REAL; index row in DOC_INDEX; last-landed in creation.md.
- **Tier tags:** scaffold (M) for `make check-v55`, interpretive
  (I) for the paper draft, product (P) for a live bitnet.cpp /
  vLLM integration reporting real throughput gains — deferred.

## v54 σ-proconductor scaffold (2026-04-16)

- **Structural claim, not a live ensemble.** σ is the missing
  routing signal in the 2024–2026 multi-LLM literature
  (MoA / RouteLLM / MoMA / FrugalGPT / Bayesian Orchestration /
  MoErging). v54 encodes σ as the routing + aggregation + abstention
  signal over frontier subagents. See `docs/v54/POSITIONING.md`.
- **Honest network story.** `src/v54/` makes **no network calls**
  (creation.md invariant #3). The scaffold is the orchestration
  policy; callers supply per-agent responses + σ. The self-test is
  fully deterministic, offline, and embeddings-free.
- **Registry + defaults:** `src/v54/proconductor.{c,h}` —
  `v54_subagent_t`, `v54_proconductor_t`, five hand-tuned reference
  profiles (`claude`, `gpt`, `gemini`, `deepseek`, `local_bitnet`).
- **Dispatch policy:** `src/v54/dispatch.{c,h}` — keyword-heuristic
  classifier; deterministic top-K selector (stakes-scaled K ∈ {1, 2, 4}
  with an easy-query shortcut when σ_primary < 0.10); σ-weighted
  aggregator with five outcomes:
  `V54_AGG_CONSENSUS`, `V54_AGG_SIGMA_WINNER`,
  `V54_AGG_ABSTAIN_SIGMA`, `V54_AGG_ABSTAIN_DISAGREE`,
  `V54_AGG_EMPTY`.
- **Disagreement analyzer:** `src/v54/disagreement.{c,h}` —
  lexical-Jaccard similarity over tokens with outlier detection.
  Pluggable; a production runtime swaps the kernel for an embedding
  backend without touching the rest of the pipeline.
- **Profile learner:** `src/v54/learn_profiles.{c,h}` — EWMA
  (α = 0.05) on per-domain σ + observed-accuracy;
  `v54_learn_from_aggregation()` attributes ground truth to the winner
  only (non-winners receive "unknown" so no false reward/punishment).
- **Paper draft:** `docs/v54/paper_draft.md` — "σ-Proconductor: σ as
  the Missing Routing Signal for Multi-LLM Ensembles" (I-tier
  position paper with explicit follow-up router benchmark scope).
- **Architecture + positioning:** `docs/v54/ARCHITECTURE.md`,
  `docs/v54/POSITIONING.md` (side-by-side vs MoA / RouteLLM / MoMA /
  FrugalGPT / Bayesian Orchestration / MoErging).
- **Makefile:** `make check-v54` (14/14 self-test); `make standalone-v54`;
  help + `.PHONY` updated. **Not** part of `merge-gate`.

## v53 σ-governed harness scaffold (2026-04-16)

- **Structural critique, not clone.** Takes Claude Code's public harness
  primitives (`nO` / `h2A` / TAOR / `wU2` / sub-agents / `claude.md` /
  Plan Mode / fork agents / 4-layer compression) as given, and encodes
  σ-governance over the top. See `docs/v53/POSITIONING.md`.
- **σ-TAOR loop:** `src/v53/harness/loop.{c,h}` —
  `while (tool_call && σ < threshold && σ_ewma < drift_cap && making_progress)`.
  Five distinct abstain outcomes (`ABSTAIN_SIGMA`, `ABSTAIN_DRIFT`,
  `ABSTAIN_NO_PROG`, `ABSTAIN_NO_TOOLS`, `BUDGET_EXHAUSTED`) + `COMPLETE`
  + reserved `SAFETY_BLOCK`. Consumes v51 `cognitive_step` for per-iter σ.
- **σ-triggered sub-agent dispatch:** `src/v53/harness/dispatch.{c,h}` —
  specialist spawn policy keyed on per-domain σ (security / performance /
  correctness defaults). Specialist runs in fresh context; returns
  summary + own σ (Mama Claude: uncorrelated context = test-time compute).
- **σ-prioritized compression:** `src/v53/harness/compress.{c,h}` —
  qualitative scoring (σ-resolution +2.0, invariant-reference +1.5,
  tool-use +1.0, peak σ +σ, filler −0.5); `v53_compress_batch` percentile
  cutoff keeps learning moments + invariant refs ahead of routine filler.
- **`creation.md` loader:** `src/v53/harness/project_context.{c,h}` —
  conservative markdown parser. Counts invariants, conventions,
  σ-profile rows. Missing file → `ok = 0`.
- **Invariants file:** `creation.md` at repo root — **invariants**,
  not instructions; σ-profile per task type; explicit rule "invariant
  beats convention; two-invariant conflict ⇒ abstain + surface".
- **Paper draft:** `docs/v53/paper_draft.md` — "Harness Architecture:
  Why σ-Governance Beats Time-Limits in Agentic Coding" (I-tier
  position paper; explicit follow-up benchmark scope).
- **Architecture + positioning:** `docs/v53/ARCHITECTURE.md`,
  `docs/v53/POSITIONING.md`.
- **Makefile:** `make check-v53` (13/13 self-test); `make standalone-v53`;
  help + `.PHONY` updated. **Not** part of `merge-gate`.

## v51 AGI-complete integration scaffold (2026-04-16)

- **Scaffold:** `src/v51/cognitive_loop.{c,h}` — six-phase loop
  (Perceive → Plan → Execute → Verify → Reflect → Learn). Pure C, deterministic,
  allocation-free hot path. Planner-facing σ is normalized softmax entropy in [0,1];
  v34 Dirichlet evidence is rescaled + surfaced as aleatoric/epistemic channels.
- **Agent:** `src/v51/agent.{c,h}` — σ-gated bounded agent loop with sandbox
  policy table (fail-closed on unknown tool), σ-deny threshold, and a ring
  experience buffer (cap 64). No "try anyway" loop — abstains cleanly.
- **UX spec:** `config/v51_experience.yaml` (chat/expert/serve/benchmark/certify
  modes — aspirational `creation-os` wrapper CLI documented honestly).
- **Web UI:** `src/v51/ui/web.html` — single-file static σ-dashboard mock
  (per-token color, abstention styled as calm gray notice, 8-channel view,
  System 1/2/Deep-Think indicator). No build step, no third-party deps.
- **Installer:** `scripts/v51/install.sh` — **safe dry-run** (no network,
  no `/usr/local/bin` writes, no weight downloads). Explains what a future
  signed `curl | sh` installer would do.
- **Architecture:** `docs/v51/ARCHITECTURE.md` — one-picture view of the
  v33–v50 layer stack, explicit "is / is not" scope section.
- **Makefile:** `make check-v51` (13/13 self-test); `make standalone-v51`;
  help + `.PHONY` updated. **Not** part of `merge-gate`.

## v50 final benchmark rollup harness (2026-04-16)

- **Harness:** `make v50-benchmark` → `benchmarks/v50/run_all.sh` (explicit **STUB** JSON slots for standard eval names until an engine+dataset harness exists).
- **Report:** `benchmarks/v50/FINAL_RESULTS.md` (generated; tables + honest tiering).
- **Automation:** `benchmarks/v50/generate_final_report.py` aggregates stub JSON + `certify`/`mcdc`/`binary_audit` logs.
- **Docs:** `docs/v50/FAQ_CRITICS.md`, `docs/v50/REDDIT_ML_POST_DRAFT.md` (do-not-post-until-real-numbers banner).
- **Honest scope:** Category 1–3 are **not** M-tier measurements yet; Category 4 logs what ran in-repo (`make certify`, coverage, audit).

## v49 certification-grade assurance pack (2026-04-16)

- **Docs:** `docs/v49/certification/` — DO-178C-*language* plans (PSAC/SDP/SVP/SCMP/SQAP), HLR/LLR/SDD, traceability matrix + `trace_manifest.json`, structural coverage notes, DO-333-style formal methods report, assurance ladder.
- **Code:** `src/v49/sigma_gate.{c,h}` — fail-closed scalar abstention gate (traceability + MC/DC driver target).
- **Tests / coverage:** `tests/v49/mcdc_tests.c` + `scripts/v49/run_mcdc.sh` (gcov/lcov best-effort).
- **Audit:** `scripts/v49/binary_audit.sh` (symbols/strings/strict compile/ASan+UBSan/optional valgrind — lab hygiene, not “Cellebrite certification”).
- **Trace automation:** `scripts/v49/verify_traceability.py`.
- **Makefile:** `make certify` / `certify-*` (aggregates formal targets + coverage + audit + `red-team` + trace checks).
- **Honest scope:** **not** FAA/EASA DAL-A certification; this is an **in-repo assurance artifact pack** aligned to DO-178C *discipline*.

## v48 σ-armored red-team lab (2026-04-16)

- **Artifact:** `creation_os_v48` — `detect_anomaly` (per-token epistemic statistics + baseline distance), `sandbox_check` (σ-gated privilege stub), `run_all_defenses` (7-layer fail-closed aggregate).
- **Red team:** `make red-team` (Garak/DeepTeam **SKIP** unless installed + REST model), `tests/v48/red_team/sigma_bypass_attacks.py`, `property_attacks.py` (pytest).
- **Gate:** `make merge-gate-v48` — `verify` + `red-team` + `check-v31` + `reviewer` (optional heavy; SKIPs OK when tools absent).
- **Docs:** [docs/v48/RED_TEAM_REPORT.md](docs/v48/RED_TEAM_REPORT.md); README σ-lab table + stack map; `docs/WHAT_IS_REAL.md`, `docs/SIGMA_FULL_STACK.md`, `docs/DOC_INDEX.md`, `CONTRIBUTING.md`.
- **Honest scope:** Garak/DeepTeam are **harness hooks**, not in-repo “90% defense” claims; σ-anomaly heuristics are **T-tier** lab code, not a certified robustness proof.

## v47 verified-architecture lab (2026-04-16)

- **Artifact:** `creation_os_v47` — ACSL-documented float σ-kernel (`src/v47/sigma_kernel_verified.c`, Frama-C/WP target), ZK-σ **API stub** (`src/v47/zk_sigma.c`, not succinct / not ZK), SymbiYosys **extended-depth** replay (`hdl/v47/sigma_pipeline_extended.sby`), Hypothesis property tests (`tests/v47/property_tests.py`), `make verify` / `trust-report`.
- **Verify:** `make check-v47`; broader stack: `make verify` (SKIPs when `frama-c` / `sby` / `pytest+hypothesis` absent).
- **Docs:** [docs/v47/INVARIANT_CHAIN.md](docs/v47/INVARIANT_CHAIN.md); README σ-lab table + stack map; `docs/WHAT_IS_REAL.md`, `docs/SIGMA_FULL_STACK.md`, `docs/DOC_INDEX.md`, `CONTRIBUTING.md`.
- **Honest scope:** Layer-7 ZK is **P-tier** until a circuit-backed prover exists; Frama-C “M-tier” requires a pinned proof log, not merely annotations.

## v46 σ-optimized BitNet pipeline lab (2026-04-16)

- **Artifact:** `creation_os_v46` — `v46_fast_sigma_from_logits` (Dirichlet σ + softmax entropy + margin blend), SIMD sum/max (`sigma_simd`), `v46_sigma_adaptive_quant`, latency profile helper.
- **Verify:** `make check-v46`; e2e stub: `make bench-v46-e2e`.
- **Docs / tables:** [docs/v46_bitnet_sigma.md](docs/v46_bitnet_sigma.md), [benchmarks/v46/SPEED_TABLE.md](benchmarks/v46/SPEED_TABLE.md) (explicit **I/M/N** tags); README σ-lab table + stack row; `docs/SIGMA_FULL_STACK.md`, `docs/WHAT_IS_REAL.md`, `docs/DOC_INDEX.md`, `CONTRIBUTING.md`.
- **Honest scope:** no wall-clock “<3% σ overhead” claim until `benchmarks/v46/*.json` exists; BitNet headline numbers remain **I-tier** imports unless reproduced in-repo.

## v45 σ-introspection lab (2026-04-16)

- **Artifact:** `creation_os_v45` — `v45_measure_introspection_lab` (σ-derived confidence vs synthetic self-report → `calibration_gap` + `meta_sigma`), `v45_doubt_reward`, `v45_probe_internals_lab` (deterministic internal σ stand-in).
- **Verify:** `make check-v45`; paradox stub: `make bench-v45-paradox`.
- **Docs:** [docs/v45_introspection.md](docs/v45_introspection.md); README σ-lab table + stack row; `docs/SIGMA_FULL_STACK.md`, `docs/WHAT_IS_REAL.md`, `docs/DOC_INDEX.md`, `CONTRIBUTING.md`.
- **Honest scope:** no archived multi-model introspection scatter until harness + `benchmarks/v45/introspection_*.json` exist; no real hidden-state probes until engine hooks land.

## v44 σ-native inference proxy lab (2026-04-16)

- **Artifact:** `creation_os_proxy` — stub logits → per-token Dirichlet σ → `decode_sigma_syndrome()` actions → OpenAI-shaped `POST /v1/chat/completions` (+ extra `choices[].sigma` JSON) and demo `text/event-stream` chunks.
- **Verify:** `make check-v44` (alias: `make check-proxy`); overhead stub: `make bench-v44-overhead`.
- **Docs / config:** [docs/v44_inference_proxy.md](docs/v44_inference_proxy.md), [config/proxy.yaml](config/proxy.yaml); README σ-lab table + stack row; `docs/SIGMA_FULL_STACK.md`, `docs/WHAT_IS_REAL.md`, `docs/DOC_INDEX.md`, `CONTRIBUTING.md`.
- **Honest scope:** no archived engine A/B overhead JSON until harness scripts exist; streaming “retraction” is a **demo contract** only.

## v43 σ-guided knowledge distillation lab (2026-04-16)

- **Artifact:** `creation_os_v43` — σ-weighted KL (`v43_sigma_weight`, forward / reverse KL), progressive teacher-epistemic stages (`v43_distill_stages`), multi-teacher σ ensemble logits, calibration + total loss helpers.
- **Verify:** `make check-v43`; benchmark stub: `make bench-v43-distill`.
- **Docs:** [docs/v43_sigma_distill.md](docs/v43_sigma_distill.md); `docs/SIGMA_FULL_STACK.md`, `docs/WHAT_IS_REAL.md`, `docs/DOC_INDEX.md`; README σ-lab table + LLM stack row; `CONTRIBUTING.md` optional labs row.
- **Honest scope:** no in-tree `--distill` / TruthfulQA harness until weights + driver + REPRO bundle exist (tier tags in WHAT_IS_REAL).

## RTL silicon mirror + formal CI stack (2026-04-16)

- **RTL:** `rtl/cos_*_iron_combo.sv`, `cos_agency_iron_formal.sv`, `cos_agency_iron_cover.sv`, `cos_boundary_sync.sv`, `cos_silicon_chip_tb.sv`.
- **Chisel / Rust / Yosys / SBY / EQY:** `hw/chisel/`, `hw/rust/spektre-iron-gate`, `hw/yosys/*.ys`, `hw/formal/*.sby`, `hw/formal/agency_self.eqy`, [hw/openroad/README.md](hw/openroad/README.md).
- **Makefile:** `merge-gate` unchanged; add `stack-ultimate`, `stack-nucleon`, `stack-singularity`, `oss-formal-extreme`, `rust-iron-lint`, Verilator `-Wall --timing`, Yosys `sat -prove-asserts`, Chisel targets.
- **CI:** `creation-os/.github/workflows/ci.yml` — `merge-gate` + `stack-ultimate` + `rust-iron-lint` on apt tools; **`oss-cad-formal`** job ([`YosysHQ/setup-oss-cad-suite@v4`](https://github.com/YosysHQ/setup-oss-cad-suite)) runs **`make oss-formal-extreme`**; **`c11-asan-ubsan`**; **CodeQL**, **OpenSSF Scorecard**, **ShellCheck** workflows; Dependabot weekly on Actions.
- **Publish:** `tools/publish_to_creation_os_github.sh` preflight is **`make merge-gate && make stack-ultimate && make rust-iron-lint`**.
- **Docs:** [docs/RTL_SILICON_MIRROR.md](docs/RTL_SILICON_MIRROR.md), [docs/FULL_STACK_FORMAL_TO_SILICON.md](docs/FULL_STACK_FORMAL_TO_SILICON.md); `CONTRIBUTING.md`, `AGENTS.md`, [hw/formal/README.md](hw/formal/README.md).

## The Tensor mind v12 (2026-04-15)

- **Artifact:** [`creation_os_v12.c`](creation_os_v12.c) — v11 **plus** M35–M37 (MPS contraction toy, entanglement σ-meter on singular-value toy, TN sequence head); **52** `self_test` checks.
- **Verify:** `make check-v12`; CI runs `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11 && make check-v12`.
- **Docs:** [docs/THE_TENSOR_MIND_V12.md](docs/THE_TENSOR_MIND_V12.md); [docs/FEATURES_AND_STANDALONE_BUILDS.md](docs/FEATURES_AND_STANDALONE_BUILDS.md); README + DOC_INDEX + MODULE_EVIDENCE_INDEX + RESEARCH + CLAIM_DISCIPLINE + ADVERSARIAL + sibling docs (v6/v7/v9/v10/v11); [kernel-lineage-evidence.svg](docs/assets/kernel-lineage-evidence.svg) label M01–M37.
- **Build / publish:** `Makefile` `standalone-v12`, `test-v12`, `check-v12`; `.gitignore` + `publish_to_creation_os_github.sh` strip `creation_os_v12`; preflight runs all `check-v*` after `make check`.

## Visuals — pro diagram pass (2026-04-15)

- **SVG refresh:** [architecture-stack.svg](docs/assets/architecture-stack.svg), [bsc-primitives.svg](docs/assets/bsc-primitives.svg), [gemm-vs-bsc-memory-ops.svg](docs/assets/gemm-vs-bsc-memory-ops.svg), [evidence-ladder.svg](docs/assets/evidence-ladder.svg), [planes-abc.svg](docs/assets/planes-abc.svg) — consistent typography, shadows, accent rules, updated v2 line count, legend + callouts on benchmark figure.
- **New:** [kernel-lineage-evidence.svg](docs/assets/kernel-lineage-evidence.svg) — portable proof vs v6–v11 lab-demo column chart for thesis/README.
- **Docs:** [VISUAL_INDEX.md](docs/VISUAL_INDEX.md) — theme column, design-system section, citation note for lineage figure; README doctoral path embeds lineage SVG; DOC_INDEX pointer.

## Docs — doctoral framing (2026-04-15)

- **README:** [doctoral and committee read path](README.md#doctoral-and-committee-read-path) — ordered list (CLAIM_DISCIPLINE → RESEARCH → REPRO bundle → FEATURES map → MODULE_EVIDENCE_INDEX → v6–v11 scoped docs → adversarial checklist) + v2 vs v6–v11 evidence-class table.
- **Iteration:** [LIVING_KERNEL_V6.md](docs/LIVING_KERNEL_V6.md), [HALLUCINATION_KILLER_V7.md](docs/HALLUCINATION_KILLER_V7.md), [PARAMETERS_IN_SILICON_V9.md](docs/PARAMETERS_IN_SILICON_V9.md) — **Threats to validity** + **How to cite** (parity with v10/v11); [ADVERSARIAL_REVIEW_CHECKLIST.md](docs/ADVERSARIAL_REVIEW_CHECKLIST.md) §A rows for forbidden merges **#5** / **#6** and v7 naming; footer links README doctoral path.
- **RESEARCH_AND_THESIS_ARCHITECTURE:** v6–v11 as explicit **lab-demo-only** row under §0; extended §5 threats table; optional thesis appendix for v6–v11; §11 gates link README path + **FEATURES_AND_STANDALONE_BUILDS**.
- **CLAIM_DISCIPLINE:** forbidden merges **#5** (v11 × BitNet-class claims), **#6** (v6–v11 `self_test` × frontier / tape-out / harness).
- **THE_REAL_MIND_V10** / **THE_MATMUL_FREE_MIND_V11:** threats-to-validity + **how to cite** blurbs; **DOC_INDEX** / **AGENTS** pointers updated.

## Ops — canonical Git (2026-04-15)

- **Docs:** [docs/CANONICAL_GIT_REPOSITORY.md](docs/CANONICAL_GIT_REPOSITORY.md) — only **https://github.com/spektre-labs/creation-os** is the product remote; parent protocol / umbrella checkouts must not receive `creation-os` as `origin`.
- **AGENTS / README / DOC_INDEX / publish script** point agents and humans to that rule.

## MatMul-free mind v11 (2026-04-15)

- **Artifact:** [`creation_os_v11.c`](creation_os_v11.c) — v10 **plus** M34 matmul-free LM schematic (ternary `BitLinearLayer` + element-wise `MLGRUState` forward); **49** `self_test` checks.
- **Verify:** `make check-v11`; CI runs `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11`.
- **Docs:** [docs/THE_MATMUL_FREE_MIND_V11.md](docs/THE_MATMUL_FREE_MIND_V11.md); [docs/FEATURES_AND_STANDALONE_BUILDS.md](docs/FEATURES_AND_STANDALONE_BUILDS.md); README hub + DOC_INDEX + GLOSSARY + MODULE_EVIDENCE_INDEX + COMMON_MISREADINGS + ROADMAP; AGENTS + CONTRIBUTING + publish preflight aligned with v11.

## The Real Mind v10 (2026-04-15)

- **Artifact:** [`creation_os_v10.c`](creation_os_v10.c) — v9 Parameters in Silicon **plus** M30–M33 schematic channels (distillation toy, prototypical few-shot distance, specialist swarm routing, σ-aware abstention gate); **46** `self_test` checks.
- **Verify:** `make check-v10`; CI runs `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11`.
- **Docs:** [docs/THE_REAL_MIND_V10.md](docs/THE_REAL_MIND_V10.md); v11 sibling: [docs/THE_MATMUL_FREE_MIND_V11.md](docs/THE_MATMUL_FREE_MIND_V11.md); [docs/FEATURES_AND_STANDALONE_BUILDS.md](docs/FEATURES_AND_STANDALONE_BUILDS.md) (single-page feature map); README + DOC_INDEX + GLOSSARY; v9 doc links v10 as sibling.
- **Build / publish:** `Makefile` targets `standalone-v10`, `test-v10`, `check-v10`; `.gitignore` + publish script strip `creation_os_v10`; publish preflight runs v6/v7/v9/v10/v11 after `make check`.

## Parameters in Silicon v9 (2026-04-15)

- **Artifact:** [`creation_os_v9.c`](creation_os_v9.c) — v7 Hallucination Killer **plus** M24–M29 schematic channels (neuromorphic event toy, CIM σ_transfer, memory wall, BNN toy, silicon-stack toy, heterogeneous orchestrator); **41** `self_test` checks.
- **Verify:** `make check-v9`; CI runs `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11`.
- **Docs:** [docs/PARAMETERS_IN_SILICON_V9.md](docs/PARAMETERS_IN_SILICON_V9.md); README + DOC_INDEX + GLOSSARY; v7 doc links v9 as sibling; [THE_REAL_MIND_V10.md](docs/THE_REAL_MIND_V10.md) is the v10 extension.
- **Build / publish:** `Makefile` targets `standalone-v9`, `test-v9`, `check-v9`; `.gitignore` + publish script strip `creation_os_v9`; `publish_to_creation_os_github.sh` preflight runs v6/v7/v9/v10/v11 checks after `make check`.

## Hallucination Killer v7 (2026-04-15)

- **Artifact:** [`creation_os_v7.c`](creation_os_v7.c) — v6 Living Kernel **plus** M19–M23 schematic channels (anchor collapse, association ratio, bluff σ, context rot, JEPA–Oracle representation error); **35** `self_test` checks.
- **Verify:** `make check-v7`; CI runs `make check && make check-v6 && make check-v7 && make check-v9 && make check-v10 && make check-v11`.
- **Docs:** [docs/HALLUCINATION_KILLER_V7.md](docs/HALLUCINATION_KILLER_V7.md); README hub + *Hallucination Killer (v7)* section; [LIVING_KERNEL_V6.md](docs/LIVING_KERNEL_V6.md) links v7 as sibling.
- **Build / publish:** `Makefile` targets `standalone-v7`, `test-v7`, `check-v7`; `.gitignore` + publish script strip `creation_os_v7` binary.

## Living Kernel v6 (2026-04-15)

- **Artifact:** [`creation_os_v6.c`](creation_os_v6.c) — second standalone C11 program: σ–`K`–`L`–`S` scaffold, M01–M18 narrative modules (RDP, alignment tax, preference collapse, RAIN-style σ-tape, ghost boot, Gödel-boundary toy, …), 1024-bit packed BSC in this file (distinct from 4096-bit `creation_os_v2.c` + `COS_D`).
- **Verification:** `make check-v6` builds `creation_os_v6` and runs `./creation_os_v6 --self-test` (**30** deterministic checks).
- **Docs:** [docs/LIVING_KERNEL_V6.md](docs/LIVING_KERNEL_V6.md) — scope, evidence class, non-claims, module map; README hub row + *Living Kernel (v6)* section + limitations bullet; [docs/COMMON_MISREADINGS.md](docs/COMMON_MISREADINGS.md) row for v6 vs harness / paper confusion.
- **Build / publish:** `Makefile` targets `standalone-v6`, `test-v6`, `check-v6`; `.gitignore` + publish script strips `creation_os_v6` binary before rsync.

## v2.0 (2026-04-15)

- **Research:** [docs/RESEARCH_AND_THESIS_ARCHITECTURE.md](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md) — thesis-grade research program (bounded RQs, contributions C1–C6, threats to validity, chapter outline); [CITATION.cff](CITATION.cff) for academic software citation; [docs/CITATION.bib](docs/CITATION.bib); [docs/ADVERSARIAL_REVIEW_CHECKLIST.md](docs/ADVERSARIAL_REVIEW_CHECKLIST.md); [docs/MODULE_EVIDENCE_INDEX.md](docs/MODULE_EVIDENCE_INDEX.md); RESEARCH doc §11 links all three.
- **Industry alignment:** [docs/COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md](docs/COHERENCE_RECEIPTS_INDUSTRY_ALIGNMENT.md) — public-theme mapping (eval, deploy, robotics, on-device) to coherence receipts; Anthropic / DeepMind / OpenAI / Apple as **illustrative** anchors only; explicit non-claims; robotics pre-actuation gate narrative.
- **Ops excellence:** [docs/GLOSSARY.md](docs/GLOSSARY.md); [docs/BENCHMARK_PROTOCOL.md](docs/BENCHMARK_PROTOCOL.md) (§7 / `make bench` audit); [.github/PULL_REQUEST_TEMPLATE.md](.github/PULL_REQUEST_TEMPLATE.md) claim-hygiene checklist; CONTRIBUTING links updated.
- **Paradigm explainer:** [docs/PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md](docs/PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md) — compressed plain-language contrast to default ML stacks; README blurb + DOC_INDEX.
- **English-only governance:** [docs/LANGUAGE_POLICY.md](docs/LANGUAGE_POLICY.md); [docs/COMMON_MISREADINGS.md](docs/COMMON_MISREADINGS.md) FAQ table; AGENTS + CONTRIBUTING point to policy; [bug_report](.github/ISSUE_TEMPLATE/bug_report.md) template (English); PARADIGM snapshot links misreadings.
- **Maintainers & supply chain hygiene:** [docs/MAINTAINERS.md](docs/MAINTAINERS.md); [docs/SECURITY_DEVELOPER_NOTES.md](docs/SECURITY_DEVELOPER_NOTES.md); [documentation](.github/ISSUE_TEMPLATE/documentation.md) issue template; [.github/dependabot.yml](.github/dependabot.yml) for Actions (monthly); SECURITY links developer notes.
- **Native metal:** `core/cos_neon_hamming.h` — AArch64 NEON Hamming (4096-bit) + prefetch; scalar fallback; `make bench-coherence`; test parity on AArch64; [docs/NATIVE_COHERENCE_NEON.md](docs/NATIVE_COHERENCE_NEON.md) (edge / embodied gate loop rationale).
- **Publish hygiene:** `coherence_gate_batch` in `.gitignore`; `tools/publish_to_creation_os_github.sh` strips Makefile binaries before commit so they are never pushed again.
- **HV AGI gate:** `core/cos_parliament.h` (odd-K bit-majority fusion; K=3 ≡ MAJ3); `core/cos_neon_retrieval.h` (argmin Hamming over memory bank); `bench/hv_agi_gate_neon.c` + `make bench-agi-gate`; tests for parliament/argmin; [docs/HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md](docs/HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md); [NATIVE_COHERENCE_NEON.md](docs/NATIVE_COHERENCE_NEON.md) links the stack-up doc.
- **Publish:** `make publish-github` runs `tools/publish_to_creation_os_github.sh` (`make check`, clone **spektre-labs/creation-os**, rsync, commit, push).
- **Docs / naming:** Creation OS–only messaging in README and ANALYSIS; [docs/publish_checklist_creation_os.md](docs/publish_checklist_creation_os.md), [docs/cursor_briefing_creation_os.md](docs/cursor_briefing_creation_os.md), [docs/cursor_integration_creation_os.md](docs/cursor_integration_creation_os.md); removed older multi-repository map and superseded publish doc names from this tree.
- Visuals: **`docs/assets/*.svg`** (architecture, BSC primitives, GEMM vs BSC bars, evidence ladder, Planes A–C) + **[docs/VISUAL_INDEX.md](docs/VISUAL_INDEX.md)**; README embeds figures + Mermaid evidence-flow.
- Docs: **[docs/HDC_VSA_ENGINEERING_SUPERIORITY.md](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md)** — web-grounded literature map (Ma & Jiao 2022; Aygun et al. 2023; Springer AIR HDC review 2025; Yeung et al. Frontiers 2025; FAISS popcount PR) + safe vs unsafe claim table + deck paragraph; links from README / EXTERNAL / AGENTS.
- **Contributor / ops:** [CONTRIBUTING.md](CONTRIBUTING.md), [SECURITY.md](SECURITY.md), [AGENTS.md](AGENTS.md); [docs/DOC_INDEX.md](docs/DOC_INDEX.md); [docs/REPRO_BUNDLE_TEMPLATE.md](docs/REPRO_BUNDLE_TEMPLATE.md); `make help` / `make check`; GitHub Actions CI (`.github/workflows/ci.yml`).
- Docs: **[docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md](docs/EXTERNAL_EVIDENCE_AND_POSITIONING.md)** — vetted external citations (Kanerva BSC/HDC, Schlegel–Neubert–Protzel VSA survey, EleutherAI `lm-evaluation-harness`); field consensus vs in-repo proofs.
- Docs: **[docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md)** — evidence classes, forbidden baseline merges, falsifiers, reproducibility bundle; README adds reviewer-proof benchmark interpretation + **Publication-hard** section (cross-examination standard, not hype).
- 26 modules in single standalone file `creation_os_v2.c`
- Correlative encoding for Oracle (generalization)
- Iterative retraining (10 epochs)
- Cross-pattern correlation
- GEMM vs BSC benchmark: 32× memory, 192× fewer ops (proxy); wall-clock and trials/sec printed (`make bench` / §7)
- Noether conservation: σ = 0.000000 (genesis toy)
- Public tree: `core/`, `gda/`, `oracle/`, `physics/`, `tests/`, `Makefile`, AGPL `LICENSE`

## v1.1

- Added: Metacognition, Emotional Memory, Theory of Mind, Moral Geodesic, Consciousness Meter, Inner Speech, Attention Allocation, Epistemic Curiosity, Sleep/Wake, Causal Verification, Resilience, Meta Goals, Privacy, LSH Index, Quantum Decision, Arrow of Time, Distributed Consensus, Authentication

## v1.0

- Initial release: 8 modules (BSC Core, Mind, Oracle, Soul, Proconductor, JEPA, Benchmark, Genesis)
