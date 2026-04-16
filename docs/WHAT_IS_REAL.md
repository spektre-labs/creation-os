# What is real in Creation OS v29 (evidence tags)

This file exists to prevent **accidental tier mixing** when discussing the v29 “collapse harness”.

| Claim | Evidence | Tier |
|------|----------|------|
| GGUF fixture round-trips through the in-tree minimal parser | `make check-v29` (`gguf_fixture_write` … `v29_integrity`; **22** checks) | **measured (C)** |
| mmap-backed GGUF tensor byte views on POSIX | `src/import/gguf_loader.c` + `check-v29` on Linux/macOS | **measured (C)** |
| Eight scalar sigma signals + abstention gate compile and behave on toy tensors | `src/sigma/channels.c` + `check-v29` | **measured (C)** |
| XNOR / Hamming-style attention toy runs on tiny tensors | `src/nn/attention_xnor.c` + `check-v29` | **measured (C)** |
| BitNet-shaped forward API exists with deterministic stub logits | `src/nn/bitnet_forward_stub.c` + `check-v29` | **measured (C)** |
| Full BitNet b1.58 2B4T numerics from Microsoft GGUF in-process | Not shipped in this portable gate; requires external engine / upstream build | **not claimed** |
| TruthfulQA / MMLU rows from `lm-eval-harness` | `benchmarks/truthfulqa_sigma.sh` is a **SKIP stub** until weights + harness are present | **not claimed** |
| Vivado / routed Artix-7 bitstreams | `hdl/synthesis/synth_yosys.sh` is optional local smoke; no CI bitstream | **not claimed** |

Rule: **do not** present stub scripts or imported papers as in-repo **measured** harness rows without an archived repro bundle (see `docs/REPRO_BUNDLE_TEMPLATE.md`).
