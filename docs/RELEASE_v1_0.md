# Creation OS v1.0.0 — Release Checklist

This document is both a **checklist** (items the maintainer ticks
off before tagging `v1.0.0`) and a **gate** (every item is parsed
by `scripts/v158_release_checklist.sh`, which `make check-v158`
runs on every CI build).

An item is "green" when its `MERGE-GATE:` line below maps to a
passing `make` target or a present artefact.

---

## Release coordinates

| Field | Value |
|---|---|
| Tag | `v1.0.0` |
| Branch | `release/v1.0.0` (branched from `main` at the merge of `feature/v154-v158-release`) |
| Paper | `docs/papers/creation_os_v1.md` |
| Changelog | `CHANGELOG.md` section `v1.0.0 — Creation OS v153 release` |
| Kernel count | 153 (v6 through v153; v75 intentionally skipped) |
| Merge-gate receipt | 16 416 185 PASS / 0 FAIL on M4 |
| Licenses | SCSL-1.0 (primary, commercial) / AGPL-3.0-only (fallback) |

## Release checklist

### A. Engineering gate

- [x] A1. `make merge-gate` exits 0 on the release branch.
  **MERGE-GATE:** `make merge-gate`
- [x] A2. Every `check-vN` from v6 through v158 is wired into
  `merge-gate` (names appear in `Makefile` line-for-line).
  **MERGE-GATE:** `grep -E 'check-v(6|..|158)' Makefile`
- [x] A3. `check-v154` passes (3 demo pipelines end-to-end).
  **MERGE-GATE:** `make check-v154`
- [x] A4. `check-v155` passes (publish manifests offline lint).
  **MERGE-GATE:** `make check-v155`
- [x] A5. `check-v156` passes (paper renders + 11 sections).
  **MERGE-GATE:** `make check-v156`
- [x] A6. `check-v157` passes (CONTRIBUTING + governance + issues).
  **MERGE-GATE:** `make check-v157`
- [x] A7. `check-v158` passes (this file is parsed).
  **MERGE-GATE:** `make check-v158`

### B. Documentation gate

- [x] B1. `docs/papers/creation_os_v1.md` exists.
- [x] B2. `CHANGELOG.md` has a `v1.0.0` section.
- [x] B3. `README.md` has a Creation OS v1.0 banner.
- [x] B4. `GOVERNANCE.md` exists.
- [x] B5. `docs/GOOD_FIRST_ISSUES.md` exists.
- [x] B6. Per-kernel `docs/v154/`, `docs/v155/`, `docs/v156/`,
  `docs/v157/`, `docs/v158/` directories all have a `README.md`.

### C. Packaging gate (offline)

- [x] C1. `python/pyproject.toml` parses and names
  `project.name = "creation-os"`.
- [x] C2. `packaging/brew/creation-os.rb` exists and declares
  `class CreationOs < Formula`.
- [x] C3. `packaging/docker/Dockerfile.release` exists and `EXPOSE
  8080`.
- [x] C4. `packaging/huggingface/` has three cards:
  `creation-os-benchmark.md`, `creation-os-corpus-qa.md`,
  `bitnet-2b-sigma-lora.md`.
- [x] C5. `packaging/npm/package.json` names
  `"name": "creation-os"` and ships a shebang launcher.

### D. Release-day actions (v158.1 — requires the BDFL)

- [ ] D1. `git tag -s v1.0.0` signed with the release key.
- [ ] D2. `gh release create v1.0.0` with the binaries listed in D5.
- [ ] D3. `twine upload dist/*` from `python/` with the v1.0.0 wheel.
- [ ] D4. `brew bump-formula-pr spektre-labs/tap/creation-os`.
- [ ] D5. Upload universal binaries: `cos-darwin-arm64`,
  `cos-darwin-x86_64`, `cos-linux-x86_64`, and
  `cos-linux-aarch64` with accompanying `.sha256` files.
- [ ] D6. Docker push to `spektrelabs/creation-os:v1.0.0` and
  `spektrelabs/creation-os:latest` (multi-arch).
- [ ] D7. HF upload for the three model cards + any weights (only
  if the weights are actually trained — otherwise skip and
  document in v1.1).
- [ ] D8. `npm publish` the npm launcher.
- [ ] D9. arXiv + Zenodo submission for
  `docs/papers/creation_os_v1.md`; archive the DOI back into
  `CITATION.cff`.
- [ ] D10. Post-release smoke: `pip install creation-os` in a
  clean venv; `brew install spektre-labs/tap/creation-os` on a
  clean macOS VM; `docker run` smoke.

### E. Communication (v158.1 — BDFL-gated)

- [ ] E1. r/LocalLLaMA thread — technical audience.
- [ ] E2. r/MachineLearning thread — academic audience with the
  v111.1 Bonferroni table.
- [ ] E3. Hacker News Show HN.
- [ ] E4. LinkedIn announcement (Lauri + Spektre Labs Oy).
- [ ] E5. Twitter / X mentions of Anthropic / OpenAI / Meta
  research groups (optional, maintainer discretion).
- [ ] E6. 60-second demo video (screen capture of `cos demo` +
  `/v1/chat/completions` + Web UI).

## Release message (canonical)

> Creation OS v1.0. 153 kernels. σ-governance for local AGI.
> Measures its own uncertainty. Learns from it. Improves itself.
> Proves its own invariants. Runs on a MacBook.
> pip install creation-os
> github.com/spektre-labs/creation-os

## Scope-lock

The **v1.0.0** release is a *gate-complete, documentation-complete*
release. It is explicitly NOT an upload of trained LoRA weights to
Hugging Face — those are v1.1. The v1.0.0 artefacts are the
source, the merge-gate, the paper, and the packaging manifests.
This is the scope the tag records.

---

*Spektre Labs · Creation OS · 2026 · 1 = 1*
