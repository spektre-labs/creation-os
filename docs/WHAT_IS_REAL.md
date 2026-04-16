# What is real in Creation OS (tier-tagged public claims)

This file exists to prevent **accidental tier mixing** when discussing Creation OS in public.

## Tier legend (one-letter tags)

- **M** — **Measured in this repo** (command + code path exists; green on supported hosts).
- **T** — **Theoretical / derived** (dimension counting, complexity shape, algebra); **not** measured here.
- **P** — **Projected** (power, latency, silicon mapping estimates); **not** measured here.
- **E** — **External evidence** (exists elsewhere, not in this repo); requires an external link / archive.
- **N** — **Not claimed** in-repo (explicitly out of scope / stubbed).
- **R** — **Retired / corrected** claim (previous public statement was wrong or ungrounded).

## Shipped, verifiable items (the “M” surface)

| Claim | Evidence (how to verify) | Tier |
|------|---------------------------|------|
| v26 flagship self-test passes **184/184** | `make check-v26 && ./creation_os_v26 --self-test` | **M** |
| v27 tokenizer scaffold self-test passes **70/70** | `make check-v27 && ./creation_os_v27 --self-test` | **M** |
| v28 LM integration shell self-test passes **29/29** | `make check-v28 && ./creation_os_v28 --self-test` | **M** |
| v29 collapse harness self-test passes **22/22** | `make check-v29 && ./creation_os_v29 --self-test` | **M** |
| GGUF fixture round-trips through minimal parser | `make check-v29` (`gguf_fixture_write` … `v29_integrity`) | **M** |
| mmap-backed GGUF tensor byte views on POSIX | `src/import/gguf_loader.c` + `make check-v29` on Linux/macOS | **M** |
| Eight scalar σ signals + abstention gate compile and behave on toy tensors | `src/sigma/channels.c` + `make check-v29` | **M** |
| XNOR / Hamming-style attention toy runs on tiny tensors | `src/nn/attention_xnor.c` + `make check-v29` | **M** |
| OpenAI-shaped loopback stub serves `/v1/*` deterministically + CORS/OPTIONS | `make check-openai-stub` (`creation_os_openai_stub`) | **M** |
| v2 bootstrap demo has `--self-test` and `--help` | `make standalone && ./creation_os --self-test` | **M** |
| v33 lab: σ-routed fallback between local BitNet and a configurable secondary model + schema-first tool JSON + JSONL session metrics | `make check-v33` | **M** |
| v34 lab: Dirichlet-style epistemic/aleatoric decomposition + Platt JSON hook + extended σ channels (`channels_v34`) + `cos_route_from_logits_v34` | `make check-v34` | **M** |
| v35 lab: σ-guided adaptive speculative draft length + dual-verify abstain hook + progressive local/spec/API routing config | `make check-v35` | **M** |
| v36 lab: MCP JSON-RPC σ server (`creation_os_mcp`) — tools/resources/prompts over STDIO + optional HTTP POST shim | `make check-mcp` | **M** |
| v37 lab: σ-pipeline SystemVerilog (XNOR bind + per-head popcount + cross-head variance σ-entropy + `sigma_abstain`) + Yosys `synth_xilinx` stats script + SymbiYosys harness | `make synth-v37` / `make formal-sby-v37` / `hdl/v37/synth_and_measure.sh` (SKIPs if `yosys` / `sby` missing) | **M** |
| v38 lab: Tiny Tapeout–style σ tile (`tt_um_sigma_tile`) Verilator smoke (fast `HV_BITS=128` geometry) | `make check-asic-tile` (SKIPs if `verilator` missing) | **M** |
| v38 lab: LibreLane/OpenLane-class RTL→GDSII **driver template** (`hdl/asic/config.json` + `hdl/asic/librelane_run.sh`) | `make librelane-v38` (SKIPs if LibreLane + PDK stack not installed) | **M** |
| v39 lab: σ_hardware scalar + σ_total composition on top of Dirichlet σ (`sigma_full_t`) | `make check-v39` | **M** |
| v39 lab: digital **ternary_crossbar** toy + harness (column MAC + pseudo-noise counter) | `make check-crossbar-sim` (SKIPs if `verilator` missing) | **M** |
| v40 lab: σ-channel **independence** diagnostics + **σ-syndrome** action decoder (6 actions) | `make check-v40` | **M** |
| v40 lab: TruthfulQA σ-channel sweep **stub** (threshold theorem harness placeholder) | `make bench-v40-threshold` (exits 0; real eval requires external weights + CLI) | **M** |
| v41 lab: σ-guided **test-time compute** scaffold (budget forcing + adaptive best-of-N + toy reasoning tree + verify bookkeeping) | `make check-v41` | **M** |
| v41 lab: GSM8K / MATH / AIME **scaling harness** (“2B + σ beats 70B”) | `make bench-v41-scaling` (exits 0 as **stub**; real curves require external weights + eval CLI + REPRO bundle) | **M** |
| v42 lab: σ-guided **self-play** scaffold (challenger/solver/replay + σ-shaped reward + curriculum hook) | `make check-v42` | **M** |
| v42 lab: self-play **improvement curve** harness (“data-free self-RL beats GSM8K”) | `make bench-v42-curve` (exits 0 as **stub**; real runs require external weights + self-play driver + eval CLI + REPRO bundle) | **M** |
| v43 lab: σ-guided **knowledge distillation** math (weighted KL + progressive stages + multi-teacher ensemble + σ calibration loss) | `make check-v43` | **M** |
| v43 lab: KD **quality** harness (TruthfulQA / calibration / toxic transfer vs standard KD) | `make bench-v43-distill` (exits 0 as **stub**; real runs require teacher+student weights + distill driver + eval CLI + REPRO bundle) | **M** |
| v44 lab: σ-native **inference proxy** (stub engine logits → per-token σ → v40 syndrome actions → OpenAI-shaped loopback HTTP + demo SSE) | `make check-v44` (alias: `make check-proxy`) | **M** |
| v44 lab: σ-proxy **latency overhead** harness (“<5% overhead vs raw engine”) | `make bench-v44-overhead` (exits 0 as **stub**; real runs require pinned engine + load generator + archived JSON) | **M** |
| v45 lab: σ-**introspection** (calibration gap + doubt-reward + internal probe stub) | `make check-v45` | **M** |
| v45 lab: **Gemini paradox / introspection scatter** harness (accuracy vs calibration_gap) | `make bench-v45-paradox` (exits 0 as **stub**; real runs require multi-model eval + archived JSON + plot inputs) | **M** |

