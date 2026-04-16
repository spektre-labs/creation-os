# σ Full Stack — from paper to physics (Creation OS v39–v40)

One equation family, many substrates:

\[
K_{\mathrm{eff}} = (1-\sigma_{\mathrm{total}})\cdot K
\]

At the software layers, \(\sigma_{\mathrm{total}}\) is often instantiated as **aleatoric + epistemic** (Dirichlet / evidence-style decomposition). At v39, the lab adds an explicit **hardware** term:

\[
\sigma_{\mathrm{total}} \approx \sigma_{\mathrm{aleatoric}} + \sigma_{\mathrm{epistemic}} + \sigma_{\mathrm{hardware}}
\]

This is **not** a claim that these three numbers are always statistically independent observables; it is a **bookkeeping interface** so analog substrate noise can be carried in the same σ ledger as semantic uncertainty.

## Layer map (engineering pointers)

| Layer | σ-component | What it measures (intent) | Where it lives |
|---|---|---|---|
| Paper | \(K_{\mathrm{eff}}=(1-\sigma)\cdot K\) | Coherence invariant | External corpus / Zenodo (see `docs/REPRO_BUNDLE_TEMPLATE.md`) |
| Algorithm | σ_aleatoric | “World is ambiguous” (Dirichlet mass / spread) | `src/sigma/decompose.c` |
| Algorithm | σ_epistemic | “Model doesn’t know” (Dirichlet / evidence coupling) | `src/sigma/decompose.c` |
| Algorithm | σ_hardware | Memristor / analog array non-ideality remainder (lab scalar) | `src/sigma/decompose_v39.c` |
| Software | σ_logit_entropy | Token prediction uncertainty (scalar channel) | `src/sigma/channels.c` |
| Software | σ_top_margin | Decision margin (scalar channel) | `src/sigma/channels.c` |
| Inference | σ_spec_draft | Speculative decoding confidence hooks | `src/v35/spec_decode.c` |
| Protocol | σ MCP tools | Exposed to MCP clients | `src/mcp/sigma_server.c` |
| FPGA | σ_pipeline | Cross-head variance gate | `hdl/v37/sigma_pipeline.sv` |
| ASIC | σ_tile | Tiny-tile abstention / SPI σ interface | `hdl/asic/sigma_tile.sv` |
| Physics (toy) | σ_hw column counter | Digital noise stand-in for analog MAC columns | `hdl/neuromorphic/crossbar_sim.sv` |
| Theory (lab) | σ-threshold hypothesis | “Below threshold + independent channels” story mapped to QEC language | `docs/sigma_threshold_theorem.md` |
| Diagnostics | σ-channel independence | Pearson correlation matrix + `threshold_regime` flag | `src/sigma/independence_test.c` |
| Policy (lab) | σ-syndrome decode | Map σ-vector highs to `{emit, abstain, fallback, …}` | `src/sigma/syndrome_decoder.c` |

## Neuromorphic / memristor substrate note

See `docs/neuromorphic/memristor_mapping.md` for the BitNet↔conductance story **as positioning**, not measured silicon.

## P-tier collaboration vectors (do not treat as shipped)

These are **planning contacts / directions**, not active partnerships:

1. **IHP (Leibniz Institute, Frankfurt Oder)** — 130 nm open PDK ecosystem; Tiny Tapeout moved here; natural ASIC partner lane for `hdl/asic/`.
2. **University of Manchester (SpiNNaker-2 group)** — large European neuromorphic research footprint; a credible place for “σ as a spike routing / abstention policy” conversations—*if* there is mutual interest and data.
3. **BrainChip (Akida)** — commercial neuromorphic SKU line; any “σ in SDK” story is **product/partnering**, not an in-repo claim.
4. **IMEC** — advanced semiconductor research; memristive crossbar research exists publicly; any collaboration is **P** until contracts + artifacts exist.

Rule: **do not** cold-contact from this repo alone; treat as roadmap until FPGA/ASIC lab artifacts and paper-grade measurements exist.

## Working paper title (only when data exists)

**“σ Across Substrates: A Unified Uncertainty Framework from Logits to Memristors”**

Thesis sentence: σ is **substrate-invariant in role** (a gate on coherence / continuation), even though its **instantiation** differs (semantic entropy vs device noise). The invariant story is **K_eff = (1−σ_total)·K**; the measurable part is what you archive per substrate.

## Version ladder (lab milestones)

| Version | Layer | σ shape |
|---|---|---|
| v33 | Agent runtime | σ-routed SLM/LLM fallback |
| v34 | Algorithmic | σ aleatoric / epistemic |
| v35 | Inference | σ-guided speculative decode |
| v36 | Ecosystem | σ MCP server |
| v37 | FPGA | σ-pipeline in RTL |
| v38 | ASIC | σ-tile (Tiny Tapeout class) |
| v39 | Physics (substrate) | σ_hardware (memristor analogue) |
| v40 | Theory / diagnostics | σ-threshold hypothesis + independence + syndrome decode |
| v41 | Inference / orchestration | σ-guided test-time compute (budget forcing, adaptive N, toy tree) |
| v42 | Learning / orchestration | σ-guided self-play (challenger/solver, σ-shaped reward, replay sampling) |
| v43 | Learning / orchestration | σ-guided knowledge distillation (uncertainty-weighted KL, anti-transfer term, staged curriculum, multi-teacher σ ensemble, σ calibration) |
| v44 | Serving / orchestration | σ-native inference proxy (engine-agnostic gate, OpenAI-shaped API + σ fields, demo per-token σ streaming) |
| v45 | Meta-cognition / calibration | σ-introspection (calibration gap, doubt reward, internal σ placeholder, paradox scatter harness stub) |
| v46 | Inference / efficiency | σ-optimized BitNet-facing pipeline (σ-from-logits scans, SIMD reductions, adaptive quant policy, e2e bench stub) |

## v40 working paper title (only when threshold harness exists)

**“The σ-Threshold Theorem: Exponential Hallucination Suppression via Independent Uncertainty Channels as Classical Quantum Error Correction”**

## v41 working paper title (only when test-time scaling harness exists)

**“σ-Guided Test-Time Compute: Adaptive Reasoning Depth via Epistemic Uncertainty Decomposition”**

## v42 working paper title (only when self-play + eval harness exists)

**“σ-Guided Self-Play: Uncertainty-Decomposed Reward Signals Enable Data-Free Self-Improvement of Language Models”**

## v43 working paper title (only when KD + eval harness exists)

**“σ-Guided Knowledge Distillation: Uncertainty-Weighted Targets Reduce Hallucination Transfer in Small Students”**

## v44 working paper title (only when proxy + overhead harness exists)

**“σ-Native Inference Proxies: Uncertainty-Gated OpenAI-Shaped Serving Across Engines”**

## v45 working paper title (only when introspection harness exists)

**“σ as Introspection: Calibrated Self-Knowledge in Language Models via Epistemic Uncertainty Decomposition”**

## v46 working paper title (only when BitNet+σ harness exists)

**“σ-Optimized BitNet Serving: Near-Zero-Margin σ Telemetry on CPU-Efficient Ternary Inference”**
