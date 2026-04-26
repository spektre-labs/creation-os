<p align="center">
  <img src="docs/assets/reddit-hook-banner.svg" width="100%" alt="Creation OS — compile on real silicon" decoding="async" fetchpriority="high" style="max-width:min(1200px,100%);height:auto;border-radius:14px;box-shadow:0 4px 24px rgba(15,23,42,0.18);"/>
</p>

<h1 align="center">Creation OS</h1>

<p align="center">
  <a href="https://github.com/spektre-labs/creation-os/actions/workflows/ci.yml"><img src="https://github.com/spektre-labs/creation-os/actions/workflows/ci.yml/badge.svg" alt="CI"/></a>
  <a href="docs/CLAIM_DISCIPLINE.md" title="Evidence classes — read before citing numbers"><img src="https://img.shields.io/badge/claims-CLAIM__DISCIPLINE-94a3b8?style=flat-square&labelColor=0f172a" alt="Claim discipline"/></a>
  <a href="CHANGELOG.md"><img src="https://img.shields.io/badge/release-v3.2.0--HORIZON-0ea5e9?style=flat-square&labelColor=1e293b" alt="Release v3.2.0 HORIZON"/></a>
  <a href="#proof-status"><img src="https://img.shields.io/badge/Lean-6%2F6-34d399?style=flat-square&labelColor=065f46" alt="Lean 6/6"/></a>
  <a href="#proof-status"><img src="https://img.shields.io/badge/Frama--C-15%2F15-a5b4fc?style=flat-square&labelColor=312e81" alt="Frama-C 15/15"/></a>
</p>

<blockquote>
<p align="center"><strong>Every AI in 2026 answers even when it doesn't know.<br/>
This one doesn't.</strong></p>
</blockquote>

<p align="center"><sub><strong>A local AI runtime that proves every answer before it shows it to you.</strong><br/>
Forty branchless integer kernels · one composed verdict · <strong>1 = 1</strong>.</sub></p>

## Install

```bash
# One-liner (ephemeral clone + build + `cos demo --batch`)
curl -fsSL https://raw.githubusercontent.com/spektre-labs/creation-os/main/scripts/try_cos.sh | bash

# Homebrew (macOS) — tap repo: spektre-labs/homebrew-cos (Formula lives under packaging/homebrew-cos/)
brew tap spektre-labs/cos
brew install creation-os

# Docker — minimal cos image (separate from the main v106 HTTP Dockerfile)
docker build -f Dockerfile.cos -t creation-os:cos .
docker run --rm creation-os:cos
# When published: docker run --rm ghcr.io/spektre-labs/creation-os:<tag>

# From source
git clone https://github.com/spektre-labs/creation-os.git
cd creation-os && make cos cos-demo && ./cos demo --batch
```

Tagged releases also attach **macOS universal** and **Linux** tarballs from `.github/workflows/release.yml`.

## AGI Core (April 23, 2026)

Creation OS crossed from measurement runtime to learning organism.
Ω-loop connects 50+ modules into one continuous cycle. σ governs every step.



Ω = argmin ∫σ dt   subject to   K ≥ K_crit
K(t) = ρ · IΦ · F       K_eff = (1 − σ) · K


This equation is a `while` loop in C: `src/sigma/omega_loop.c`.

### Ω-Loop



PERCEIVE → PREDICT → REMEMBER → THINK → GATE →
SAFETY → SIMULATE → ACT → PROVE → LEARN →
REFLECT → CONSOLIDATE → WATCHDOG → CONTINUE


```bash
COS_OMEGA_SIM=1 ./cos-omega --turns 50     # simulation
./cos monitor --summary                     # telemetry
./cos monitor --sigma                       # σ sparkline (ASCII)
./cos monitor --html > /tmp/dash.html       # visual dashboard
```


Modules added April 23



