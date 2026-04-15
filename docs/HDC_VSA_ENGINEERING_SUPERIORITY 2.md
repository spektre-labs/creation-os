# HDC / VSA engineering advantages — literature map (and how Creation OS uses them)

This document answers: **what independent research already argues** about hyperdimensional / vector-symbolic computation versus dense neural pipelines — and **what this git repository actually demonstrates**. It is **not** a substitute for [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md); it **feeds** it with external anchors.

---

## 1. Field-level claims (peer-reviewed and widely cited)

| Theme | What the literature tends to claim | Representative sources |
|--------|-------------------------------------|-------------------------|
| **Efficiency footprint** | HDC can leverage **simple bitwise / counting primitives** and compact representations; comparisons to neural nets often highlight **energy efficiency** and **smaller model footprint**, while noting **limitations on the hardest learning tasks** without hybridization. | Ma & Jiao, *Hyperdimensional Computing vs. Neural Networks: Comparing Architecture and Learning Process*, [arXiv:2207.12932](https://arxiv.org/abs/2207.12932) (see abstract: efficiency / size vs sophistication trade-off; hybrid gains). |
| **Encoding is non-trivial** | Hypervectors are commonly **1K–10K** dimensions; **encoding strategy** (orthogonal vs correlated) materially affects downstream learning — the field treats this as first-class engineering, not a detail. | Aygun et al., *Learning from Hypervectors: A Survey on Hypervector Encoding*, [arXiv:2308.00685](https://arxiv.org/abs/2308.00685). |
| **Operator taxonomy** | **Binding / bundling** behavior, capacity, and decode quality differ across VSA families — selection must be **evidence-backed**, not aesthetic. | Schlegel, Neubert, Protzel, [arXiv:2001.11797](https://arxiv.org/abs/2001.11797) / [DOI 10.1007/s10462-021-10110-3](https://doi.org/10.1007/s10462-021-10110-3) — already summarized in [EXTERNAL_EVIDENCE_AND_POSITIONING.md](EXTERNAL_EVIDENCE_AND_POSITIONING.md). |
| **Classification / pipelines** | Recent **reviews** consolidate HDC classification methods and comparative behavior across benchmarks (useful when arguing “this is an active, maturing research line,” not a one-off hack). | *Classification using hyperdimensional computing: a review with comparative analysis*, [Artificial Intelligence Review (Springer)](https://doi.org/10.1007/s10462-025-11181-2) (DOI `10.1007/s10462-025-11181-2`). |
| **Robustness analysis** | Formal **robustness estimation** for hyperdimensional learning appears in the peer-reviewed track (Lipschitz-style analysis — use when you need “robustness” as **theorem / bound**, not vibes). | Yeung et al., *Lipschitz-based robustness estimation for hyperdimensional learning*, [Frontiers in Artificial Intelligence (2025)](https://doi.org/10.3389/frai.2025.1637105) · [PMC12486168](https://pmc.ncbi.nlm.nih.gov/articles/PMC12486168/). |

---

## 2. Industrial micro evidence (Hamming / POPCNT on real stacks)

Large-scale similarity stacks **invest in** XOR + population-count paths for Hamming-style distances because they matter at scale (instruction-set support, vectorized popcount). Example: FAISS community work on **VPOPCNT / popcount** acceleration for Hamming evaluation — [facebookresearch/faiss PR #4020](https://github.com/facebookresearch/faiss/pull/4020).

**Repository bridge:** Creation OS’s README microbench is **not** FAISS-scale engineering — it is a **transparent, reproducible** demonstration that the **inner loop** of bitwise distance is a **different cost class** than float GEMM-style similarity for the same nominal dimensionality, with ratios **auditable** from the stated encodings.

---

## 3. What Creation OS adds on top of “HDC is interesting”

| Layer | External field | This repository |
|-------|----------------|-----------------|
| **Coherence as one quantity** | VSAs give operators + similarity; many papers treat one task. | σ / Hamming-derived coherence is **one receipt language** across kernel demos, JEPA toy energy, oracle attention-as-σ, and (in the full tree) dispatch receipts — see README + [ANALYSIS.md](ANALYSIS.md). |
| **Gates before transformers** | HDC often wins on **small-data / on-device** motifs; frontier chat is different. | README + ANALYSIS state **explicit limitations**; the architectural bet is **cheap structure first**, then generation — aligned with Ma & Jiao’s “hybrid can beat pure HDC” direction without pretending the single C file is GPT-class. |
| **Publication hygiene** | Benchmark culture still mixes baselines. | [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md), [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md), harness vs native **two-row** rules in ANALYSIS. |

---

## 4. Discipline: do not upgrade literature into product claims

| Safe sentence | Unsafe sentence |
|---------------|------------------|
| “Ma & Jiao (2022) summarize empirical trade-offs between HDC and neural nets, including efficiency advantages on some axes.” | “HDC beats GPT-5 on reasoning because arXiv said so.” |
| “Yeung et al. (2025) give robustness estimation machinery for hyperdimensional learning.” | “This repo is provably robust to all adversaries.” |
| “FAISS invests in popcount paths for Hamming at scale.” | “We outperformed Meta’s retrieval stack.” |

---

## 5. One-paragraph slide summary (English)

> Hyperdimensional computing and vector symbolic architectures are a **mature research line** with peer-reviewed surveys on encodings (Aygun et al., 2023), comparative VSA operator taxonomies (Schlegel et al., 2021), and explicit neural-vs-HDC trade studies (Ma & Jiao, 2022). Creation OS **instantiates** that lineage for coherence-heavy systems: **bitwise** binding/bundling, **Hamming-derived** σ, and **measured** inner-loop cost tables — while keeping **frontier LM scores** on the **harness** path and toy demos in the **lab** evidence class.

---

*Spektre Labs · Creation OS · 2026*