## Interpretive tier (literature positioning; not measured in-repo)

| Claim | Notes | Tier |
|------|-------|:----:|
| SLM-default / LLM-fallback architecture (survey framing; external) | `arXiv:2510.03847` | **I** |
| NVIDIA position paper — small models sufficient for agentic workloads (external) | `arXiv:2506.02153` | **I** |
| UQ / abstention / calibration themes (ICLR 2026, TACL “Know Your Limits”, `arXiv:2603.06317`, ACM CSUR taxonomies, LogTokU, DiverseAgentEntropy, …) | Positioning map in `docs/SIGMA_VS_FIELD.md` | **I** |
| Speculative decoding + edge–cloud drafts (EAGLE-class, Mirror-SD, “AI-RAN” narratives, …) | Positioning in `docs/SIGMA_GUIDED_SPEC.md` | **I** |
| XNOR + popcount ternary / binary FPGA inference line (FINN / Xilinx, LUTNet, XNOR Neural Engine, BRein-style blocks, TerEffic-class ternary GEMM, …) | Survey / citations in vendor + arXiv literature; not a shipped bitstream in this repo | **I** |
| `hls4ml` (CERN) supports quantized / ternary-ish export paths for FPGA HLS flows | External tooling + papers; not wired into this repo’s build | **I** |
| Neuromorphic ↔ transformer bridge narratives (Nature collections / surveys; “efficient transformer-like inference” positioning) | External literature; not a measured memristor row in this repo | **I** |
| Quantum threshold / below-threshold error suppression narratives (e.g., Willow-class demos) + FPGA syndrome decoding (e.g., Riverlane-class tooling) | External physics + engineering literature; used only as **analogy** in `docs/sigma_threshold_theorem.md` | **I** |
| Learned decoders for stabilizer syndromes (e.g., AlphaQubit-class lines) | External ML-for-decoding literature; “σ-AlphaQubit” is a **hypothesis** until a dataset + training recipe is archived | **I** |

