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

## Interpretive tier (literature positioning; not measured in-repo)

| Claim | Notes | Tier |
|------|-------|:----:|
| SLM-default / LLM-fallback architecture (survey framing; external) | `arXiv:2510.03847` | **I** |
| NVIDIA position paper — small models sufficient for agentic workloads (external) | `arXiv:2506.02153` | **I** |
| UQ / abstention / calibration themes (ICLR 2026, TACL “Know Your Limits”, `arXiv:2603.06317`, ACM CSUR taxonomies, LogTokU, DiverseAgentEntropy, …) | Positioning map in `docs/SIGMA_VS_FIELD.md` | **I** |
| Speculative decoding + edge–cloud drafts (EAGLE-class, Mirror-SD, “AI-RAN” narratives, …) | Positioning in `docs/SIGMA_GUIDED_SPEC.md` | **I** |
| XNOR + popcount ternary / binary FPGA inference line (FINN / Xilinx, LUTNet, XNOR Neural Engine, BRein-style blocks, TerEffic-class ternary GEMM, …) | Survey / citations in vendor + arXiv literature; not a shipped bitstream in this repo | **I** |
| `hls4ml` (CERN) supports quantized / ternary-ish export paths for FPGA HLS flows | External tooling + papers; not wired into this repo’s build | **I** |

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

## Retired claims (corrections)

This section is intentionally plain. If a prior post implied “measured” when it was only theoretical/projection/external, it belongs here.

| Previous public phrasing (approx.) | Correction in this repo | Tier |
|---|---|:--:|
| “87,000× measured speedup” | Treated as **theoretical / derived** unless accompanied by an archived repro bundle + harness row. | **R** |
| “5.8 W measured” | Treated as **projected** unless measured instrumentation + archive exists. | **R** |
| “1,052 cases in the repo” | If referenced, it must be linked as **external archive**; it is not stored here. | **R** |
| “SkyWater 130 nm tapeout via **Efabless / ChipIgnite**” | **Efabless shut down (2025)**; ChipIgnite-style SkyWater buckets through that channel are not the live default planning story in this repo (see `hdl/asic/TAPEOUT_CHECKLIST.md`). | **R** |

Rule: **do not** present scripts, stubs, or imported papers as in-repo **measured** harness rows without an archived repro bundle (see `docs/REPRO_BUNDLE_TEMPLATE.md`).
