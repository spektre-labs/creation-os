# σ Threshold Theorem (Creation OS v40 — classical analogue)

This document is **T-tier / P-tier positioning**: it states a **research hypothesis** and a **mapping** to quantum fault tolerance language. It is **not** an in-repo proof that a particular LLM’s hallucination rate scales exponentially with σ-channel count.

## Classical analogue of quantum error correction

### Quantum fault tolerance (sketch)

In quantum computing (threshold theorem story):

- Physical error rate **p** below a threshold **p_th**
- Logical error rate can shrink with code distance **d** (more physical qubits encoding one logical qubit), often summarized informally as “exponential suppression” when **below threshold** and with good decoding.

### Creation OS σ story (sketch)

In Creation OS (hypothesis):

- Per-token uncertainty signals **σ** (vector channel measurements) are compared to calibrated thresholds **σ_th** (Platt-style calibration appears in the v34 lab path; see `docs/WHAT_IS_REAL.md`).
- An **ensemble of weakly correlated σ-channels** plays a role analogous to **syndrome information** in QEC: more *independent* channels ⇒ more reliable identification of “this continuation is unsafe”.
- **Abstention / routing** plays a role analogous to a **decoder action** that maps syndromes to recovery / avoidance.

This is intentionally **analogy-first**: token prediction is not a literal quantum code, and “distance” is not a literal Hamming distance of a stabilizer code.

## The parallel (engineering cartoon)

| Quantum (cartoon) | Classical (σ) |
|---|---|
| Physical qubit | Raw per-token decision substrate (logits / hidden states / tools) |
| Physical error rate **p** | Raw uncertainty readouts **σ** (pre-calibration) |
| Error correcting code | σ-channel ensemble + bookkeeping |
| Code distance **d** | Effective number of **independent** σ-channels **n** |
| Logical qubit | “Verified output” / routed action |
| Logical error rate **p_L** | Hallucination / unsafe-action rate **H** (must be defined per harness) |
| Threshold theorem | **σ-threshold hypothesis**: below calibrated regime, extra *independent* channels strongly reduce **H** |
| Syndrome measurement | σ-channel measurement vector |
| Decoder | `decode_sigma_syndrome()` (`src/sigma/syndrome_decoder.c`) |
| Fault-tolerant gate | σ-verified tool call / abstain-first policy |

## Prediction (**P-tier**, must be measured externally)

If σ-channels are **sufficiently independent** and **calibrated**, then increasing **n** may induce a **sharp improvement** in hallucination metrics at some empirical **n_crit** (phase-transition language).

Example *hypothesis shapes* (not constants measured here):

- **2 channels** → moderate reduction (maybe)
- **4 channels** → stronger reduction (maybe)
- **8 channels** → “exponential suppression territory” (**only if** independence + calibration + dataset harness exist)

The key empirical question is: **where is n_crit for your model family, prompt distribution, and abstention policy?**  
The repo ships **independence diagnostics** (`src/sigma/independence_test.c`) and a **benchmark stub** (`benchmarks/v40/threshold_test.sh`) until a TruthfulQA (or equivalent) harness is archived in-tree.

## Decoder analogue: FPGA syndrome decode (external evidence)

Riverlane-style demonstrations that **decoding workloads can be executed on FPGA-class substrates at microsecond scales** are useful *existence proofs* that “syndrome → action” pipelines can be hardware-latency dominated (see external literature; treat as **E** in `docs/WHAT_IS_REAL.md` unless you vendor a reproducible bit-exact artifact in-repo).

Creation OS already has an FPGA-class σ datapath sketch (`hdl/v37/sigma_pipeline.sv`). v40 adds a **software syndrome decoder** as a stepping stone toward “σ decode on the same substrate class as QEC decoders” — **not** a claim of shipped integration.

## σ-AlphaQubit (hypothesis)

DeepMind’s **AlphaQubit** line uses learned models to assist decoding (external). The v40 analogue is: **learn a decoder** from σ-syndrome vectors to optimal actions (abstain / fallback / resample / …), potentially using a small teacher model — **P-tier** until a dataset + training recipe is archived.

## Working paper title (only when a harness exists)

**“The σ-Threshold Theorem: Exponential Hallucination Suppression via Independent Uncertainty Channels as Classical Quantum Error Correction”**

Rule: do not treat this title as an **M-tier** publication claim until an archived repro bundle exists (see `docs/REPRO_BUNDLE_TEMPLATE.md`).
