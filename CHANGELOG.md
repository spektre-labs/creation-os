# Changelog

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
