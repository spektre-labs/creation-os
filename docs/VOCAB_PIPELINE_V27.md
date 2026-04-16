# Vocabulary & tokenization pipeline — v27 (Creation OS)

**Evidence class:** **lab demo (C)** — deterministic `self_test` on `creation_os_v27.c` plus small tokenizer sources under `src/tokenizer/`. This document separates **what ships in-tree today** from the **product / FPGA / formal roadmap** described in the v27 directive.

## What ships today (merge-gate eligible)

| Item | Path | Notes |
|:--|:--|:--|
| Tier-1 stand-in | `src/tokenizer/bpe.c` | Greedy byte-token pipeline + toy merge table; **not** a trained 32K multilingual BPE. |
| Tier-2 byte codebook | `src/tokenizer/byte_fallback.c` | 256 deterministic rows; composition is **XOR-bind chain** (MAJ-window bundle is roadmap). |
| Tier-3 literal codec | `src/tokenizer/gda27_stub.c` | Base-27 unsigned codec only; **no** Rust `experimental/gda/` linkage yet. |
| Harness binary | `creation_os_v27.c` | `make check-v27` → **64** internal checks; `--inference "…"` writes JSON trace. |
| Microbench | `make bench-tokenizer-v27` | Host-dependent loops/sec; archive stdout per [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md). |

## What is explicitly **not** claimed here

- **No** “coherent LM output” guarantee from `./creation_os_v27 --inference`.
- **No** `decode(encode(x)) == x` for a full BPE vocabulary (SBY targets are roadmap).
- **No** Artix-7 utilization / 50k tok/s FPGA proof in this markdown (RTL hook is roadmap).
- **No** “87,000× fewer ops vs transformer at matched quality” headline without two-row harness + microbench separation per [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Roadmap (directive alignment)

1. **Real 32K / 128K / 1M codebooks** — memory math in the directive is fine as **arithmetic**; shipping it requires mmap-friendly layouts + reproducible training artifacts (not committed here).
2. **Rust GDA bridge** — add `experimental/gda/` linkage + C ABI once the crate boundary is stable.
3. **Benchmarks** — `binding_fidelity.c`, `vocab_scaling.c`, `vs_transformer.c` are placeholders for a future `benchmarks/` expansion; only `tokenizer_throughput.c` is wired in `Makefile` today.
4. **FPGA** — tokenizer BRAM table + 2-cycle lookup is a **hardware program**, not implied by green `self_test` in C.
5. **SBY** — prove roundtrips on a **finite** alphabet first; universal quantification over “all UTF-8” is not a near-term proof target.

## Commands

```bash
make check-v27
./creation_os_v27 --inference "hello" --trace-out inference_trace.json
make bench-tokenizer-v27
```

---

*Spektre Labs · Creation OS · 2026*