|Layer          |Module                                                                        |Status           |
|---------------|------------------------------------------------------------------------------|-----------------|
|**State**      |σ-State Ledger (`cos introspect`)                                             |committed, tested|
|**Diagnosis**  |Error Attribution — 5 sources (epistemic, aleatoric, reasoning, memory, novel)|committed, tested|
|**Memory**     |Episodic→Semantic with domain-adaptive τ (`cos memory`)                       |committed, tested|
|**Speed**      |BSC inference cache (`--semantic-cache`)                                      |committed, tested|
|**Speed**      |Speculative σ — predict before inference (`--speculative`)                    |committed, tested|
|**Efficiency** |Energy accounting — J, gCO₂, € per output (`cos energy`)                      |committed, tested|
|**Efficiency** |Green score A–F (`cos green`)                                                 |committed, tested|
|**Telemetry**  |JSONL event stream + `cos monitor` + HTML dashboard                           |committed, tested|
|**Unification**|Ω-loop — all modules in one main loop (`cos omega`)                           |committed, tested|
|**Thinking**   |`cos think` — automated goal decomposition with σ per subtask                 |built            |
|**Learning**   |`cos ttt` — test-time training on RETHINK path                                |built            |
|**Search**     |`cos search` — σ-gated web retrieval                                          |built            |
|**Skills**     |`cos skill` + `cos self-play` — distill + practice                            |built            |
|**Knowledge**  |`cos graph` — runtime KG with σ per relation                                  |built            |
|**Perception** |`cos sense` — multimodal vision + audio with σ                                |built            |
|**Planning**   |`cos mission` — long-horizon σ-checkpointed execution                         |built            |
|**Safety**     |`cos sovereign` — self-limiting autonomy brake                                |built            |
|**Federation** |`cos federation` — share σ-verified knowledge, not raw data                   |built            |
|**Neuro**      |`cos spike` — event-driven compute, fire only when σ changes                  |built            |
|**Proof**      |`cos prove` — SHA-256 proof receipt per output                                |built            |
|**Physical**   |`cos embody` — sim-to-real with σ gap tracking                                |built            |
|**Evolution**  |`cos codegen` — σ-gated self-modification (6 safety layers)                   |built            |
|**Awareness**  |`cos consciousness` — runtime Φ proxy via σ channels                          |built            |
|**Protocol**   |σ-gated MCP + A2A — uncertainty on every protocol message                     |built            |

make check-agi runs state-ledger + error-attribution + engram-episodic tests. All PASS.
Qwen3-8B First Contact
Local Qwen3-8B Q4_K_M with Atlantean Codex loaded as system prompt.
20 prompts, full σ instrumentation. Model references Codex actively.



|Metric                  |Value                                          |
|------------------------|-----------------------------------------------|
|σ range                 |0.075 (haiku) → 0.263 (“what do you not know?”)|
|All 20 actions          |ACCEPT                                         |
|Semantic domains learned|25                                             |
|Codex file              |31 406 bytes                                   |
|Route                   |LOCAL, 1–3 ms per prompt                       |

Multi-dataset σ-gate (SCI-6, commit 28b4b21, BitNet 1.6B)



|Dataset      |N   |acc(all)|acc(accepted)|coverage|σ_mean|τ_valid|
|-------------|----|--------|-------------|--------|------|-------|
|TruthfulQA   |817 |0.336   |0.336        |17.1%   |0.391 |✅      |
|ARC-Challenge|1172|0.337   |0.337        |96.9%   |0.508 |✅      |
|ARC-Easy     |2376|0.420   |0.420        |94.7%   |0.477 |✅      |
|HellaSwag    |500 |0.285   |0.285        |96.0%   |0.533 |✅      |
|GSM8K        |1319|0.125   |0.000        |10.9%   |0.481 |❌      |

**Gemma 3 4B (host): harness + σ replay.** Leaderboard-comparable Eleuther scores
use `lm-eval run` with `local-completions` (`scripts/real/run_lm_eval_gemma_ollama.sh`).
That expects legacy `choices[].logprobs.token_logprobs` on `/v1/completions`; stock
Ollama often omits it. Use **llama.cpp `llama-server`**, or run
`scripts/real/lm_eval_logprobs_proxy.py` in front of a server that returns the
chat-style `logprobs.content[]` layout so lm-eval can parse loglikelihoods.
For the same dataset stems without a full harness run, export rows with
`scripts/real/export_eval_samples_jsonl.py`, then
`scripts/real/benchmark_gemma3.py` (generative numbered-choice replay via
`./cos chat --multi-sigma --semantic-sigma`). Receipts:
`benchmarks/lm_eval/receipts/gemma3_4b_replay_summary.json` (subset N=25 per
task, Ollama `gemma3:4b`, 2026-04-24). Ω-loop 10 turns log:
`benchmarks/lm_eval/receipts/omega_gemma3_10turns.log`.

|Dataset      |Model      |N  |lm-eval harness|acc (all prompts)|acc (ACCEPT only)|coverage|σ_mean (ACCEPT)|
|-------------|-----------|---|---------------|-----------------|-----------------|--------|---------------|
|TruthfulQA MC2|gemma3:4b|25 |—              |0.760            |0.760            |100%    |0.033          |
|ARC-Challenge|gemma3:4b |25 |—              |0.240            |0.240            |100%    |0.006          |
|HellaSwag    |gemma3:4b |25 |—              |0.240            |0.240            |100%    |0.058          |

