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

## Interpretive tier (literature positioning; not measured in-repo)

| Claim | Notes | Tier |
|------|-------|:----:|
| SLM-default / LLM-fallback architecture (survey framing; external) | `arXiv:2510.03847` | **I** |
| NVIDIA position paper — small models sufficient for agentic workloads (external) | `arXiv:2506.02153` | **I** |

## Common headline numbers (explicitly not “M” here)

| Claim (headline style) | What it actually means | Tier |
|---|---:|:--:|
| “**87,000×**” | A **dimension-count / operation-shape** statement (e.g., memory ops / bit-parallelism at fixed \(D\)), not a benchmark row in this repo. | **T** |
| “**5.8 W**” | A **power projection** or mapping estimate, not an in-repo measured wattmeter reading. | **P** |
| “**1,052 cases**” | A dataset/analysis count that lives in an **external archive** (e.g., Zenodo record), not in this repository tree. | **E** |

## Explicit non-claims / stubs

| Item | Why it is not claimed here | Tier |
|------|----------------------------|------|
| Full BitNet b1.58 2B4T numerics from Microsoft GGUF in-process | Not shipped in this portable gate; requires external engine / upstream build | **N** |
| TruthfulQA / MMLU rows from `lm-eval-harness` | `benchmarks/truthfulqa_sigma.sh` is a **SKIP stub** until weights + harness are present | **N** |
| Routed FPGA bitstreams | Optional local smoke only; no CI bitstream artifacts | **N** |

## Retired claims (corrections)

This section is intentionally plain. If a prior post implied “measured” when it was only theoretical/projection/external, it belongs here.

| Previous public phrasing (approx.) | Correction in this repo | Tier |
|---|---|:--:|
| “87,000× measured speedup” | Treated as **theoretical / derived** unless accompanied by an archived repro bundle + harness row. | **R** |
| “5.8 W measured” | Treated as **projected** unless measured instrumentation + archive exists. | **R** |
| “1,052 cases in the repo” | If referenced, it must be linked as **external archive**; it is not stored here. | **R** |

Rule: **do not** present scripts, stubs, or imported papers as in-repo **measured** harness rows without an archived repro bundle (see `docs/REPRO_BUNDLE_TEMPLATE.md`).
