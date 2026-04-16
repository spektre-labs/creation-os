# σ Full Stack — from paper to silicon (Creation OS v33–v54)

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
| v47 | Verification / hygiene | ACSL σ-kernel surface + extended SymbiYosys replay + ZK-σ API stub + `make verify` / property tests (not merge-gate) |
| v48 | Security / red team | σ-pattern anomaly + σ-gated sandbox + defense-in-depth aggregate + `make red-team` harness (Garak/DeepTeam optional) |
| v49 | Certification / assurance | DO-178C-aligned plans + traceability automation + MC/DC driver + binary hygiene + `make certify` (not regulator certification) |
| v50 | Benchmarks / public falsifiability | `make v50-benchmark` rollup (`FINAL_RESULTS.md`) + critic FAQ + Reddit draft (engine harness still external) |
| v51 | Integration scaffold / UX spec | `make check-v51` (cognitive loop + σ-gated agent + sandbox) + `config/v51_experience.yaml` + `src/v51/ui/web.html` + `scripts/v51/install.sh` (dry-run) + `docs/v51/ARCHITECTURE.md` |
| v53 | σ-governed harness scaffold / positioning | `make check-v53` (σ-TAOR loop + σ-dispatch + σ-compression + `creation.md` loader) + `creation.md` at repo root + `docs/v53/ARCHITECTURE.md` + `docs/v53/POSITIONING.md` + `docs/v53/paper_draft.md` |
| v54 | σ-proconductor scaffold / positioning | `make check-v54` (classify + select + aggregate + disagreement + profile learner; no network) + `docs/v54/ARCHITECTURE.md` + `docs/v54/POSITIONING.md` + `docs/v54/paper_draft.md` |
| v55 | σ₃-speculative scaffold (σ₃ decomposition + EARS + EASD) | `make check-v55` (29/29; NEON 4-accumulator entropy hot path, branchless fast log₂; wires Taparia 2603.24967, Sun 2512.13194, Su 2512.23765) + `docs/v55/ARCHITECTURE.md` + `docs/v55/POSITIONING.md` + `docs/v55/paper_draft.md` |
| v56 | σ-Constitutional scaffold (rule-based VPRM verifier + σ-gated IP-TTT budget controller + grokking commutator σ-channel + ANE `matmul→1×1 conv` layout helper) | `make check-v56` (56/56; NEON 4-accumulator defect reduction; branchless policy arithmetic; wires VPRM 2601.17223, IP-TTT 2604.06169, SLT grokking 2603.01192 + 2603.13331, 2026 ANE RE; one invariant — *any inference-time self-modification must strictly lower σ*) + `docs/v56/ARCHITECTURE.md` + `docs/v56/POSITIONING.md` + `docs/v56/paper_draft.md` |
| v57 | **The Verified Agent** convergence (5 invariants × 9 composition slots, each tier-tagged **M** / **F** / **I** / **P**; `static const` registry; no new σ math, no socket; convergence of v47 + v48 + v49 + v53 + v54 + v55 + v56 into one named artifact) | `make check-v57` (49/49 deterministic registry self-test) + `make verify-agent` (live aggregate: dispatches each owning `make` target and reports **PASS / SKIP / FAIL** per slot — never silent downgrade; this build → 6 PASS, 3 SKIP, 0 FAIL; SKIPs are honest reports for `verify-c` / `certify` / `red-team` when external tooling is absent) + `docs/v57/THE_VERIFIED_AGENT.md` + `docs/v57/ARCHITECTURE.md` + `docs/v57/POSITIONING.md` + `docs/v57/paper_draft.md` |

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

## v47 note (verification stack; not a product feature)

v47 is a **discipline layer**: contracts + tooling hooks + honest tier tags. Public claims about Frama-C / SymbiYosys / ZK must match `docs/v47/INVARIANT_CHAIN.md` and `docs/WHAT_IS_REAL.md`.