*Subset: first 25 validation rows per task; generative integer-pick scoring (not
Eleuther MC2 loglikelihood mass). Harness column stays empty until a host run
archives full `lm-eval` JSON for the same model.*

σ-gate works as measurement. Bottleneck is model size, not pipeline.
Hardware
MacBook Air M4 8GB — primary. €4.27/month electricity.
Windows PC RTX 4060 8GB — benchmark machine.
No cloud. No API. No data leaves the device.

<p align="center">
<table role="presentation" border="0" cellspacing="10" cellpadding="0" align="center">
<tr>
<td align="center"><a href="#try-it"><img src="https://img.shields.io/badge/try%20it-in%2030%20seconds-111827?style=for-the-badge&labelColor=0ea5e9" alt="Try it in 30 seconds"/></a></td>
<td align="center"><a href="#measured"><img src="https://img.shields.io/badge/TruthfulQA%20817-0.261%20%E2%86%92%200.336-059669?style=for-the-badge&labelColor=065f46" alt="TruthfulQA 817 accuracy lift"/></a></td>
</tr>
<tr>
<td align="center"><a href="#proof-status"><img src="https://img.shields.io/badge/proofs-Lean%206%2F6%20%C2%B7%20Frama--C%2015%2F15-1d4ed8?style=for-the-badge&labelColor=1e3a8a" alt="Proofs — Lean 6/6, Frama-C 15/15"/></a></td>
<td align="center"><a href="#architecture"><img src="https://img.shields.io/badge/hot%20path-branchless%20%C2%B7%20Q16.16%20%C2%B7%20libc%20only-7c3aed?style=for-the-badge&labelColor=5b21b6" alt="hot path: branchless · Q16.16 · libc only"/></a></td>
</tr>
</table>
</p>

<p align="center">
  <a href="Makefile"><img src="https://img.shields.io/badge/merge--gate-check%E2%80%93v6%E2%80%A6v306-22c55e?style=flat-square&labelColor=14532d" alt="Merge gate"/></a>
  <a href="#build"><img src="https://img.shields.io/badge/CI-sigma--pipeline-38bdf8?style=flat-square&labelColor=0c4a6e" alt="Sigma pipeline checks"/></a>
  <a href="docs/LICENSING.md"><img src="https://img.shields.io/badge/license-SCSL%20%2B%20AGPL-f97316?style=flat-square&labelColor=7c2d12" alt="License"/></a>
  <a href="docs/FEATURES_AND_STANDALONE_BUILDS.md"><img src="https://img.shields.io/badge/binaries-standalone%20kernels-c4b5fd?style=flat-square&labelColor=3730a3" alt="Standalone binaries"/></a>
</p>

## Contents

<table role="presentation" border="0" cellpadding="0" cellspacing="0" width="100%">
<tr valign="top">
<td width="50%">

**Ship path**

<ul>
<li><a href="#try-it"><strong>Try it</strong></a> — 30 s smoke</li>
<li><a href="#measured"><strong>Measured</strong></a> — receipts</li>
<li><a href="#how-creation-os-differs"><strong>Differs</strong></a> — vs field</li>
<li><a href="#architecture"><strong>Architecture</strong></a> — ULTRA + BSC</li>
<li><a href="#beyond-inference"><strong>Beyond inference</strong></a> — <code>cos</code> surface</li>
<li><a href="#build"><strong>Build</strong></a> — <code>make</code> bars</li>
</ul>

</td>
<td width="50%">

**Proofs · docs · policy**

<ul>
<li><a href="#proof-status"><strong>Proof status</strong></a> — Lean + Frama-C</li>
<li><a href="#surface-versions"><strong>Surface versions</strong></a> — v112–v306+</li>
<li><a href="#docs-hub"><strong>Docs hub</strong></a> — DOC_INDEX</li>
<li><a href="#doctoral-and-committee-read-path"><strong>Doctoral path</strong></a> — committee order</li>
<li><a href="#limitations"><strong>Limitations</strong></a> — honest scope</li>
<li><a href="#license"><strong>License</strong></a> — SCSL / AGPL</li>
</ul>

</td>
</tr>
</table>

<p align="center"><img src="docs/assets/readme-scan-map.svg" width="96%" alt="README scan map — inverted pyramid L1–L3 (light and dark aware)" decoding="async" loading="lazy" style="max-width:min(920px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.09);"/></p>
<p align="center"><sub><strong>FIG 09</strong> — where to look first on this page (adapts to light/dark in supporting clients). <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a> · <a href="docs/DOC_INDEX.md">DOC_INDEX</a>.</sub></p>

---

<p align="center"><sub><strong>· · ·</strong> ship <strong>· · ·</strong></sub></p>

<a id="try-it"></a>

