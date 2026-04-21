# Creation OS — Limitations

This is a **research prototype**.  Canonical claim discipline:
[`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).  Canonical thesis
scope / non-goals: [`RESEARCH_AND_THESIS_ARCHITECTURE.md`](RESEARCH_AND_THESIS_ARCHITECTURE.md).

The root `README.md` links here from its **Limitations** section; every
bullet below is one “what this is *not*” paragraph.  Nothing in the
root README or in any flagship doc promotes a bullet from this list to
a headline without first publishing the measurement that would retire
the caveat.

## Flagship versions

- **Oracle** generates text from a 15-sentence corpus via n-gram
  codebook.  It demonstrates that attention can be implemented as σ,
  not that it matches LLM-quality text generation.
- **JEPA learning** is codebook memorization with correlative
  blending.  Energy decreases because the codebook stores training
  pairs, not because the model has learned to generalize to unseen
  data.
- **GEMM benchmark** compares computational cost of the same geometric
  task (vector distance) at different precision levels.  The 192× ops
  ratio is measured and real.  Whether binary precision is sufficient
  for a given application is an empirical question.
- **Cognitive modules** are BSC implementations of cognitive
  primitives.  They demonstrate that these computations can be
  expressed in three bit operations.  They are not validated against
  cognitive-science benchmarks.

## Living-kernel / hallucination / silicon tiers

- **Living Kernel (`creation_os_v6.c`)** is a second program:
  schematic σ–K–L composition + M01–M18 *toys*.  The 30 `self_test`
  checks are **internal consistency**, not a clinical consciousness
  proof, not COGITATE reproduction, and not a substitute for
  `make bench` or NEON/HV receipts.  See
  [`LIVING_KERNEL_V6.md`](LIVING_KERNEL_V6.md).
- **`creation_os_v7.c`** is a third program: v6 **plus** M19–M23
  hallucination-*shaped* σ channels; 35 `self_test` checks.  Still
  **not** measured LM hallucination rates — see
  [`HALLUCINATION_KILLER_V7.md`](HALLUCINATION_KILLER_V7.md).
- **`creation_os_v9.c`** is a fourth program: v7 **plus** M24–M29
  stack/silicon-*shaped* σ toys; 41 checks — not tape-out or vendor
  TOPS/W claims — see
  [`PARAMETERS_IN_SILICON_V9.md`](PARAMETERS_IN_SILICON_V9.md).
- **`creation_os_v10.c`** is a fifth program: v9 **plus** M30–M33
  distillation / routing / abstention toys; 46 checks — see
  [`THE_REAL_MIND_V10.md`](THE_REAL_MIND_V10.md).
- **`creation_os_v11.c`** is a sixth program: v10 **plus** M34
  matmul-free LM **schematic**; 49 checks — not a trained BitNet-class
  model or published throughput reproduction — see
  [`THE_MATMUL_FREE_MIND_V11.md`](THE_MATMUL_FREE_MIND_V11.md).
- **`creation_os_v12.c`** is a seventh program: v11 **plus** M35–M37
  classical tensor-train / entropy / sequence-head **toys**; 52
  checks — not quantum hardware, not TN-LM harness rows — see
  [`THE_TENSOR_MIND_V12.md`](THE_TENSOR_MIND_V12.md).

## v27 — v29 tokenizer / LM shell

- **`creation_os_v27.c`** is an eighth program: **M177–M186** vocab /
  tokenizer / mmap COSB / inference-trace **scaffold** with
  `src/tokenizer/*.c`; 70 checks — **not** a trained multilingual LM
  tokenizer product, not FPGA timing proof, not “coherent LM”
  quality — see [`VOCAB_PIPELINE_V27.md`](VOCAB_PIPELINE_V27.md).
- **`creation_os_v28.c`** is a ninth program: **M190–M199** LM
  **integration shell** (`src/import`, `src/nn`, `src/server`); 29
  checks — **not** an in-process BitNet b1.58 2B4T forward, not
  `lm-eval` rows by itself, not a weights bundle.
- **`creation_os_v29.c`** is a tenth program: **v29 collapse harness**
  (`src/import/gguf_loader.c`, `src/sigma/channels.c`,
  `src/nn/attention_xnor.c`, `src/nn/bitnet_forward_stub.c`); 22
  checks — **not** a downloaded 2B checkpoint, not harness rows by
  itself — see [`WHAT_IS_REAL.md`](WHAT_IS_REAL.md).

## σ-gate (benchmark signal)

- **σ is not a universal signal.**  It is **Bonferroni-significant**
  on TruthfulQA-class factual-confidence tasks; on HellaSwag (N=746)
  and on the 7-subject MMLU eligible floor (N=605 total) **entropy is
  the best signal** and σ-gating adds no usable information.  The
  v111 frontier matrix reports both the positive and the negative
  findings without editorial softening —
  [`v111/THE_FRONTIER_MATRIX.md`](v111/THE_FRONTIER_MATRIX.md).
- **Conformal guarantee scope.**  The Vovk–Gammerman threshold τ_c
  provides a finite-sample, distribution-free bound **on exchangeable
  draws from the calibration distribution**.  On distribution shift
  the bound reverts to empirical AURCC behaviour.  See
  [`v111/CONFORMAL_GUARANTEE.md`](v111/CONFORMAL_GUARANTEE.md).

## Arithmetic vs throughput claims

- **192× ops** and **32× RAM** are arithmetic ratios derived from the
  stated `D = 4096` encodings; see
  [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).
- **Throughput numbers** require `make bench` plus archived host
  metadata ([`REPRO_BUNDLE_TEMPLATE.md`](REPRO_BUNDLE_TEMPLATE.md)).
  Microbench throughput and harness MMLU / ARC / TruthfulQA accuracy
  are **separate evidence classes** and must never be merged into a
  single headline sentence.

## Dissertation / publication scope

- v6 — v12 are **Lab demo (C)** artefacts and never substitute for
  harness, silicon, or quantum claims.  The ordered doctoral read path
  in the root README (§ *Doctoral and committee read path*) is the
  canonical reviewer map;
  [`RESEARCH_AND_THESIS_ARCHITECTURE.md`](RESEARCH_AND_THESIS_ARCHITECTURE.md)
  carries the formal contributions (C1 — C6) and threats-to-validity
  list.

---

*Spektre Labs · Creation OS · 2026*
