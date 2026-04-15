# External evidence and positioning (English-only technical brief)

This note **does not** replace in-repo measurements. It **anchors** Creation OS / BSC choices to **peer-reviewed and widely used** sources so readers can see why the design direction is non-trivial relative to mainstream dense-vector practice.

---

## 1. Binary Spatter Codes and hyperdimensional computing (foundational literature)

| Source | What it establishes (relevant to this repo) | Link |
|--------|---------------------------------------------|------|
| **Kanerva — “Binary spatter-coding of ordered K-tuples”** (ICANN 1996; LNCS vol. 1112) | Introduces **binary spatter coding**: structured tuples encoded into long binary words; extraction without explicit field boundaries — the lineage from which **BSC** naming and **bitwise composition** intuition descend. | [Springer chapter (DOI 10.1007/3-540-61510-5_146)](https://doi.org/10.1007/3-540-61510-5_146) |
| **Kanerva — “Hyperdimensional Computing: An Introduction…”** (*Cognitive Computation*, 2009) | Positions **computing with very high-dimensional distributed vectors** as a coherent paradigm; complements the BSC-specific chapter above. | [Springer article (DOI 10.1007/s12559-009-9009-8)](https://doi.org/10.1007/s12559-009-9009-8) |
| **Schlegel, Neubert, Protzel — “A comparison of Vector Symbolic Architectures”** (*Artificial Intelligence Review*, 2021; arXiv:2001.11797) | Systematic comparison of **eleven VSA implementations**: binding/bundling taxonomies, capacity, and empirical behavior — standard reference for why **operator algebra + representation space** must be chosen deliberately, not hand-waved. | [arXiv:2001.11797](https://arxiv.org/abs/2001.11797) · [Springer AIR](https://doi.org/10.1007/s10462-021-10110-3) |

**Repository bridge:** `creation_os_v2.c` is a **pedagogical, executable** slice: XOR (bind), MAJ (bundle), POPCNT / Hamming-derived σ — the same **operator family** VSA/HDC literature discusses, compiled to a single TU you can audit without a framework stack.

---

## 2. Why the GEMM-vs-bitops contrast is structurally real (not a marketing trick)

Dense **cosine similarity** at dimension \(D\) with float32 vectors is textbook **\(O(D)\)** multiply-add work (plus norms if not pre-normalized). A **4096-bit** pattern comparison via XOR + population count is **\(O(D)\)** in bits but implemented as **word-level XOR + POPCNT** on modern CPUs — a **different constant factor and memory footprint** (512 bytes for two 4096-bit patterns vs 16 KiB for two `float32` vectors of length 4096).

That is standard computer arithmetic and microarchitecture territory; the README table makes the **chosen encodings and op counts explicit** so the ratios are **auditable** (see also [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md): arithmetic vs measured classes).

---

## 3. Evaluation norms for “frontier-class” claims (community standard)

| Norm | Why it matters | Link |
|------|----------------|------|
| **EleutherAI `lm-evaluation-harness`** | De facto standard for **few-shot LM benchmarks**; task configs, logging, and community review reduce ad-hoc score reporting. | [`github.com/EleutherAI/lm-evaluation-harness`](https://github.com/EleutherAI/lm-evaluation-harness) |
| **Harness hygiene** | The harness project’s own culture emphasizes **independent reruns**, prompt transparency, and careful logging — aligned with this repo’s **two-row** (harness vs native) discipline in [ANALYSIS.md](ANALYSIS.md). | [Harness documentation tree](https://github.com/EleutherAI/lm-evaluation-harness/tree/main/docs) |

**Repository bridge:** ANALYSIS sections *Evaluation modes* and *Parity program* name **reporting IDs** (`official_harness_hf_pytorch` vs `internal_mlx_genesis_native`) so Spektre/Creation OS numbers are not **accidentally** read as the wrong evidence class.

---

## 4. What external consensus **does** and **does not** prove about this repository

| Statement | Status |
|-----------|--------|
| HDC/VSA is an **active research paradigm** with serious surveys and benchmarks of **binding/bundling** behavior. | **True** — citations in §1. |
| Bitwise / Hamming-style similarity can be **extremely cheap** per comparison vs dense float pipelines for matched dimensionality and layout. | **True in principle**; this repo adds **measured** throughput for one harness (`make bench` / §7) — **host-dependent**, must be archived. |
| This standalone C file **replaces** frontier LLMs on MMLU/ARC without harness runs. | **False** — explicitly out of scope; see README *Limitations* and [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) *Forbidden merges*. |

---

## 5. Suggested citation line for papers / reports

> We treat Binary Spatter Codes as part of the Vector Symbolic Architecture / Hyperdimensional Computing family (Kanerva, 1996/1997; Kanerva, 2009; Schlegel et al., 2021). Our implementation is a compact C reference (`creation_os_v2.c`) with **explicit** microbenchmarks and **separate** harness-facing evaluation per EleutherAI `lm-evaluation-harness` norms.

---

## 6. Related in-repo brief (strategy + “why now”)

For a **curated map** of recent HDC/VSA literature (efficiency trade-offs, encoding surveys, robustness paper, FAISS Hamming note) tied to **what this repository does and does not claim**, see **[HDC_VSA_ENGINEERING_SUPERIORITY.md](HDC_VSA_ENGINEERING_SUPERIORITY.md)**.

---

*Spektre Labs · Creation OS · 2026 · repository text English-only*
