# Creation OS v31 — “The Purge” (lab framing, honest tiers)

This document is the **v31 lab** contract: a thin wrapper direction around an upstream BitNet inference binary, without rewriting kernels.

It is **not** a claim that v31 replaces the repository’s default maintainer surface. The portable maintainer bar remains **`make merge-gate`**, and the reviewer bar remains **`make reviewer`**.

## Tier legend (same as `docs/WHAT_IS_REAL.md`)

- **M** — measured in this repo (command exists; green on supported hosts)
- **T** — theoretical / derived (not measured here)
- **P** — projected (not measured here)
- **E** — external evidence (outside this repo; must link)
- **N** — not claimed in-repo
- **R** — retired / corrected public phrasing

## What v31 ships *in this repo today* (measured)

| Claim | Evidence | Tier | Verify |
|---|---|:--:|:--|
| Deterministic σ-style entropy + margin math on toy logprobs | `src/v31/creation_os_v31.c` `--self-test` | **M** | `make standalone-v31 && ./creation_os_v31 --self-test` |
| Optional upstream spawn hook exists (best-effort) | env `COS_BITNET_CLI` + `COS_BITNET_MODEL` | **M** | build exists + exports set (manual) |

## What v31 does **not** claim as “M” without external receipts

| Item | Tier | Notes |
|---|---:|:--|
| “Upstream chat works end-to-end in CI” | **N** | requires weights + stable CLI flags + harness + repro bundle |
| “TruthfulQA improves under σ-gate” | **N** | external harness + archive |
| “ASIC/FPGA shipped here” | **N** | optional RTL stack is separate |

## External / imported (must stay **E**)

| Claim | Tier | Source |
|---|---:|:--|
| BitNet kernels + model behavior | **E** | Microsoft `BitNet` upstream repository and its releases/docs |
| Any benchmark tables | **E** | `docs/REPRO_BUNDLE_TEMPLATE.md` |

## Retired / forbidden to smuggle as Creation OS “M”

- “87,000×”, “5.8W”, “1,052 cases in-repo”, “SkyWater tapeout shipped” → **R** (see `docs/WHAT_IS_REAL.md`)

## Recommended integration path (safe)

1. Keep **`creation_os_openai_stub`** for protocol wiring tests.
2. Vendor/build upstream **outside** merge-gate, export `COS_BITNET_*`, then iterate the wrapper until the protocol is stable.
3. Only then add optional harness targets that archive numbers per `docs/REPRO_BUNDLE_TEMPLATE.md`.