## Common headline numbers (explicitly not “M” here)

| Claim (headline style) | What it actually means | Tier |
|---|---:|:--:|
| “**87,000×**” | A **dimension-count / operation-shape** statement (e.g., memory ops / bit-parallelism at fixed \(D\)), not a benchmark row in this repo. | **T** |
| “**5.8 W**” | A **power projection** or mapping estimate, not an in-repo measured wattmeter reading. | **P** |
| “**1,052 cases**” | A dataset/analysis count that lives in an **external archive** (e.g., Zenodo record), not in this repository tree. | **E** |

## Projections (explicit “P” surface)

| Claim | Notes | Tier |
|------|-------|:----:|
| Full BitNet-class ternary inference on FPGA with σ-gating in the same synthesized datapath as TerEffic-style cores | Requires accelerator integration + closed timing on a chosen board; not claimed as shipped silicon here | **P** |
| σ-tile / σ-pipeline tapeout via **IHP SG13G2 (130 nm)** on a **Tiny Tapeout-class** shuttle (public run calendar varies; mid‑2026 is a planning anchor, not a contract date) | Requires template repo wiring + CI + foundry queue; no in-repo GDSII artifact is claimed by default | **P** |
| Alternative σ-tile route via **GlobalFoundries GF180MCU** (open PDK) on a shuttle or direct MPW slot | Node choice depends on aggregator + PDK pin‑out; not claimed as locked here | **P** |
| Direct **IHP** MPW slot (outside Tiny Tapeout) if tile size / macro budgeting demands it | Commercial-ish planning bracket often cited as **USD ~5k–15k** per slot; treat as **P** until a signed quote exists | **P** |
| Neuromorphic collaboration lanes (IHP neuromorphic/SiGe lines, Manchester SpiNNaker-2, BrainChip Akida SDK, IMEC memristor programs) | **Contacts / positioning only** until contracts + shared artifacts exist; see `docs/SIGMA_FULL_STACK.md` | **P** |
| Empirical **n_crit** for “σ-threshold” phase transition (hallucination vs independent channel count) | Requires an archived eval harness + model weights; see `docs/sigma_threshold_theorem.md` + `benchmarks/v40/threshold_test.sh` | **P** |

## Explicit non-claims / stubs

