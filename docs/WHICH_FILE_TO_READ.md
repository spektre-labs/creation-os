# Which file to read first? (reviewer map)

This repository is intentionally **not** “one program” — it is a **teaching spine** plus a set of **standalone harnesses** with explicit evidence tiers.

## If you are reviewing the repo

Start here:

- **`README.md`**: high-level story + how to run the merge gate.
- **`docs/CLAIM_DISCIPLINE.md`**: what counts as measured vs projected vs not-claimed.
- **`docs/WHAT_IS_REAL.md`**: tier tags for headline claims (the “no accidental mixing” list).

Then run:

```bash
make reviewer
```

If `make reviewer` is green, the “review surface” is consistent: portable checks pass, the v26 harness passes, v2 self-test exists, and the tier-tag discipline file parses.

## v2 vs v26 (the common confusion)

| Item | What it is | What it is not |
|------|------------|----------------|
| **`creation_os_v2.c`** | Single-file **bootstrap demo**: shows the BSC algebra and 26 illustrative modules. Builds to `./creation_os`. | Not the merge gate; not an LM harness; not a performance claim source. |
| **`creation_os_v26.c`** | The **flagship merge-gate harness** for the “kernel narrative” (prints **184/184 PASS** on `--self-test`). | Not “a full product”; still a lab demo tier (C) unless paired with external repro bundles / silicon proofs. |

Rule of thumb: **use v2 to understand the primitives**, use **v26+ to verify the harness discipline**.

## Directory map (what lives where)

| Path | Contents | Reviewer question |
|------|----------|------------------|
| `core/` | BSC primitives, NEON helpers, shared headers | Are the primitives internally consistent? |
| `src/` | v27–v29 support modules (tokenizer, GGUF import, σ channels, HTTP bits) | Are the harness dependencies minimal and test-backed? |
| `docs/` | Claim discipline, evidence ladders, reviewer guidance | Do the public claims map to reproducible artifacts? |
| `benchmarks/` | External/optional scripts and SKIP stubs | Are benchmarks clearly marked as optional / not shipped? |
| `rtl/` / `hw/` | Optional formal/RTL mirror artifacts | Are hardware claims properly separated from CI outputs? |
| `integration/` | Optional glue headers / experiments | Are experiments clearly labeled as not load-bearing? |
| `experimental/` | Non-merge-gate experiments (may require extra tooling) | Is it isolated from the main evidence chain? |
| `web/` | Static local demo pages (optional) | Is the UI honest about scope and non-claims? |
| `gda/` | Exploratory “GDA” fragments | Is it clearly non-load-bearing, and is duplication managed? |

## Reviewer checklist (fast)

- `make reviewer` passes.
- `docs/WHAT_IS_REAL.md` tier tags match the actual shipped artifacts.
- Anything that looks like a “headline number” is either:
  - **M** (measured) with an in-repo harness command, or
  - **T/P/E** with an explicit pointer and “not measured here”, or
  - explicitly **retired**.

