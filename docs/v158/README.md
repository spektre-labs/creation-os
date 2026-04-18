# v158 — σ-v1.0 · Release Gate

**Checklist:** [`docs/RELEASE_v1_0.md`](../RELEASE_v1_0.md) · **Changelog:** [`CHANGELOG.md`](../../CHANGELOG.md) · **Validator:** [`scripts/v158_release_checklist.sh`](../../scripts/v158_release_checklist.sh) · **Make:** `check-v158`

## Problem

Creation OS has v1.0.0 worth of engineering. Shipping it is a
coordination problem: a release tag, a GitHub release, five
package ecosystems, a paper, a changelog, checksums, demo videos,
and five communication surfaces. The failure mode we protect
against is a **release that claims to include something it does
not** — a tag that references a Hugging Face adapter nobody has
trained yet, a changelog entry that describes an unmerged PR, a
packaging manifest that the merge-gate has not validated.

v158 is the release gate: a machine-checkable checklist that parses
itself and refuses to call the release "green" until every
engineering (A), documentation (B), and packaging (C) item is
backed by an artefact the merge-gate has already verified.

## σ-innovation

There is no new σ channel. The σ-innovation is **separation of
concerns between in-tree items and BDFL-driven items**:

* **A + B + C** — engineering, documentation, packaging gates.
  All 18 items must be `[x]`-ticked in-tree AND backed by a
  present artefact (paper file, CHANGELOG section, packaging
  manifest, docs page, etc.). The linter enforces both.
* **D + E** — release-day actions and communication. All 16
  items must be *enumerated* in-tree but must NOT be `[x]`-ticked
  until the BDFL actually performs the action (git tag, twine
  upload, Docker push, r/LocalLLaMA thread, HN Show-HN, etc.).
  The linter refuses to merge a PR that pre-ticks any D or E
  item. This is the in-tree equivalent of a release witness: the
  repo is honest about what has been done and what has not.

## Release coordinates (v1.0.0)

| Field | Value |
|---|---|
| Tag | `v1.0.0` |
| Branch | `release/v1.0.0` |
| Kernel count | 153 |
| Paper | `docs/papers/creation_os_v1.md` |
| Merge-gate receipt | 16 416 185 PASS / 0 FAIL on M4 |
| Licenses | SCSL-1.0 / AGPL-3.0-only |

## Merge-gate

`make check-v158` runs `bash scripts/v158_release_checklist.sh`
and asserts:

1. Every required section (A/B/C/D/E + Release message) is present.
2. A1..A7 are ticked; every A item maps to a `check-vN` target.
3. B1..B6 are ticked; every B item maps to a present artefact
   (paper, changelog v1.0.0 section, GOVERNANCE, good-first-issues,
   `docs/v154..v158/README.md`).
4. C1..C5 are ticked; every C item maps to a present packaging
   manifest (`python/pyproject.toml`, Homebrew formula, release
   Dockerfile, three HF cards, npm package.json + launcher).
5. D1..D10 are enumerated and NOT ticked (BDFL-driven).
6. E1..E6 are enumerated and NOT ticked (BDFL-driven).
7. The canonical release message is intact: `pip install
   creation-os`, `153 kernels.`, `github.com/spektre-labs/creation-os`.

## v158.0 vs v158.1

* **v158.0** — the gate (this doc + the checklist + the linter).
  Everything BDFL-independent is green.
* **v158.1** — the actual release. The BDFL signs the tag, cuts
  the GitHub release, pushes to PyPI / Docker / HF / npm,
  submits the paper to arXiv + Zenodo, and ticks D1..D10 in a
  follow-up commit. Communication items E1..E6 are ticked as
  each channel is posted to, and only then.

## Honest claims

* **v1.0.0 is a gate-complete release, not a "weights shipped"
  release.** The Hugging Face `bitnet-2b-sigma-lora` card is a
  placeholder for v1.1; its training contract is documented but
  its weights are not uploaded. `docs/RELEASE_v1_0.md` says so
  explicitly under "Scope-lock".
* **No D or E items are ticked in-tree.** Anyone who wants to
  falsify a claim about the v1.0.0 release (e.g. "has the HN post
  been published?") looks at `docs/RELEASE_v1_0.md` on `main`;
  if the box is still `[ ]`, the action has not been performed.
  This is the canonical single source of truth.
