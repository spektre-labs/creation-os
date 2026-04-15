# Glossary — Creation OS (portable tree)

Canonical discipline terms live in [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md). This page is a **fast lookup** for readers crossing from ML, robotics, or safety docs.

| Term | Meaning here |
|------|----------------|
| **BSC** | Binary Spatter Codes (Kanerva lineage): high-dimensional **binary** representations; bind ≈ XOR; bundle ≈ MAJ; similarity ≈ Hamming / POPCNT. |
| **HV** | Hypervector: a fixed-width bit pattern representing state or content; in this repo often **4096 bits** (`D=4096`, `W=64` words of 64 bits). |
| **σ (sigma)** | Coherence readout used in the reference program: squared normalized Hamming fraction between two HVs (see README *What is BSC?*). **Not** LM logit σ unless explicitly wired. |
| **XOR bind** | Per-word XOR of two hypervectors; association / binding operator in demos. |
| **MAJ bundle** | Majority superposition of three (or more) words per lane; superposition operator in demos. |
| **POPCNT** | Population count on XOR lanes; feeds Hamming distance and σ. |
| **D** | Dimensionality in bits for the documented benchmark harness (`D=4096`). |
| **W** | Number of 64-bit words covering `D` bits (`W = D/64` here). |
| **GEMM path (benchmark)** | Float32 dot-product style cosine similarity proxy in `core/gemm_vs_bsc.c`: per trial, accumulate dot and norms over `D` floats. |
| **BSC path (benchmark)** | XOR + POPCOUNT across `W` words; σ-style scalar from Hamming mass (see code). |
| **Op proxy** | Instruction-count **story** for the microbench (printed ratios); not a hardware performance counter. |
| **Evidence class** | One of the labeled buckets in CLAIM_DISCIPLINE (arithmetic, measured microbench, invariant, harness, lab demo, …). **One headline number → one class.** |
| **Receipt** | Loggable artifact proving a check ran: stdout line, test pass, JSON row, CI log — discrete enough to audit. |
| **Plane A / B / C** | Systems map in [ANALYSIS.md](ANALYSIS.md): inference + superkernel; Python / MLX orchestration; native M4 dylib receipts. Optional in minimal checkouts. |
| **SK8** | Superkernel generation referenced in ANALYSIS (token kernel state, σ, manifold); not fully duplicated in `creation_os_v2.c`. |
| **GDA** | Geometric / grounded codebook bridge: BSC structures around inference (see ANALYSIS); `gda/` in this tree holds M4-oriented C modules. |
| **Oracle (§3)** | N-gram toy language demo in C — proves σ-as-attention **mechanism**, not chat quality. |
| **JEPA (§6)** | Toy predictive loop with codebook energy; not a generalization claim without held-out eval. |
| **Noether / Genesis (§8)** | Toy conservation narrative; not lattice QCD. |
| **Crystal Lock** | Demo of tamper sensitivity on identity / XOR chains (§4 narrative). |
| **Forbidden merge** | Claim pattern that MUST NOT appear in prose (e.g., microbench × MMLU); see CLAIM_DISCIPLINE §2. |
| **Publication-hard** | README standard: cross-examination-ready binding of claims to commands, code, or evidence class. |
| **Lab demo (C)** | Evidence class: smoke / pedagogy in C; not harness frontier scores. |
| **`make check`** | `standalone` + `test`: structural gate used in CI. |
| **`make bench`** | Runs GEMM vs BSC microbench; host-dependent wall time — see [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md). |
| **`cos_hv_hamming_hw`** | Same Hamming count as `cos_hv_hamming`, **AArch64 NEON** fast path (prefetch + `vcnt`); scalar elsewhere — see [NATIVE_COHERENCE_NEON.md](NATIVE_COHERENCE_NEON.md). |
| **`make bench-coherence`** | Batch Hamming gate microbench (`bench/coherence_gate_batch.c`). |
| **Parliament (`cos_hv_bit_parliament`)** | Odd-K per-bit majority fusion of agent hypervectors — [HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md](HYPERVECTOR_PARLIAMENT_AND_RETRIEVAL.md). |
| **Argmin bank (`cos_hv_argmin_hamming_rows_hw`)** | Row scan for nearest HV under Hamming — NEON on AArch64 — same doc. |
| **`make bench-agi-gate`** | Parliament + memory-bank argmin microbench (`bench/hv_agi_gate_neon.c`). |

---

*Spektre Labs · Creation OS · 2026*
