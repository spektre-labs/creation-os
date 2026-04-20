# arXiv submission manifest — Creation OS v2.0 Omega

This file is the **human-readable** twin of the machine-checked
manifest emitted by `make check-sigma-arxiv`.  The kernel
`creation_os_sigma_arxiv` is the source of truth; regenerate at
any time with:

```bash
make creation_os_sigma_arxiv && ./creation_os_sigma_arxiv
```

Per [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md), the
submission act itself (uploading to arxiv.org, publishing a
Zenodo DOI) is a one-shot, human-owned step that leaves an
external trail.  This document pins everything that can be
mechanically verified *before* that step runs.

## Metadata

| Field | Value |
|---|---|
| Title | Creation OS: σ-Gated Sovereign Inference with Formal Guarantees and Substrate-Agnostic Design |
| Author | Lauri Elias Rainio |
| ORCID | [0009-0006-0903-8541](https://orcid.org/0009-0006-0903-8541) |
| Affiliation | Spektre Labs, Helsinki |
| Category | `cs.LG` (Machine Learning) |
| License | SCSL-1.0 OR AGPL-3.0-only |
| Release tag | `v2.0.0-omega` |
| DOI (reserved) | `10.5281/zenodo.RESERVED-ON-SUBMIT` |

## Abstract

We present Creation OS, a σ-gated sovereign-inference operating
system whose kernel enforces `declared==realized` as a measurable
runtime invariant, with a partial formal backing in Lean 4 +
Frama-C/Wp and a substrate-agnostic design spanning CPU,
neuromorphic, and photonic back-ends.

## Anchor files

The SHA-256 of every anchor below is pinned by the kernel.  If any
file changes, `make check-sigma-arxiv` fails and the submission
must be re-prepared.

- `docs/papers/creation_os_v1.md`
- `docs/papers/creation_os_v1.tex`
- `CHANGELOG.md`
- `include/cos_version.h`
- `LICENSE`

## Submission checklist

1. [ ] `make merge-gate` passes (all 53 σ-pipeline checks).
2. [ ] `make check-sigma-formal-complete` shows `ledger_matches_truth: true`.
3. [ ] `make check-cos-version-genesis` confirms v2.0.0 Omega coherence.
4. [ ] `make check-sigma-arxiv` prints byte-stable manifest.
5. [ ] Anchor SHA-256s in `./creation_os_sigma_arxiv` match this document.
6. [ ] Author runs `arxiv-submit` (manual) against
       `docs/papers/creation_os_v1.tex`.
7. [ ] Zenodo DOI minted against the tagged commit; replace
       `doi_reserved` slot in `arxiv.c` and re-run the check.

## Claim discipline

- No public announcement of "paper accepted" until arXiv moderation
  returns an `arXiv:` identifier in writing.
- No claim of "formally verified" until
  `make check-sigma-formal-complete` reports `effective_discharged ≥ 6`.
- Benchmark numbers in the paper must match the ledger entries
  in `docs/ledger/` via the anchor SHA-256 chain.
