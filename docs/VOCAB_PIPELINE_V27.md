# Vocabulary & tokenization pipeline — v27 (Creation OS)

**Evidence class:** **lab demo (C)** — deterministic `self_test` on `creation_os_v27.c` plus tokenizer sources under `src/tokenizer/`. This document separates **what ships in-tree today** from the **product / training / FPGA roadmap** described in the broader v27 directive.

## What ships today (merge-gate eligible)

| Item | Path | Notes |
|:--|:--|:--|
| Tier-1 BPE stand-in | `src/tokenizer/bpe.c` | Greedy byte-token pipeline + toy merge table; **not** a trained multilingual BPE learned on a corpus. |
| Tier-1 COSB mmap table | `src/tokenizer/cos_codebook_mmap.{h,c}` | On-disk **COSB** header + `n_vocab` × 4096-bit rows; **POSIX `mmap` / Win32 `malloc+fread`**. Deterministic rows via `cos_codebook_row_fill`. |
| Codebook generator | `tools/gen_cos_codebook.c` + `make gen-cos-codebook` | Writes `.build/cos_codebook_4k.bin` by default; pass `32768 …` for a ~16 MiB Tier-1 table. |
| Tier-2 byte codebook | `src/tokenizer/byte_fallback.c` | 256 deterministic rows; **XOR-bind chain** + **MAJ sliding window** (`byte_bundle_maj_sliding`). |
| Tier-3 literal codec | `src/tokenizer/gda27_stub.c` | Base-27 unsigned encode/decode (C). |
| Tier-3 Rust sidecar (optional) | `experimental/gda27_rust/` | `staticlib` exporting `creation_os_gda27_rust_{encode,decode}` — link via `make standalone-v27-rust` (requires **cargo**). |
| Harness binary | `creation_os_v27.c` | `make check-v27` → **70** internal checks; `--inference "…"` writes JSON trace; **`COS_CODEBOOK_PATH`** enables mmap HV path. |
| Microbenches | `make bench-tokenizer-v27`, `make bench-v27-all` | Throughput, XOR bind/unbind fidelity stats, vocab-size scaling loop, toy ops accounting vs dense GEMV story. |
| Formal hook (optional) | `make formal-sby-tokenizer` | SymbiYosys BMC on `rtl/cos_gda27_roundtrip_formal.sv` (digit alphabet injective for **0..26**). |

## What is explicitly **not** claimed here

- **No** “coherent LM output” guarantee from `./creation_os_v27 --inference`.
- **No** `decode(encode(x)) == x` for a **full** learned BPE vocabulary (finite-alphabet proofs start small; see SBY hook above).
- **No** Artix-7 utilization / 50k tok/s FPGA proof in this markdown (RTL hook below is **not** a tape-out receipt).
- **No** “87,000× fewer ops vs transformer at matched quality” headline without a two-row harness + microbench separation per [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md).

## Roadmap (directive alignment)

1. **Learned 32K / 128K / 1M BPE merges + vocab strings** — training pipeline + reproducible corpus hashes (not implied by COSB row generator alone).
2. **FPGA tokenizer BRAM shell** — add a dedicated `cos_tokenizer_codebook` macro block + vendor synthesis receipts (LUT/RAMB18 counts).
3. **Stronger formal** — extend SBY to pipeline BRAM timing abstraction + encode/decode over a bounded UTF-8 subset.
4. **Quality harness** — external tokenizer parity + `lm-eval` class rows if/when a real LM stack is attached.

## Commands

```bash
make check-v27
./creation_os_v27 --self-test
./creation_os_v27 --inference "hello" --trace-out inference_trace.json
COS_CODEBOOK_PATH=.build/cos_codebook_4k.bin ./creation_os_v27 --inference "hello"

make gen-cos-codebook          # default 4k table into .build/
make bench-v27-all             # tokenizer + binding + scaling + ops accounting
make formal-sby-tokenizer      # optional SymbiYosys
make standalone-v27-rust       # optional Rust staticlib link (needs cargo)
```

---

*Spektre Labs · Creation OS · 2026*
