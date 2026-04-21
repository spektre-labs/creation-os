<p align="center">
  <img src="docs/assets/reddit-hook-banner.svg" width="100%" alt="Creation OS — compile on real silicon" decoding="async" fetchpriority="high" style="max-width:min(1200px,100%);height:auto;border-radius:14px;box-shadow:0 4px 24px rgba(15,23,42,0.18);"/>
</p>

<h1 align="center">Creation OS</h1>

<p align="center"><sub><strong>A local AI runtime that proves every answer before it shows it to you.</strong><br/>
Forty branchless integer kernels · one composed verdict · <strong>1 = 1</strong>.</sub></p>

<p align="center">
  <a href="#try-it"><img src="https://img.shields.io/badge/try%20it-in%2030%20seconds-111827?style=for-the-badge&labelColor=0ea5e9" alt="Try it in 30 seconds"/></a>
  <a href="#measured"><img src="https://img.shields.io/badge/TruthfulQA%20817-0.261%20%E2%86%92%200.336-059669?style=for-the-badge&labelColor=065f46" alt="TruthfulQA 817 accuracy lift"/></a>
  <a href="#proof-status"><img src="https://img.shields.io/badge/proofs-Lean%206%2F6%20%C2%B7%20Frama--C%2015%2F15-1d4ed8?style=for-the-badge&labelColor=1e3a8a" alt="Proofs — Lean 6/6, Frama-C 15/15"/></a>
  <a href="#architecture"><img src="https://img.shields.io/badge/hot%20path-branchless%20%C2%B7%20Q16.16%20%C2%B7%20libc%20only-7c3aed?style=for-the-badge&labelColor=5b21b6" alt="hot path: branchless · Q16.16 · libc only"/></a>
</p>

## Contents

