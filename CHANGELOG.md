# Changelog

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
