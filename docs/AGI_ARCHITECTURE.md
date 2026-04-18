# Creation OS — AGI Architecture

_Stable reference for the six-layer stack that turns a 1.58-bit base model
into a σ-governed, measured, distributable local AGI._

This document consolidates the architectural claims previously scattered
across the v60–v111 per-kernel READMEs.  It is the canonical map; the
per-kernel pages under `docs/v71..v100/` and `docs/v101..v111/` remain
the authoritative spec for their own numbered layer.

## Diagram

![Six-layer σ-stack](./AGI_ARCHITECTURE.svg)

If the SVG does not render in your Markdown viewer, the plain-text
version of the same diagram is in `docs/AGI_ARCHITECTURE.txt`.

## The six layers

### 1. Silicon layer  —  RTL + formal equivalence
Authoritative directory: `rtl/`, `hw/yosys/`, `hw/formal/`.

| responsibility                          | component(s)                                |
|-----------------------------------------|---------------------------------------------|
| Matmul-free bit-serial HDC operations   | `rtl/bind_xor.sv`, `rtl/bundle_majority.sv` |
| Formal equivalence vs golden reference  | `hw/formal/*.sby`, `make formal-rtl-lint`   |
| Synthesis-adjacent smoke (Yosys OSS)    | `make yosys-elab`                           |

This layer is a _claim about what silicon the stack is ultimately
heading to_; today the build targets stay Lab-demo in C on commodity
CPUs (`creation_os_v2.c`, `src/v6..v29/`).

### 2. Generation layer  —  BitNet 1.58-bit bridge
Authoritative directory: `src/v101/`, `third_party/bitnet/`.

| API                                  | where to look                                  |
|--------------------------------------|------------------------------------------------|
| `cos_v101_bridge_loglikelihood_v105` | `src/v101/bridge.h`                            |
| `cos_v101_bridge_generate`           | `src/v101/bridge.h`                            |
| Eight σ-channels per next-token dist | `src/v101/sigma_channels.c`                    |
| Default aggregator = geometric mean  | `src/v101/sigma_channels.c` (v105 swap)        |

The bridge is the only code in the tree that talks to `llama.cpp`.  All
upstream layers consume σ profiles and text, never raw logits.

### 3. σ-Governance layer  —  abstention, τ-sweep, operator search
Authoritative directory: `benchmarks/v103/`, `benchmarks/v104/`,
`benchmarks/v111/`.

| responsibility                                 | artefact                                        |
|------------------------------------------------|-------------------------------------------------|
| τ-sweep on RCC + AUACC                         | `benchmarks/v103/compute_rc_metrics.py`         |
| 10-operator pre-registered search              | `benchmarks/v104/compute_operator_rcc.py`       |
| 4 tasks × 6 signals, Bonferroni 24             | `benchmarks/v111/frontier_matrix.py`            |
| Default aggregator wire-format (geometric mean)| `src/v101/sigma_channels.c`                     |

v111 is the first layer in the tree that publishes a frontier-parity
matrix.  Two signals (`sigma_max_token`, `sigma_n_effective`) beat the
entropy baseline at α = 0.05 / 24 on TruthfulQA MC2 (n=5000).

### 4. Reasoning layer  —  v62 / v64 / v111.2 σ-Reason
Authoritative directory: `src/v62/`, `src/v64/`, `src/v111/`.

| responsibility                                        | artefact              |
|-------------------------------------------------------|-----------------------|
| Energy-based path-consistency gate                    | `src/v62/fabric.c`    |
| MCTS-style candidate ranking heuristic                | `src/v64/intellect.c` |
| Introspection / waypoint-match check                  | `src/v45/introspection.c` |
| `POST /v1/reason` endpoint wiring all four layers     | `src/v111/reason.c`   |
| Abstain on undifferentiated σ margin                  | `src/v111/reason.c`   |

The reasoning layer does not add weights.  It composes existing v101
generations with multi-candidate σ-selection and a waypoint-match
introspection probe to either choose the most-confident candidate or
abstain with a structured diagnostic.

### 5. Training layer  —  σ-supervised SFT / LoRA
Authoritative directory: `training/v111/`, `models/v111/`.

