# v156 — σ-Paper · Unified Creation OS Paper

**Paper:** [`docs/papers/creation_os_v1.md`](../papers/creation_os_v1.md) · **Validator:** [`scripts/v156_paper_check.py`](../../scripts/v156_paper_check.py) · **Make:** `check-v156`

## Problem

The Creation OS corpus on Zenodo is ~ 80 papers. Each paper
addresses one kernel or one theme — invaluable as provenance, but
unreadable as a single entry point. A committee member, a
reviewer, or a first-time reader needs **one** document that
describes the whole stack: theory, architecture, measurements,
formal guarantees, limitations, reproducibility.

v156 ships that document and a machine-checkable gate so the
paper cannot silently rot into a stub.

## σ-innovation

There is no new σ channel in v156 — the innovation is that
**every number in the paper reduces to a `make` target**. The
paper's §10 Reproducibility block is a copy-paste recipe that any
reader can run on a MacBook Pro M4 to reproduce §4's measurements
byte-for-byte.

## Structure (enforced)

The paper must contain these 12 top-level sections
(`scripts/v156_paper_check.py` fails if any is missing):

1. Abstract
2. Introduction — why LLM wrappers are not enough
3. σ-theory — `K_eff = (1 − σ)·K`, eight channels, `σ_product`
4. Architecture — seven layers silicon → metacognition
5. Measurements — v111.1 Bonferroni, v152 corpus σ drop,
   merge-gate receipt
6. Comparisons — vs OpenAI Agents SDK, LangGraph, CrewAI,
   OpenCog Hyperon
7. Living system — v124 continual + v125 σ-DPO + v144 RSI under
   v148 σ-sovereign
8. Formal guarantees — v123 TLA+ and v138 Frama-C WP
9. Limitations — single model family, benchmark breadth,
   single-node, sim-only embodiment
10. Future — v134.1 neuromorphic, v129.1 mesh, v152.1 corpus,
    v149.1 MuJoCo, v156.1 arxiv + Zenodo DOI
11. Reproducibility — exact `make` commands + artefact manifest
    + bibtex
12. Acknowledgments

## Merge-gate

`make check-v156` runs `scripts/v156_paper_check.py` and asserts:

* The paper file exists at `docs/papers/creation_os_v1.md`.
* Word count ≥ 1200.
* All 12 headings above are present (regex-matched).
* Six required references are present: canonical repo URL,
  `benchmarks/v111`, the `@misc{creation_os_v1}` bibtex key,
  `CC-BY-4.0`, `SCSL-1.0`, and `BitNet-b1.58`.
* The paper explicitly names σ_product, v152, v153, and v154.

## v156.0 vs v156.1

* **v156.0** — the paper file + the offline validator. No DOI, no
  arXiv submission, no Zenodo upload.
* **v156.1** — the arXiv + Zenodo submission, with the resulting
  DOI back-linked into `CITATION.cff` and a second pass of
  author-response revisions.

## Honest claims

* **The paper is a *companion* paper**, not a replacement for the
  ~80 Zenodo notes. Those notes remain the per-kernel provenance;
  this paper integrates them.
* **Every quantitative claim is reproducible on commodity
  hardware.** The v111.1 Bonferroni numbers on `truthfulqa_mc2`
  are tier-1 (archived live run, reproducible via
  `bash benchmarks/v111/run_matrix.sh`). The v152 corpus σ-drop
  is tier-0 (deterministic 16-paper fixture). The 16 416 185 PASS
  / 0 FAIL receipt is tier-0 deterministic across every `check-vN`.
  Every tier label is disclosed in §4 and §10 of the paper.
