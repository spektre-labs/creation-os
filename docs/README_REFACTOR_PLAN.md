# README refactor plan — Creation OS

**Purpose:** keep the root `README.md` honest, short, and maintainable while deep documentation stays in `docs/` and version catalogues do not rot in duplicate.

**Repository language:** English only — [`LANGUAGE_POLICY.md`](LANGUAGE_POLICY.md).

---

## 1. Executive diagnosis

**Strengths**

- Strong opening: install path, **v111 frontier matrix** with explicit negatives, TruthfulQA table with caveats, invariant line.
- **Limitations** and lab-demo disclaimers align with [`CLAIM_DISCIPLINE.md`](CLAIM_DISCIPLINE.md).

**Problems**

1. **Length** — One scroll stack served newcomers, integrators, and thesis readers at once.
2. **Duplication** — Version-banded tables overlapped [`DOC_INDEX.md`](DOC_INDEX.md), per-version `docs/vNN/README.md`, and [`CHANGELOG.md`](../CHANGELOG.md).
3. **Drift risk** — Pinned badge numbers and long tables without generation drift from reality.

**Mitigation shipped (iteration 1)**

- Moved the **v112–v278+** catalogue verbatim to [`SURFACE_VERSIONS.md`](SURFACE_VERSIONS.md) and linked it from the root README.
- Added a **Contents** block at the top of the README with anchor links.

---

## 2. Phase 0 (same week)

- [x] Extract long surface tables → `docs/SURFACE_VERSIONS.md`.
- [x] README **Contents** + pointer section with stable anchor `surface-versions-v112v278`.
- [ ] Optional: one **quickstart** block (clone + `./scripts/install.sh` + `./cos chat`) duplicated text removal.
- [ ] Replace **§** in ASCII art with `Sec.` where still present.

---

## 3. Phase 1 (~1 month)

- Target **≤800 lines** on `README.md` by moving or summarising the large **σ-pipeline / “what it does”** prose blocks into `docs/` (keep README as index + measured claims + build + limits).
- Add `scripts/gen_readme_fragment.sh` (optional) to refresh badge / receipt lines from CI logs.
- Weekly **link check** on README + `DOC_INDEX.md`.

---

## 4. Phase 2 (~1 quarter)

- Machine-readable **manifest** (YAML) for version rows → generated fragment for README or for `SURFACE_VERSIONS.md` only.
- **Committee bundle** script: slim README + thesis architecture + frontier matrix PDF.

---

## 5. Success metrics

| Metric | Target |
|--------|--------|
| README length | **≤800 lines** after phase 1 trim. |
| Broken links (README + hub) | **0** in CI. |
| Claim hygiene | Every headline number maps to **one** evidence class (see `CLAIM_DISCIPLINE.md`). |

---

## 6. Automation sketch (`gen_readme_fragment.sh`)

1. Run `make merge-gate` (or read last CI log) and parse PASS/FAIL totals.
2. Emit `README.fragment.md` with badge Markdown + receipt one-liner.
3. `README.md` includes a fenced region `<!-- FRAGMENT:BEGIN -->` … `<!-- FRAGMENT:END -->` replaced by CI or `make readme-sync`.

This file is the living checklist for README work; update it when phases complete.