| Item | Why it is not claimed here | Tier |
|------|----------------------------|------|
| Full BitNet b1.58 2B4T numerics from Microsoft GGUF in-process | Not shipped in this portable gate; requires external engine / upstream build | **N** |
| TruthfulQA / MMLU rows from `lm-eval-harness` | `benchmarks/truthfulqa_sigma.sh` is a **SKIP stub** until weights + harness are present | **N** |
| TruthfulQA / FreshQA / SelfAware AUROC–ECE tables “for σ_total vs σ_epistemic” | `benchmarks/v34/run_abstention_benchmarks.sh` is a **smoke stub** until datasets + weights + harness are archived in-repo | **N** |
| Measured tokens/sec / acceptance curves for σ-guided vs fixed-K speculative decode | `benchmarks/v35/spec_bench.sh` is **synthetic** until BitNet+Qwen (or API) harness + weights exist | **N** |
| Claude Desktop TruthfulQA/GSM8K A/B: σ-MCP on vs off | `benchmarks/v36/mcp_bench_stub.sh` is a **local JSON-RPC smoke** until a client harness + dataset bundle is archived | **N** |
| Routed FPGA bitstreams | Optional local smoke only; no CI bitstream artifacts | **N** |
| “SymbiYosys proved σ-abstention for **full 4096-bit** hypervectors at **16 heads**” | BMC harness in `hdl/v37/` uses **reduced** `HV_BITS` / `N_HEADS` for tractability; default parameters are the architectural target, not the proved geometry unless you rerun SBY with a larger bound | **N** |
| “Measured Fmax / timing closure on Artix-7 hardware” | `synth_xilinx` + `stat` are **tool estimates** from Yosys unless accompanied by Vivado timing on a locked part + P&R | **N** |
| “LibreLane always completes green on a cold laptop” | Needs pinned tool rev + installed PDK + disk; the repo ships a **template** and a **runner script**, not a hermetic repro bundle | **N** |
| “Verilator `check-asic-tile` proves the 4096-bit tapeout geometry” | The Makefile target builds **`HV_BITS=128`** for fast smoke; full 4096-bit behavior is a separate heavy sim unless you change the flag | **N** |
| “`ternary_crossbar` is a measured memristor array” | It is a **digital toy model** for column MAC + pseudo noise (`hdl/neuromorphic/crossbar_sim.sv`), not device physics | **N** |
| “σ_hardware is calibrated to a specific fab device” | `sigma_hardware_estimate()` is a **scalar lab mapping** (`src/sigma/decompose_v39.c`), not a foundry-qualified noise model | **N** |
| “`bench-v40-threshold` proves exponential hallucination suppression vs channel count” | The script is a **stub** until `creation_os` (or another evaluator) can run archived TruthfulQA-style harnesses in-tree | **N** |
| “σ-threshold theorem is proven for LLMs in this repo” | v40 ships **definitions + diagnostics + decoder**; the exponential suppression claim is **hypothesis / P-tier** until measured and archived | **N** |
| “**2B BitNet + σ-guided test-time compute matches 70B** on GSM8K/AIME in this repo” | v41 ships **policy + toy integration** only; any crossover budget claim is **P-tier** until `benchmarks/v41/budget_*.json` exists with archived harness metadata | **N** |
| “**σ-only reward replaces all external verifiers** for self-play RL in this repo” | v42 ships **toy consistency + σ decomposition** only; any “no external data ever” claim is **N** until archived self-play + eval harnesses exist | **N** |
| “**σ-weighted KD always beats standard KD** on TruthfulQA / toxic transfer in this repo” | v43 ships **scalar loss contracts** only; any A/B headline is **N** until `benchmarks/v43/distill_*.json` exists with archived harness metadata | **N** |
| “**σ-proxy adds <5% latency** vs raw vLLM/SGLang in this repo” | v44 ships **loopback HTTP + stub logits** only; any overhead headline is **N** until `benchmarks/v44/*.json` exists with archived harness metadata | **N** |
| “**Mid-stream retraction is safe** against arbitrary tokenizer + engine streams in this repo” | SSE demo lines are **not** a guaranteed rewind protocol until integrated with a real streaming parser + engine cancellation contract | **N** |
| “**Public Table 1** introspection scatter (Gemini vs Claude vs BitNet+σ) exists in this repo” | v45 ships **deterministic lab math + stubs** only; any model-point scatter is **N** until `benchmarks/v45/introspection_*.json` is archived with harness metadata | **N** |
| “**σ_internal from real hidden states** is shipped for vLLM/SGLang/bitnet.cpp in this repo” | `v45_probe_internals_lab` is a **deterministic placeholder** until engine hooks + weights bundle exist | **N** |

## Retired claims (corrections)

This section is intentionally plain. If a prior post implied “measured” when it was only theoretical/projection/external, it belongs here.

| Previous public phrasing (approx.) | Correction in this repo | Tier |
|---|---|:--:|
| “87,000× measured speedup” | Treated as **theoretical / derived** unless accompanied by an archived repro bundle + harness row. | **R** |
| “5.8 W measured” | Treated as **projected** unless measured instrumentation + archive exists. | **R** |
| “1,052 cases in the repo” | If referenced, it must be linked as **external archive**; it is not stored here. | **R** |
| “SkyWater 130 nm tapeout via **Efabless / ChipIgnite**” | **Efabless shut down (2025)**; ChipIgnite-style SkyWater buckets through that channel are not the live default planning story in this repo (see `hdl/asic/TAPEOUT_CHECKLIST.md`). | **R** |

Rule: **do not** present scripts, stubs, or imported papers as in-repo **measured** harness rows without an archived repro bundle (see `docs/REPRO_BUNDLE_TEMPLATE.md`).