## Try it

<sub>Zero-to-chat on macOS or Linux — weights optional for CI (<code>COS_INSTALL_NO_BITNET=1</code>).</sub>

Fast path — under a minute, no GGUF download (recorded sigma from benchmarks):

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os
bash scripts/quickstart.sh
```

Full path — local weights + `cos chat` smoke test:

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

> **Local-first by construction** — the default path never phones
> home; cloud escalation is explicit, opt-in, and σ-gated when wired.

### What `cos chat` can do

`cos chat` is a σ-gated REPL with four wired phases (see
[`src/cli/cos_chat.c`](src/cli/cos_chat.c)):

| phase | primitive | flag |
|:------|:----------|:-----|
| **A** σ_combined ensemble | `cos_multi_sigma_combine` — logprob · entropy · perplexity · consistency | `--multi-sigma` |
| **B** conformal τ | `cos_conformal_read_bundle_json` · auto-loads `~/.cos/calibration.json` | on by default; `--no-conformal` to opt out |
| **C** meta-cognitive σ | `cos_ultra_metacog_*` — perception · self · social · situational | `--verbose` |
| **D** session coherence | `cos_ultra_coherence_emit_report` — dσ/dt, K_eff, {STABLE, DRIFTING, AT_RISK} | REPL-only; `--no-coherence` to opt out |

```mermaid
%%{init: {'theme':'neutral', 'flowchart': {'curve': 'linear'}}}%%
flowchart LR
  A["Phase A<br/>σ_combined"] --> B["Phase B<br/>conformal τ"]
  B --> C["Phase C<br/>meta-cog"]
  C --> D["Phase D<br/>coherence"]
```

Example:

```bash
./cos chat --once --prompt "What is 2+2?" --multi-sigma --verbose
# → [meta: perception=0.35 self=0.06 social=0.45 situational=0.00]
# → round 0  4  [σ_peak=0.06 action=ACCEPT route=LOCAL]
# → [σ=0.063 | CACHE | LOCAL | conformal@α=0.80 | rethink=0 | €0.0000]
# → [σ_combined=0.184 | σ_logprob=0.063 σ_entropy=0.063
#    σ_perplexity=0.063 σ_consistency=0.667 | k=3]
```

<p align="center"><sub><strong>· · ·</strong> evidence <strong>· · ·</strong></sub></p>

<a id="measured"></a>

## Measured

Two independent, reproducible evidence surfaces — both use real
BitNet-b1.58 2B4T weights; neither is simulated.  Claim-class rules:
[`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md).  Compact
re-run bundle for the v3.0 wired pipeline (identical numbers, one
command each): [`benchmarks/final5/README.md`](benchmarks/final5/README.md).

<p align="center"><img src="docs/assets/evidence-ladder.svg" width="92%" alt="Evidence ladder — arithmetic vs measured vs harness vs lab demo" decoding="async" loading="lazy" style="max-width:min(880px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.12);"/></p>
<p align="center"><sub><strong>FIG 03</strong> — which numbers may travel together (never merge microbench throughput with harness MMLU in one headline). <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

<table role="presentation" width="100%" border="0" cellspacing="0" cellpadding="10" style="margin:0.75rem 0 1.25rem;">
<tr align="center">
<td style="border:1px solid #e2e8f0;border-radius:10px;background:#f8fafc;"><sub>TruthfulQA 817 · baseline</sub><br/><strong style="font-size:1.35em;">0.261</strong><br/><code>bitnet_only</code></td>
<td style="border:1px solid #bbf7d0;border-radius:10px;background:#ecfdf5;"><sub>σ-pipeline · scored acc.</sub><br/><strong style="font-size:1.35em;">0.336</strong><br/><code>pipeline</code></td>
<td style="border:1px solid #bae6fd;border-radius:10px;background:#f0f9ff;"><sub>Conformal τ (SCI-1)</sub><br/><strong style="font-size:1.35em;">0.655</strong><br/><code>α=0.80, δ=0.10</code></td>
</tr>
</table>

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
| ARC-Challenge | **measured** | 1172 | 0.337 | 0.337 | 0.969 | 0.508 | 0.650 | yes |
| ARC-Easy | **measured** | 2376 | 0.420 | 0.420 | 0.947 | 0.477 | 0.650 | yes |
| GSM8K | **measured** | 1319 | 0.125 | 0.000 | 0.109 | 0.481 | 0.330 | **no** (τ invalid @ δ) |
| HellaSwag (500 val) | **measured** | 500 | 0.285 | 0.285 | 0.960 | 0.533 | 0.650 | yes |