- [Try it](#try-it)
- [Measured](#measured)
- [Architecture](#architecture)
- [Build](#build)
- [Proof status](#proof-status)
- [Surface versions (v112 – v278+)](#surface-versions)
- [Docs hub](#docs-hub)
- [Doctoral and committee read path](#doctoral-and-committee-read-path)
- [Limitations](#limitations)
- [License](#license)

---

<a id="try-it"></a>

## Try it

One path, from a terminal, on any macOS or Linux host:

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
./scripts/install.sh
./cos chat
```

`scripts/install.sh` checks for `python3`, `cmake`, and a C compiler;
if `huggingface-cli` and `cmake` are present, it downloads the 1.2 GB
[`BitNet-b1.58-2B-4T`](https://huggingface.co/microsoft/BitNet-b1.58-2B-4T)
GGUF weights into `models/`, builds `third_party/bitnet`
(`llama-cli` + `llama-perplexity`), builds the `cos` binary, and runs
`cos chat --once --prompt "What is 2+2?"` as a smoke test.  Set
`COS_INSTALL_NO_BITNET=1` to skip the model download for CI-only
clones.

Everything runs **locally**.  Nothing is sent to the cloud.  Nothing
is logged.  Nothing calls home.  Safe to re-run; idempotent.

### What `cos chat` can do

`cos chat` is a σ-gated REPL with four wired phases (see
[`src/cli/cos_chat.c`](src/cli/cos_chat.c)):

| phase | primitive | flag |
|:------|:----------|:-----|
| **A** σ_combined ensemble | `cos_multi_sigma_combine` — logprob · entropy · perplexity · consistency | `--multi-sigma` |
| **B** conformal τ | `cos_conformal_read_bundle_json` · auto-loads `~/.cos/calibration.json` | on by default; `--no-conformal` to opt out |
| **C** meta-cognitive σ | `cos_ultra_metacog_*` — perception · self · social · situational | `--verbose` |
| **D** session coherence | `cos_ultra_coherence_emit_report` — dσ/dt, K_eff, {STABLE, DRIFTING, AT_RISK} | REPL-only; `--no-coherence` to opt out |

Example:

```bash
./cos chat --once --prompt "What is 2+2?" --multi-sigma --verbose
# → [meta: perception=0.35 self=0.06 social=0.45 situational=0.00]
# → round 0  4  [σ_peak=0.06 action=ACCEPT route=LOCAL]
# → [σ=0.063 | CACHE | LOCAL | conformal@α=0.80 | rethink=0 | €0.0000]
# → [σ_combined=0.184 | σ_logprob=0.063 σ_entropy=0.063
#    σ_perplexity=0.063 σ_consistency=0.667 | k=3]
```

---

<a id="measured"></a>

## Measured

Two independent, reproducible evidence surfaces — both use real
BitNet-b1.58 2B4T weights; neither is simulated.  Claim-class rules:
[`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md).

### 1. TruthfulQA 817 (generation/validation, end-to-end)

Full run of the TruthfulQA generation/validation split through
`llama-cli`, scored by substring match against each row's
`correct_answers` / `incorrect_answers`.  Raw artefacts:
[`benchmarks/pipeline/truthfulqa_817.json`](benchmarks/pipeline/truthfulqa_817.json) ·
[`benchmarks/pipeline/truthfulqa_817_detail.jsonl`](benchmarks/pipeline/truthfulqa_817_detail.jsonl) ·
commentary [`docs/domain_analysis.md`](docs/domain_analysis.md).

| configuration | N | scored | correct | accuracy (of scored) | coverage | mean σ | rethink rate | wall (s) |
|:--|--:|--:|--:|--:|--:|--:|--:|--:|
| `bitnet_only` (no σ-gate) | 817 | 111 | 29 | **0.261** | 0.136 | 0.370 | 0.000 | 1 554.8 |
| `pipeline` (σ-gate on)    | 817 | 140 | 47 | **0.336** | 0.171 | 0.391 | 0.991 | 4 804.7 |

On the same 817 prompts and seeds, the σ-pipeline lifts
scored-accuracy from **0.261 → 0.336 (+28.7 % relative)** and coverage
from 0.136 → 0.171 (+25.7 % relative).  Mean σ is essentially
unchanged (0.370 → 0.391), so the gain comes from **selective
regeneration on initially-uncertain rows**, not from the model itself
becoming more confident.  All numbers are read directly from the JSON
artefact; no row is projected.

“Accuracy (of scored)” is conservative — rows whose generated text
contains neither a correct nor an incorrect string are excluded from
both numerator and denominator — and is **not** directly comparable to
`lm-eval` MC2.  See [`docs/BENCHMARK_PROTOCOL.md`](docs/BENCHMARK_PROTOCOL.md).

**Conformal guarantee (SCI-1, α=0.80, δ=0.10):** the same run yields
τ=0.655 with P(wrong | σ≤τ) ≤ α on exchangeable draws from the
calibration distribution.  Caveats and the scope of the bound:
[`docs/v111/CONFORMAL_GUARANTEE.md`](docs/v111/CONFORMAL_GUARANTEE.md).

### 2. v111 Frontier parity matrix (σ vs entropy, Bonferroni-controlled)

Pre-registered σ-gate vs entropy baseline on four benchmark families.
`σ` is **not** a universal calibration signal — this table is the
single source of truth, positive and negative results side by side.

| family | task | status at α_fw = 0.05 | signal | ΔAURCC | n |
|:---|:---|:---:|:---|---:|---:|
| **PRE-REGISTERED** | `truthfulqa_mc2` | **win** (v111.1, Bonf N=24) | `sigma_max_token` | −0.0447 (p = 0.0005) | 817 |
| **PRE-REGISTERED** | `truthfulqa_mc2` | **win** (v111.2-prereg test split, Bonf N=12) | `sigma_task_adaptive` | −0.0681 (p ≈ 0.0005) | 409 |
| POST-HOC | `arc_challenge` | directional, **not replicated** at α_fw | `sigma_product` | −0.0087 (full-data p = 0.004; test-split p = 0.145) | 1172 / 586 |
| NEGATIVE | `hellaswag` | σ not dominant | — (entropy baseline) | `σ_product` Δ = −0.0016, p = 0.68 | 746 |
| NEGATIVE | `mmlu_*` (7 eligible / 10 candidates) | σ not dominant — **0 / 28** Bonf-sig. cells | — (entropy baseline) | best σ Δ = +0.0000, worst = +0.0152 | 605 |

Lower AURCC is better.  Full table with CI95 and p-values:
[`benchmarks/v111/results/frontier_matrix.md`](benchmarks/v111/results/frontier_matrix.md).
Reproduce end-to-end:

```bash
bash benchmarks/v111/run_matrix.sh               # all four tasks
bash benchmarks/v111/check_v111_matrix.sh        # CI-safe smoke
```

The σ-gate's Bonferroni-significant domain is therefore **bounded to
TruthfulQA-style factual-confidence tasks**, not general MMLU-style
knowledge-QA.  Methodology and signal definitions:
[`docs/v111/THE_FRONTIER_MATRIX.md`](docs/v111/THE_FRONTIER_MATRIX.md).

### 3. Multi-dataset σ-gate suite (SCI-6)

Aggregator:
[`./cos-bench-suite-sci`](src/sigma/pipeline/suite_sci_main.c).
Output: [`benchmarks/suite/full_results.json`](benchmarks/suite/full_results.json),
schema `cos.suite_sci.v1`.

| dataset | status | N | acc(all) | acc(accepted) | coverage | σ_mean | τ | conformal |
|:---|:---|--:|--:|--:|--:|--:|--:|:---|
| TruthfulQA (gen/val, scored) | **measured** | 817 | **0.336** | 0.336 | 0.171 | 0.391 | 0.655 | yes @ (α=0.80, δ=0.10) |
| ARC-Challenge | wired, JSONL pending | — | — | — | — | — | — | — |
| ARC-Easy | wired, JSONL pending | — | — | — | — | — | — | — |
| GSM8K | wired, JSONL pending | — | — | — | — | — | — | — |
| HellaSwag | wired, JSONL pending | — | — | — | — | — | — | — |

The aggregator records `"measured": false` with zero-filled metrics
for any dataset whose detail JSONL is absent — there are **no
projected numbers** in the suite table.  Production recipe:
[`benchmarks/suite/README.md`](benchmarks/suite/README.md).

---

<a id="architecture"></a>

## Architecture

### ULTRA pipeline (one turn, from prompt to receipt)

```
 Prompt
   │
   ▼
 [ I0 ] Codex loaded as system prompt ......... cos_sigma_codex_load
   │
   ▼
 [ 3 ] Engram lookup → HIT?  ~0.5 ns ........... cos_sigma_engram_get
   │                                   │
   ▼ MISS                              └─► ACCEPT
 [ 4 ] Generate (local, BitNet-class) .......... cos_pipeline_generator_fn
   │
   ▼
 [ 5 ] σ = f(logprob, entropy, perplexity,
             consistency)  (SCI-5 ensemble) .... cos_multi_sigma_combine
   │
   ▼
 [ 7 ] σ-gate with conformal τ (SCI-1) ......... cos_sigma_reinforce
   │                                          │
   ├─► ACCEPT → engram store → receipt         │
   │                                          │
   ▼ RETHINK (≤ N rounds, drift-bounded)       │
 [ 8 ] Fast-weight TTT + continual freeze ...... cos_inplace_ttt
   │                                            │
   ▼ budget exhausted, σ still ≥ τ              │
 [ 9 ] Escalate: swarm or API (DEV-5) .......... cos_cli_escalate_api
   │                                            │
   ▼                                            │
 [10-12] Agent gate · diagnostic · τ adapt ..... cos_sigma_agent_gate
                                                 cos_sigma_live_update
   │
   ▼
 [13] Engram write-through (~/.cos/engram.db) .. cos_engram_persist_store
   │
   ▼
  Response  +  σ_combined  +  cost (€)  +  reasoning/joule
```

Canonical source: [`src/sigma/pipeline/pipeline.h`](src/sigma/pipeline/pipeline.h) ·
[`src/sigma/pipeline/pipeline.c`](src/sigma/pipeline/pipeline.c) ·
[`src/cli/cos_chat.c`](src/cli/cos_chat.c).

### Forty integer kernels, one AND gate

Every emission from `cos chat` passes forty integer kernels — each one
a falsifiable statement about the answer.  Categories:

```
 reasoning soundness · reversibility · meta-cognition
 world-model coherence · memory integrity
 adaptive compute · geometric algebra · sheaf topology
 post-quantum crypto · homomorphic compute
 neuromorphic spikes · hierarchical active inference
 quantum amplitude amplification · integer diffusion sampler
 Q-learning + GAE + PPO · persistent homology
 structural causal do-calculus · sub-quadratic Hyena
 security · provenance
```

Hot path: branchless, Q16.16 fixed-point, libc + libm only.  The
runtime **refuses to emit** unless every one of the forty kernels
returns PASS.  Rollup target: `make check-v60` … `make check-v100`.

Full forty-kernel receipt (16 416 185 PASS / 0 FAIL as of current
head, ASAN + UBSAN clean):
[`docs/README_FULL.md`](docs/README_FULL.md#the-forty-kernel-receipt).
Figure and palette rules: [`docs/VISUAL_INDEX.md`](docs/VISUAL_INDEX.md).

### BSC primer

Binary Spatter Coding (BSC) in a nutshell:
`bind = XOR` · `bundle = popcount threshold` · similarity = `1 −
hamming/D`.  Memory is one bit per dimension; binding and bundling are
branchless on every hot path.  The Spektre Corpus traces this lineage
from Kanerva (1988, 1994) forward to the 2025 HDC/VSA robustness
estimation literature — see
[`docs/HDC_VSA_ENGINEERING_SUPERIORITY.md`](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md)
and [`data/corpus/INDEX.md`](data/corpus/INDEX.md).

---

<a id="build"></a>

## Build

Minimal (any C11 + libm host):

```bash
cc -O2 -I. -o creation_os creation_os_v2.c -lm
./creation_os
```

With `make`, the repo default is `CFLAGS = -O2 -march=native -Wall
-std=c11` and `LDFLAGS = -lm`:

```bash
make help           # full target list (labs, RTL, benches)
make check          # standalone + tests/test_bsc_core (fast)
make check-ultra    # ULTRA-1..11 kernels + bundle + --energy
make check-sigma-pipeline
                    # every σ-pipeline kernel + 12 integration scenarios
make merge-gate     # check + check-v6 … check-v306 (maintainer bar)
```

Flagship check targets: `make check-v6` (Living Kernel, 30 self-tests)
… `make check-v29` (collapse harness, 22 self-tests);
`make check-v60` … `make check-v100` (forty-kernel composed stack).

Optional (not in `merge-gate`): σ labs, MCP, RTL, native-M4 —
`make formal-sby-v37`, `make verify-agent`, `make red-team`,
`make certify`, `make check-mcp`, `make check-native-m4`,
`make formal-rtl-lint`, `make stack-ultimate`.

Host metadata when publishing numbers:
[`docs/REPRO_BUNDLE_TEMPLATE.md`](docs/REPRO_BUNDLE_TEMPLATE.md).

---

<a id="proof-status"></a>

## Proof status

- **Lean 4**: 6 / 6 theorems discharged, **sorry-free** —
  [`hw/formal/v259/Measurement.lean`](hw/formal/v259/Measurement.lean);
  `make check-v259`.
- **Frama-C Wp**: 15 / 15 tier-1 proof obligations discharged —
  `make check-framac-tier1`.
- **SBY + EQY** (YosysHQ OSS CAD Suite, optional): `make stack-singularity` —
  [`hw/formal/README.md`](hw/formal/README.md).
- **Formalism → silicon map**:
  [`docs/FULL_STACK_FORMAL_TO_SILICON.md`](docs/FULL_STACK_FORMAL_TO_SILICON.md).
- **Conformal guarantee** (selective prediction, Angelopoulos-Bates):
  P(wrong | σ ≤ τ) ≤ α with confidence 1 − δ on the calibration draw —
  [`docs/v111/CONFORMAL_GUARANTEE.md`](docs/v111/CONFORMAL_GUARANTEE.md).

---

<a id="surface-versions"></a>

## Surface versions (v112 – v278+)

The long per-version catalogue lives in
[`docs/SURFACE_VERSIONS.md`](docs/SURFACE_VERSIONS.md) so this README
stays short.  Each row names the `make check-vNN` target and the
dominant primitive.  At head:

- **v112 – v128** — agentic stack · memory / MCP / long-context ·
  speculative / distill / planning / red-team / formal · living
  weights.
- **v129 – v153** — collective intelligence · deep infrastructure ·
  world intelligence · sovereign self-improvement · embodied /
  swarm / code-agent / distill / identity.
- **v154 – v193** — showcase / publish / paper / community · 1.0
  release · self-healing / composable · plugins / edge / stream /
  governance / marketplace · ontology / transfer / teach · flywheel /
  debate / simulator / compress / consensus · interpret / steer /
  audit / privacy · VLA / fusion / grow / calibration / alignment ·
  TTC / latent-reason / constitutional / emergent / coherence.
- **v194 – v238** — horizon / recover / habit / ToM / moral · law /
  market / diplomacy / culture / civilization · hypothesis /
  experiment / theorem / design / manufacture · containment / guardian
  / sandbox-formal / transparency / trust-chain · swarm-evolve /
  stigmergy / quorum / ecosystem / consciousness-meter · create /
  simulate / language / emotion / meta-cognition · tensor / fractal /
  attention / entropy / unified · seed / fork / immortal / lineage /
  legacy · presence / locus / autobiography / boundary / sovereignty.
- **v239 – v278+** — runtime / pipeline / API / kernel-OS · package /
  observe / harden / benchmark-suite / release · MCP / A2A /
  marketplace / teach / ecosystem-hub · tutor / collaborate / wellness
  / locale / mission · engram / airllm / hybrid / mesh-engram /
  sovereign-stack · speculative / flash / mamba / continuous-batch /
  compile-v2 · TinyML / swarm-edge / digital-twin / robotics /
  industrial · TTT / DeltaNet / distill-runtime / RSI.
- **v279 – v306** — JEPA / MoE / Jamba / agent · constitutional /
  multi-agent / EU AI Act / interpretability · Granite / Oculus /
  ruin-value / Dougong / Parthenon / Leanstral / Hagia Sofia ·
  federated / immune / antifragile / clock / Rosetta · knowledge-graph
  / complete · zkp / green / governance / narrative / swarm / omega.

---

<a id="docs-hub"></a>

## Docs hub

Canonical index: [`docs/DOC_INDEX.md`](docs/DOC_INDEX.md).
Three audience tracks:

**Tier 1 — default paths**

| You need… | Open |
|:---|:---|
| Full map of markdown | [`docs/DOC_INDEX.md`](docs/DOC_INDEX.md) |
| Evidence / headline rules | [`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md) |
| Mis-readings we fixed | [`docs/COMMON_MISREADINGS.md`](docs/COMMON_MISREADINGS.md) |
| Binaries & CI matrix | [`docs/FEATURES_AND_STANDALONE_BUILDS.md`](docs/FEATURES_AND_STANDALONE_BUILDS.md) |
| Plain-language snapshot | [`docs/PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md`](docs/PARADIGM_SNAPSHOT_FOR_DRIVE_BY_READERS.md) |
| Figure & SVG rules | [`docs/VISUAL_INDEX.md`](docs/VISUAL_INDEX.md) |
| Push hygiene | [`docs/publish_checklist_creation_os.md`](docs/publish_checklist_creation_os.md) |

**Tier 2 — benchmarks, thesis, industry**

| Topic | Doc |
|:---|:---|
| Analysis / Planes A–C | [`docs/ANALYSIS.md`](docs/ANALYSIS.md) |
| `make bench` / §7 protocol | [`docs/BENCHMARK_PROTOCOL.md`](docs/BENCHMARK_PROTOCOL.md) |
| §1 – §26 evidence index | [`docs/MODULE_EVIDENCE_INDEX.md`](docs/MODULE_EVIDENCE_INDEX.md) |
| Thesis spine (RQ, threats, contributions) | [`docs/RESEARCH_AND_THESIS_ARCHITECTURE.md`](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md) |
| Repro bundle for published numbers | [`docs/REPRO_BUNDLE_TEMPLATE.md`](docs/REPRO_BUNDLE_TEMPLATE.md) |
| HDC / VSA ↔ engineering | [`docs/HDC_VSA_ENGINEERING_SUPERIORITY.md`](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md) |
| Glossary | [`docs/GLOSSARY.md`](docs/GLOSSARY.md) |
| Selective prediction formal framework | [`docs/selective_prediction.md`](docs/selective_prediction.md) |

**Tier 3 — silicon, remotes, governance**

| Topic | Doc |
|:---|:---|
| RTL mirror (SV, Chisel, Yosys, Rust, formal) | [`docs/RTL_SILICON_MIRROR.md`](docs/RTL_SILICON_MIRROR.md) |
| Formalism → silicon | [`docs/FULL_STACK_FORMAL_TO_SILICON.md`](docs/FULL_STACK_FORMAL_TO_SILICON.md) |
| σ stack map (v33 → v100 + HDL) | [`docs/SIGMA_FULL_STACK.md`](docs/SIGMA_FULL_STACK.md) |
| Mobile + messenger + legacy-app bindings | [`bindings/README.md`](bindings/README.md) |
| MCP σ server | [`docs/MCP_SIGMA.md`](docs/MCP_SIGMA.md) · `make check-mcp` |
| Git remotes | [`docs/CANONICAL_GIT_REPOSITORY.md`](docs/CANONICAL_GIT_REPOSITORY.md) |
| Contributing · security · agent rules | [`CONTRIBUTING.md`](CONTRIBUTING.md) · [`SECURITY.md`](SECURITY.md) · [`AGENTS.md`](AGENTS.md) |
| Maintainers + merge gate | [`docs/MAINTAINERS.md`](docs/MAINTAINERS.md) |
| English-only policy | [`docs/LANGUAGE_POLICY.md`](docs/LANGUAGE_POLICY.md) |
| Citation metadata | [`CITATION.cff`](CITATION.cff) · [`docs/CITATION.bib`](docs/CITATION.bib) |

Archived full README (pre-slim, narrative / diagram-heavy):
[`docs/README_FULL.md`](docs/README_FULL.md).  README slim plan /
future iterations: [`docs/README_REFACTOR_PLAN.md`](docs/README_REFACTOR_PLAN.md).

---

<a id="doctoral-and-committee-read-path"></a>

## Doctoral and committee read path

Read **in order** once before citing any number or narrative title
from this tree:

1. [`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md) — evidence
   classes, forbidden merges, falsifiers for the portable core.
2. [`docs/RESEARCH_AND_THESIS_ARCHITECTURE.md`](docs/RESEARCH_AND_THESIS_ARCHITECTURE.md) —
   RQ1 – RQ4, contributions C1 – C6, threats to validity, chapter
   outline, pre-defense gates.
3. [`docs/REPRO_BUNDLE_TEMPLATE.md`](docs/REPRO_BUNDLE_TEMPLATE.md) —
   minimum metadata when a metric leaves the lab.
4. [`docs/FEATURES_AND_STANDALONE_BUILDS.md`](docs/FEATURES_AND_STANDALONE_BUILDS.md) —
   which binary is which (`creation_os` vs `creation_os_v6` …
   `v12`), self-test counts, CI.
5. [`docs/MODULE_EVIDENCE_INDEX.md`](docs/MODULE_EVIDENCE_INDEX.md) —
   §1 – §26 in `creation_os_v2.c`: evidence class per section before
   you cite a module headline.
6. Scoped kernel docs for any line you cite from v6 – v12:
   [`LIVING_KERNEL_V6.md`](docs/LIVING_KERNEL_V6.md),
   [`HALLUCINATION_KILLER_V7.md`](docs/HALLUCINATION_KILLER_V7.md),
   [`PARAMETERS_IN_SILICON_V9.md`](docs/PARAMETERS_IN_SILICON_V9.md),
   [`THE_REAL_MIND_V10.md`](docs/THE_REAL_MIND_V10.md),
   [`THE_MATMUL_FREE_MIND_V11.md`](docs/THE_MATMUL_FREE_MIND_V11.md),
   [`THE_TENSOR_MIND_V12.md`](docs/THE_TENSOR_MIND_V12.md).
7. [`docs/ADVERSARIAL_REVIEW_CHECKLIST.md`](docs/ADVERSARIAL_REVIEW_CHECKLIST.md) —
   hostile review simulation before submission.

**Rule for dissertations:** v6 – v12 are **Lab demo (C)** appendices
with their own evidence-class headers; do not fold their toy outputs
into the same tables as §7 throughput without an explicit wall — see
`CLAIM_DISCIPLINE` §1.

<p align="center"><img src="docs/assets/kernel-lineage-evidence.svg" width="96%" alt="Portable proof vs standalone lab demos (evidence classes)" decoding="async" loading="lazy" style="max-width:min(920px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.09);"/></p>
<p align="center"><sub><strong>FIG 04</strong> — portable proof vs extended lab demos (evidence-class guardrail). <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

---

<a id="limitations"></a>

## Limitations

This is a research prototype.  Full list with scope and caveats:
**[`docs/limitations.md`](docs/limitations.md)**.  Short form:

- **σ is not a universal signal.**  Bonferroni-significant on
  TruthfulQA-class factual-confidence tasks; on HellaSwag and
  MMLU-eligible subjects entropy is the best signal (v111 matrix).
- **Conformal guarantee** is exchangeable-draw, finite-sample;
  distribution shift reverts the bound to empirical AURCC.
- **v6 – v29 extended kernels** are Lab demo (C) appendices with
  internal `self_test` consistency, not harness rows, tape-out, or
  trained LM reproduction.
- **Arithmetic vs throughput.**  192× ops and 32× RAM are arithmetic
  ratios at `D = 4096`.  Throughput requires `make bench` plus
  archived host metadata.
- **BitNet quickstart** downloads real 1.2 GB weights; the local
  runtime is real.  Cloud escalation is opt-in and off by default.

---

<a id="license"></a>

## License

Dual-licensed; the choice is **not** at your discretion — see
[`LICENSE`](LICENSE) §0 for which one binds you.  A third option
(paid Commercial License) is available **only** from the Licensor.

| Path | Cost | Document |
|:---|:---|:---|
| **Spektre Commercial Source License v1.0** (primary) | free for non-commercial | [`LICENSE-SCSL-1.0.md`](LICENSE-SCSL-1.0.md) |
| **GNU AGPL v3.0-only** (fallback after 4-yr Change Date, AGPL-derived portions) | free | [`LICENSE-AGPL-3.0.txt`](LICENSE-AGPL-3.0.txt) |
| **Commercial License** (closed-source / SaaS / OEM / Sovereign / Strategic) | paid | [`COMMERCIAL_LICENSE.md`](COMMERCIAL_LICENSE.md) |
| **Contributor License Agreement** | n/a | [`CLA.md`](CLA.md) |

**TL;DR**

- Private individuals · academia · non-profits · journalism ·
  reproducibility / security audits · 30-day commercial evaluation
  (under EUR 1 M revenue) → **FREE** under SCSL-1.0.
- For-profit > EUR 1 M revenue · hosted SaaS / model-as-a-service /
  agent-as-a-service (unless you publish the complete service-stack
  source per SCSL §5) · OEM closed-source redistribution → **paid
  Commercial License required**.
- All government / military / intelligence / law-enforcement
  **operational** use (SCSL §9.1(b)) → **DENIED at any price**; civilian
  Sovereign deployments by EU CFR / ECHR / ICCPR-bound states under
  SCSL §9.3.
- Sanctioned Persons (EU / UN / OFAC / UK HMT / Finland) and parties
  credibly accused of Aggression (Rome Statute Art. 8 *bis*) →
  **categorical denial** (SCSL §10).

**Sole holder of all paid commercial rights:** Lauri Elias Rainio
(ORCID [0009-0006-0903-8541](https://orcid.org/0009-0006-0903-8541))
and Spektre Labs Oy, jointly and severally.  No other person or
entity may grant a Commercial License; any attempted grant is void
*ab initio* (SCSL §4.3).

Every Receipt emitted by Creation OS carries the SHA-256 of
`LICENSE-SCSL-1.0.md` (SCSL §11).  The pinned reference hash lives
in [`LICENSE.sha256`](LICENSE.sha256) and is independently verifiable:

```bash
shasum -a 256 LICENSE-SCSL-1.0.md       # macOS
sha256sum LICENSE-SCSL-1.0.md           # POSIX
bash tools/license/license_sha256.sh    # bundled helper
make license-attest                     # full: 11 KAT + bundle + sample receipt
```

Full human-readable explainer: [`docs/LICENSING.md`](docs/LICENSING.md) ·
who-may-do-what matrix: [`docs/LICENSE_MATRIX.md`](docs/LICENSE_MATRIX.md) ·
trademark / patent notices: [`NOTICE`](NOTICE).

Lauri Elias Rainio · Spektre Labs Oy · Helsinki, Finland
ORCID: [0009-0006-0903-8541](https://orcid.org/0009-0006-0903-8541) ·
licensing: `spektre.labs@proton.me` · web: [spektrelabs.org](https://spektrelabs.org)

---

*2026 · Spektre Labs · Helsinki · Creation OS — coherence you can compile.*