| responsibility                                        | artefact                                              |
|-------------------------------------------------------|-------------------------------------------------------|
| σ-label extraction from v103/v104 sidecars            | `training/v111/extract_sigma_training_data.py`        |
| MLX-LM LoRA wrapper (TinyLlama / Phi-2 base)          | `training/v111/train_sigma_abstain.py`                |
| Default adapter output path                           | `models/v111/sigma_abstain_lora/`                     |
| Dry-run smoke (merge-gate safe)                       | `training/v111/check_v111_sft_smoke.sh`               |

Training is σ-self-distillation: the model's own uncertainty tells us
_which_ prompts to label `"I don't know."` and _which_ answers to reuse
as supervision.  There is no external grader, no human label, no extra
dataset.  The loss is vanilla causal-LM next-token — the σ supervision
lives entirely in the data.

### 6. Distribution layer  —  installer, server, web UI
Authoritative directory: `packaging/`, `src/v106/`, `web/`.

| responsibility                                        | artefact                                              |
|-------------------------------------------------------|-------------------------------------------------------|
| OpenAI-compatible HTTP (chat / completions / reason)  | `src/v106/server.c`, `src/v111/reason.c`              |
| Homebrew formula (stable tap)                         | `packaging/homebrew/cos.rb`                           |
| Curl install script                                   | `packaging/install.sh`                                |
| Docker image build                                    | `packaging/Dockerfile`                                |
| Release workflow (macOS arm64 / x86_64 + Linux)       | `.github/workflows/release.yml`                       |
| σ-channel web UI                                      | `web/index.html`, `web/app.js`, `web/app.css`         |

The distribution layer is how the stack becomes a _product_ — a single
command installs the server, points it at a GGUF, and exposes the entire
σ-stack over HTTP and a zero-build web UI.

## How the layers compose

### Inference flow
```
    prompt  ─►  v106 server
                   │
                   ▼
            v111.2  /v1/reason  (multi-candidate, σ-ranking, waypoint)
                   │
                   ▼
            v101 bridge         (BitNet 1.58-bit generation + σ profile)
                   │
                   ▼
            v29 / v101 σ-aggregator  (sigma_product default, v105+)
                   │
                   ▼
    {text, sigma_profile[8], sigma_product, abstained, waypoint_match}
```

### Training feedback flow
```
    v104 n=5000 sidecars   ──►   v111.3 extract_sigma_training_data.py
                                                   │
                                                   ▼
                                       JSONL {prompt, completion}
                                       label ∈ { idk | answer }
                                                   │
                                                   ▼
                                       v111.3 MLX LoRA adapter
                                       models/v111/sigma_abstain_lora
                                                   │
                                                   ▼
                                       v101 bridge --adapter  (v109 extension)
```

## Frontier-model gap (honest inventory)

See `docs/ROADMAP_TO_AGI.md` for the living list.  As of v111:

**Present and measured**
- next-token generation on a real multi-billion-parameter quantised model
- selective-prediction calibration with measured Bonferroni significance
- multi-candidate reasoning with waypoint-match introspection
- σ-supervised SFT pipeline (data extraction + LoRA recipe; live training
  is an optional step, not a merge-gate dependency)
- OpenAI-compatible server + web UI + installer

**Missing, documented in roadmap, not in v111**
- pretraining loop (Creation OS consumes BitNet; it does not train it)
- RLHF / DPO / constitutional AI
- long-context (> 2048 tokens) benchmarks on frontier tasks
- multi-modality (vision, audio)
- code-execution sandbox for HumanEval live runs
- agent loops with persistent memory
- distributed / tensor-parallel inference
- speculative decoding, QAT

These are not hidden; the README's `Honest inventory` and this
document's bullet list both name them, so a reviewer can calibrate
claims against what is actually present in the tree.

## Where to read next

- `README.md` — front-door summary; measured numbers first.
- `docs/v111/THE_FRONTIER_MATRIX.md` — v111.1 matrix, signals, CLI.
- `docs/v111/THE_SIGMA_REASON_ENDPOINT.md` — v111.2 endpoint contract.
- `docs/v111/THE_SIGMA_ABSTAIN_LORA.md` — v111.3 training recipe.
- `docs/v106/THE_SIGMA_SERVER.md` — server architecture & config.
- `docs/v105/REPRESENTATION_SURGERY.md` — aggregator swap rationale.
- `docs/RESEARCH_AND_THESIS_ARCHITECTURE.md` — thesis-level scope.
- `docs/CLAIM_DISCIPLINE.md` — rules about which numbers go where.