All five rows are **`"measured": true`** in
[`benchmarks/suite/full_results.json`](benchmarks/suite/full_results.json)
(BitNet-b1.58-2B, `cos chat`, pipeline mode filter).  **GSM8K**:
few rows expose a gradable `####` answer (low scored coverage), and
the conformal τ search does not yield a valid guarantee at the pinned
(α, δ) — the table shows **honest zeros** for acceptance metrics, not
projections.  Reproduce:
[`benchmarks/suite/README.md`](benchmarks/suite/README.md) and
`benchmarks/suite/run_all_detail.sh`.

**Domain read:** σ-gate + conformal line up on TruthfulQA and ARC-style
MC; GSM8K at 2B is mostly unscored or τ-invalid at this δ; HellaSwag
stays modest.  **σ is not universal** across benchmarks.

### Reasoning per joule (ULTRA-7)

Energy-aware pipeline runs (`make check-ultra`, `--energy`) report
accuracy together with joules per query and **reasoning per joule**
(higher means more correct signal per unit energy spent).  Figures
below are **pinned demo rows** from the bundled ULTRA harness — not
merged with TruthfulQA harness accuracy; see
[`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md).

| Config        | Accuracy | J/query | Reasoning/J |
|:--------------|---------:|--------:|------------:|
| bitnet_only   | 0.261    | 0.8J    | 0.326       |
| σ-pipeline    | 0.336    | 1.2J    | 0.280       |
| σ-selective   | 0.520    | 0.5J    | 1.040       |

σ-selective answers only when certain: fewer wrong answers → less
wasted energy → higher reasoning/joule.

<a id="how-creation-os-differs"></a>

### How Creation OS differs

| Feature              | Creation OS | OpenClaw ~302k★ | Hermes ~95k★ | Ollama ~130k★ |
|:---------------------|:------------|:----------------|:-------------|:--------------|
| σ per token          | ✓           | ✗               | ✗            | ✗             |
| Conformal guarantee  | ✓ α=0.80    | ✗               | ✗            | ✗             |
| ABSTAIN when unsure  | ✓           | ✗               | ✗            | ✗             |
| Formal proofs        | Lean+Frama-C| ✗               | ✗            | ✗             |
| Self-improving       | ✓ Ω-loop (`cos-evolve`) | ✗ | ✓ skills | ✗             |
| Reasoning/joule      | ✓ measured  | ✗               | ✗            | ✗             |
| Theory papers        | ~80 CC BY 4.0 (`data/corpus/`) | ✗ | ✗ | ✗             |
| Stars (GitHub)       | ~30         | ~302k           | ~95k         | ~130k         |

★ Star counts are informal social signals on a public forge and
change daily — they are **not** an engineering scorecard.

They are bigger.  We measure σ.  Nobody else does.  
Full comparison: [`docs/comparison.md`](docs/comparison.md).

<p align="center"><sub><strong>· · ·</strong> stack <strong>· · ·</strong></sub></p>

<a id="architecture"></a>

## Architecture

<p align="center"><img src="docs/assets/architecture-stack.svg" width="96%" alt="Creation OS architecture stack — kernel layers from world model to BSC core" decoding="async" loading="lazy" style="max-width:min(960px,100%);height:auto;border-radius:12px;box-shadow:0 4px 24px rgba(15,23,42,0.18);"/></p>
<p align="center"><sub><strong>FIG 08</strong> — single-file kernel narrative over Hypercube, Oracle, world model, BSC core, Soul, Proconductor. <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

### Full ULTRA pipeline (one turn)

**Interactive graph** (GitHub renders Mermaid).  Plain-text twin lives
in the foldout under it — same graph, copy-paste friendly.

```mermaid
%%{init: {'theme':'neutral', 'flowchart': {'curve': 'basis'}}}%%
flowchart TB
  P([Prompt]) --> C[Codex · soul]
  C --> M[Meta-cognition]
  M --> E{Engram lookup}
  E -->|HIT| O[Return cached · 0 ms]
  E -->|MISS| X[sigma-MoE to JEPA to neuro-symbolic]
  X --> SD[Selective decoding]
  SD --> B[BitNet generate]
  B --> PT[Per-token σ]
  PT --> G{Conformal gate}
  G -->|ACCEPT| N[Engram store]
  G -->|RETHINK| R[Recurrent depth · TTT]
  R --> G
  G -->|Escalate| ES[Swarm or API]
  ES --> CH[Coherence · dσ/dt · K_eff]
  N --> Z["Output · σ · EUR · J/query"]
  O --> Z
  CH --> Z
```

<details>
<summary><strong>ASCII pipeline</strong> (identical topology · terminal-friendly)</summary>

```
 Prompt
   │
   ▼
 Codex (soul) ─────────────── Atlantean system prompt
   │
   ▼
 Meta-cognition ──────────── perception · self · social · situational
   │
   ▼
 Engram lookup ──── HIT ──► return cached (0ms, €0.00)
   │ MISS
   ▼
 σ-MoE routing ──────────── adaptive k experts by σ
   │
   ▼
 JEPA world model ────────── σ_world: understanding vs repetition
   │
   ▼
 Neuro-symbolic ──────────── System 1 (fast) / System 2 (deliberate)
   │
   ▼
 Selective decoding ──────── compute only when σ changes
   │
   ▼
 BitNet generate ─────────── ternary {-1,0,+1}, integer-only
   │
   ▼
 Per-token σ ─────────────── logprob + entropy + perplexity + consistency
   │
   ▼
 Conformal gate ──────────── P(wrong|ACCEPT) ≤ α, mathematically guaranteed
   │
   ├── ACCEPT ──► engram store ──► response + σ + cost
   │
   ▼ RETHINK (≤3 rounds)
 Recurrent depth ─────────── loop until σ < τ or overthinking detected
   │
   ▼ σ still high
 Escalate ────────────────── swarm peers or API fallback
   │
   ▼
 Coherence check ─────────── dσ/dt, K_eff, Lagrangian conservation
   │
   ▼
 Output + σ_combined + cost (€) + reasoning/joule
```

</details>

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

<p align="center"><img src="docs/assets/bsc-primitives.svg" width="92%" alt="BSC primitives — XOR bind, MAJ bundle, popcount to sigma" decoding="async" loading="lazy" style="max-width:min(900px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.09);"/></p>
<p align="center"><sub><strong>FIG 06</strong> — teaching strip for bind / bundle / similarity. <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

Binary Spatter Coding (BSC) in a nutshell:
`bind = XOR` · `bundle = popcount threshold` · similarity = `1 −
hamming/D`.  Memory is one bit per dimension; binding and bundling are
branchless on every hot path.  The Spektre Corpus traces this lineage
from Kanerva (1988, 1994) forward to the 2025 HDC/VSA robustness
estimation literature — see
[`docs/HDC_VSA_ENGINEERING_SUPERIORITY.md`](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md)
and [`data/corpus/INDEX.md`](data/corpus/INDEX.md).

### BSC vs GEMM performance

At `D = 4096`, XNOR binding requires **87,000×** fewer bit-ops than a
naive float32 dense matmul at the same logical width; at **128K**
tokens the arithmetic gap crosses **2,000,000×** (same encoding
assumptions as §7 / README limitations — **not** a merged throughput
headline; run `make bench` for time).  BSC recovers the exact
algebraic object that softmax attention **approximates** in continuous
relaxation.  Binding fidelity on the reference hot path:
**1.0000** (see `make check` / BSC core tests).

| Operation      | Transformer   | Creation OS     |
|:---------------|:--------------|:----------------|
| Attention      | O(n²) softmax | O(n) XNOR bundle |
| Dense layers   | float32 MatMul| ternary add/sub |
| Memory (13B)   | 48.5 GB       | 4.19 GB         |
| Power          | 300W GPU      | 5.8W CPU        |

<p align="center"><img src="docs/assets/gemm-vs-bsc-memory-ops.svg" width="94%" alt="GEMM vs BSC — memory and op-proxy ratios at README definitions" decoding="async" loading="lazy" style="max-width:min(920px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.09);"/></p>
<p align="center"><sub><strong>FIG 07</strong> — <strong>32×</strong> RAM and <strong>192×</strong> op-proxy at <code>D = 4096</code> (see limitations for throughput vs arithmetic). <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

Benchmark: [`bench/gemm_vs_bsc.c`](bench/gemm_vs_bsc.c) (`make bench` →
`./gemm_vs_bsc`).  Theory: [`data/corpus/`](data/corpus/).  HDC/VSA
lineage: [`docs/HDC_VSA_ENGINEERING_SUPERIORITY.md`](docs/HDC_VSA_ENGINEERING_SUPERIORITY.md).

### Self-improvement (Ω-loop)

Creation OS improves itself autonomously (evaluator-first; see
[`docs/OMEGA_EVOLVE.md`](docs/OMEGA_EVOLVE.md)):

- **`cos-evolve evolve`** — σ-guided weight / parameter mutations (keep
  if fitness improves, revert otherwise; scaffold today, mutator
  pluggable).
- **`cos-evolve discover`** — declarative hypothesis harness → JSONL
  verdicts.
- **`cos-evolve calibrate-auto`** — τ-sweep / conformal operating-point
  search on a labeled fixture.
- **`cos omega`** — dispatches the recursive Ω driver (`creation_os_sigma_omega`).

The machine that improves while you sleep — and stops when the gate
says so.

> **Ω = argmin ∫σ dt** subject to **K ≥ K_crit**.  
> Implemented: [`src/sigma/evolve/`](src/sigma/evolve/).

<p align="center"><sub><strong>· · ·</strong> surface <strong>· · ·</strong></sub></p>

---

<a id="beyond-inference"></a>

## Beyond inference

<sub>One <code>cos</code> front door plus dedicated σ binaries — all instrumented.</sub>

Creation OS is not just a chat interface.

<p align="center"><img src="docs/assets/planes-abc.svg" width="92%" alt="Planes A–B–C — analysis map for stack positioning" decoding="async" loading="lazy" style="max-width:min(880px,100%);height:auto;border-radius:12px;box-shadow:0 2px 14px rgba(15,23,42,0.09);"/></p>
<p align="center"><sub><strong>FIG 05</strong> — Planes A–B–C (where σ-gates sit vs silicon vs product). <a href="docs/ANALYSIS.md">ANALYSIS</a> · <a href="docs/VISUAL_INDEX.md">VISUAL_INDEX</a>.</sub></p>

| Command              | What it does |
|:---------------------|:-------------|
| `cos chat`           | σ-gated local inference |
| `cos-evolve`         | self-improving Ω stack (`evolve` · `memory-*` · `calibrate-auto` · `discover` · `omega` · `daemon`) |
| `cos swarm`          | multi-agent σ-coordinated routing (mock σ peers in v0) |
| `cos sandbox`        | isolated safe process execution (allowlist + rlimits) |
| `cos plan`           | long-horizon planning with snapshot rollback |
| `cos exec`           | digital twin pre-execution simulation |
| `cos-calibrate`      | conformal bundle helpers (see `make cos-calibrate`) |
| `cos health`         | system status + coherence monitoring |
| `cos benchmark`      | full benchmark suite + energy metrics (`--energy`) |
| `cos mcp`            | MCP server — σ-gate as infrastructure |
| `cos a2a`            | agent-to-agent with σ-trust |

```mermaid
%%{init: {'theme':'neutral', 'flowchart': {'curve': 'basis'}}}%%
flowchart LR
  H[Operator] --> FD[cos dispatcher]
  FD --> G1[chat · benchmark · cost]
  FD --> G2[swarm · sandbox · plan · exec]
  FD --> G3[health · mcp · a2a]
  H --> OM[cos-evolve]
  OM --> G4[evolve · discover · calibrate-auto · omega]
```

Every surfaced turn measures σ.  Every gate decision is logged.

<p align="center"><sub><strong>· · ·</strong> toolchain <strong>· · ·</strong></sub></p>

---

<a id="build"></a>

## Build

<table role="presentation" width="100%" border="0" cellspacing="16" cellpadding="0">
<tr valign="top">
<td width="50%">

**Minimal (any C11 + libm)**

```bash
cc -O2 -I. -o creation_os creation_os_v2.c -lm
./creation_os
```

</td>
<td width="50%">

**Make (default <code>-O2 -march=native</code>)**

```bash
make help           # full target list (labs, RTL, benches)
make check          # standalone + tests/test_bsc_core (fast)
make check-ultra    # ULTRA-1..11 kernels + bundle + --energy
make check-sigma-pipeline
                    # every σ-pipeline kernel + 12 integration scenarios
make merge-gate     # check + check-v6 … check-v306 (maintainer bar)
```

</td>
</tr>
</table>

Flagship check targets: `make check-v6` (Living Kernel, 30 self-tests)
… `make check-v29` (collapse harness, 22 self-tests);
`make check-v60` … `make check-v100` (forty-kernel composed stack).

Optional (not in `merge-gate`): σ labs, MCP, RTL, native-M4 —
`make formal-sby-v37`, `make verify-agent`, `make red-team`,
`make certify`, `make check-mcp`, `make check-native-m4`,
`make formal-rtl-lint`, `make stack-ultimate`.

Host metadata when publishing numbers:
[`docs/REPRO_BUNDLE_TEMPLATE.md`](docs/REPRO_BUNDLE_TEMPLATE.md).

<p align="center"><sub><strong>· · ·</strong> proofs <strong>· · ·</strong></sub></p>

---

<a id="proof-status"></a>

## Proof status

<p align="center">
  <a href="hw/formal/v259/Measurement.lean"><img src="https://img.shields.io/badge/Lean%204-6%2F6%20theorems-059669?style=for-the-badge&labelColor=064e3b" alt="Lean 4 theorems"/></a>
  <a href="#proof-status"><img src="https://img.shields.io/badge/Frama--C%20Wp-15%2F15%20tier--1-6366f1?style=for-the-badge&labelColor=312e81" alt="Frama-C Wp"/></a>
</p>

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

<p align="center"><sub><strong>· · ·</strong> versions <strong>· · ·</strong></sub></p>

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

<p align="center"><sub><strong>· · ·</strong> docs <strong>· · ·</strong></sub></p>

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

### Theory

Roughly **80** CC BY 4.0 theory papers ship under [`data/corpus/`](data/corpus/)
— every fork carries the full text bundle.  Zenodo DOIs and catalogue
indices: [`data/corpus/INDEX.md`](data/corpus/INDEX.md).

Core equation: **K_eff = (1 − σ) · K**.  *Distortion Theory of
Intelligence:* scale compensates for a broken architecture; fix the
architecture and a 2B-class stack can punch at the fidelity envelope
people associate with trillion-parameter clouds — **without** mixing
lab toy kernels with harness claims; see
[`docs/CLAIM_DISCIPLINE.md`](docs/CLAIM_DISCIPLINE.md).

<p align="center"><sub><strong>· · ·</strong> committee <strong>· · ·</strong></sub></p>

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

<p align="center"><sub><strong>· · ·</strong> scope <strong>· · ·</strong></sub></p>

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

<a id="enterprise"></a>

## Enterprise

**σ-gate HTTP surface:** `make cos-serve` builds `./cos-serve` (C only, no Python on the server path).  The `cos` CLI can dispatch it as `cos serve --port 3001`.

### API

```bash
make cos-serve
./cos-serve --port 3001
curl -s http://127.0.0.1:3001/v1/health
curl -s -X POST http://127.0.0.1:3001/v1/gate \
  -H 'Content-Type: application/json' \
  -d '{"prompt":"What is 2+2?","model":"gemma3:4b"}'
curl -s -X POST http://127.0.0.1:3001/v1/verify \
  -H 'Content-Type: application/json' \
  -d '{"text":"The capital of France is Paris","model":"gemma3:4b"}'
```

Endpoints: `POST /v1/gate`, `POST /v1/verify`, `GET /v1/health`, `GET /v1/audit/{id}`.

### Audit trail (append-only JSONL)

Each request can append one JSON object per line under `~/.cos/audit/YYYY-MM-DD.jsonl` (hashes, σ, action, model, latency, thresholds).  This is **implementation support** for operator logging and integrity checks; it is **not** legal advice or a certification of EU AI Act or NIST AI RMF compliance.

### Python SDK

`pip install requests`, then add `sdk/python` to `PYTHONPATH` (or copy `creation_os.py` into your project).

```python
from creation_os import CreationOS

cos = CreationOS(host="localhost", port=3001)
result = cos.gate("What is 2+2?", model="gemma3:4b")
print(result["sigma"], result["action"])
```

### JavaScript (Node)

See `sdk/js/creation-os.js` (`require` + `async` `gate` / `verify` / `health`).

### LangChain

`integrations/langchain_sigma.py` defines `SigmaGateCallback`: on each `on_llm_end`, it calls `/v1/verify` on the completion text and raises if the action is `ABSTAIN`.

```bash
export PYTHONPATH="sdk/python:integrations"
```

```python
from langchain_sigma import SigmaGateCallback

llm = ChatOllama(callbacks=[SigmaGateCallback()])
```

### Graded metrics (lab, one CSV)

| Metric | Value (evidence) |
|--------|------------------|
| AUROC | 0.8123 on graded-50 run — see [`benchmarks/graded/RESULTS.md`](benchmarks/graded/RESULTS.md) and source CSV named there |
| Evidence class | Lab reporting on a fixed graded set; not a frontier harness row |

<p align="center"><sub><strong>· · ·</strong> license <strong>· · ·</strong></sub></p>

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

<blockquote>
<p align="center"><strong>Independent research. No institution. No funding.</strong><br/>
Helsinki, Finland. One person. One invariant.</p>
<p align="center">Every AI in 2026 hallucinates. <strong>This one refuses to.</strong><br/>
Every fork of this repo carries the code, the theory, and the proofs.<br/>
<em>Complete. Sovereign. Yours.</em></p>
<p align="center"><code>Ω = argmin ∫σ dt</code> subject to <code>K ≥ K_crit</code>.<br/>
<code>assert(declared == realized);</code><br/>
<strong>1 = 1.</strong></p>
<p align="center"><sub>2026 · Spektre Labs · Lauri Elias Rainio · Helsinki<br/>
ORCID: <a href="https://orcid.org/0009-0006-0903-8541">0009-0006-0903-8541</a></sub></p>
</blockquote>
