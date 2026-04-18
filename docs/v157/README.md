# v157 — σ-Community · Contribution Infrastructure

**Governance:** [`GOVERNANCE.md`](../../GOVERNANCE.md) · **Templates:** [`.github/ISSUE_TEMPLATE/`](../../.github/ISSUE_TEMPLATE/) · **Good-first-issues:** [`docs/GOOD_FIRST_ISSUES.md`](../GOOD_FIRST_ISSUES.md) · **Linter:** [`scripts/v157_contributing_lint.sh`](../../scripts/v157_contributing_lint.sh) · **Make:** `check-v157`

## Problem

Opening Creation OS to outside contributors without diluting the
merge discipline that carried the stack from v6 to v153. Two
failure modes to prevent:

1. **Silent claim inflation.** A PR adds "kernel X achieves Y" to
   the README without adding a falsifiable gate. The 1 = 1
   invariant (declared = realized) is eroded.
2. **Scope creep.** A PR refactors existing kernels "for
   cleanliness." Kernel-per-capability is the architecture;
   merging two kernels is a structural change that requires an
   RFC + BDFL sign-off.

## σ-innovation

The community infrastructure is itself σ-gated. The linter
(`scripts/v157_contributing_lint.sh`) runs on every CI build and
fails if any required section is absent or if any issue template
is missing a required prompt. This means a contributor cannot open
a "feature request" that skips the σ-contract section, and a
maintainer cannot merge a PR that deletes the `1 = 1` invariant
from `GOVERNANCE.md`.

## Artefacts

| File | Purpose | Contract |
|---|---|---|
| `GOVERNANCE.md` | BDFL + merge requirements + 1 = 1 | names BDFL, lists 7 merge reqs, states 1 = 1 |
| `CONTRIBUTING.md` | step-by-step PR workflow | names `merge-gate`, `CLA`, `LANGUAGE_POLICY` |
| `CLA.md` | Contributor License Agreement | must exist |
| `SECURITY.md` | disclosure channel | must exist |
| `.github/ISSUE_TEMPLATE/bug_report.md` | bug reports | pre-existing |
| `.github/ISSUE_TEMPLATE/feature_request.md` | new kernel proposal (short) | prompts σ-contract + merge-gate + v0/v1 split |
| `.github/ISSUE_TEMPLATE/kernel_proposal.md` | new kernel proposal (RFC) | prompts σ-contract |
| `.github/ISSUE_TEMPLATE/benchmark_submission.md` | benchmark runs | enforces tier-0/1/2 label |
| `.github/PULL_REQUEST_TEMPLATE.md` | PR checklist | pre-existing |
| `docs/GOOD_FIRST_ISSUES.md` | 7 scoped tickets | mentions kurtosis, WASM, Phi-3, Finnish |

## Merge-gate

`make check-v157` runs `bash scripts/v157_contributing_lint.sh`
and asserts every bullet above. The linter is coreutils-only
(`grep`, `[ -f ]`) so it runs in any CI environment.

## v157.0 vs v157.1

* **v157.0** — every community file is in-tree and linted.
* **v157.1** — GitHub Discussions is activated with five
  categories (Architecture, Benchmarks, Kernels, Corpus, Hardware)
  and the first wave of community contributors has opened at
  least one PR per category.

## Honest claims

* **This is infrastructure, not culture.** Files and templates
  only ensure the *form* of a healthy contribution process; the
  *substance* (response times, tone, maintainer bandwidth) is
  built by Spektre Labs over time.
* **The BDFL model is explicit.** Creation OS is not a consensus-
  driven project. Lauri Rainio and Spektre Labs Oy hold the
  commercial rights (per the SCSL-1.0 license) and the BDFL rights
  (per GOVERNANCE.md). Contributors accept this by signing CLA.md.
* **Refactoring is not a good first issue.** The good-first-issues
  list is entirely *additive* (new σ channel, new WASM build, new
  locale, new aggregator target). This is deliberate — v157.0 is
  not the moment to invite structural changes from strangers.
