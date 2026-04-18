# v168 — σ-Marketplace: skill / kernel / plugin registry with reputation σ

## Problem

`v145` skill library is local.  `v164` plugins are local.  A
sovereign OS needs a way for third parties to *share* their
work — but sharing arbitrary code with arbitrary users is how
ecosystems collapse into malware and slop.  The marketplace
needs a *signal* users can trust before they install.

## σ-innovation

- **Unified artifact model.**  Three kinds (`skill`, `kernel`,
  `plugin`) share the same record: id (owner/slug), version
  (semver), deterministic sha hex, author, stats, and
  `σ_reputation`.

- **σ_reputation formula.**
  `σ = clamp(0.6 · fail_rate + 0.3 · neg_report_rate + 0.1 ·
  benchmark_penalty, 0, 1)`.
  *Fail rate* dominates (test suite is ground truth); negative
  user reports contribute the next slice; a regressing
  benchmark delta adds the final penalty.  No tests yet? σ is
  pessimistic by default (`0.6`).

- **σ-gated install.**  Any artifact with `σ > 0.50` is
  **refused** without `--force`.  The refusal receipt spells
  out the threshold and suggests the override.  A forced
  install is logged as `status: "forced"` — the audit trail
  shows who bypassed which gate.

- **Publish pipeline.**  `cos_v168_publish(manifest)` appends
  the artifact, recomputes σ from the supplied stats, and
  stamps a deterministic SHA (FNV-1a demo; real SHA-256 in
  v168.1).  Duplicate ids are refused.

- **`.cos-skill` fixture validator.**  A skill must ship
  `{adapter.safetensors, template.txt, test.jsonl, meta.toml,
  README.md}`.  The validator returns a 1-based index for the
  first missing file.

## Merge-gate

`benchmarks/v168/check_v168_marketplace_publish_install.sh`
asserts:

1. self-test
2. registry carries 6 artifacts across all three kinds
3. every artifact has a 16-hex deterministic sha
4. a healthy artifact (σ < 0.15) installs with `status: "ok"`
5. a high-σ artifact refuses without `--force`
6. the same artifact installs with `--force`; `forced: true`
7. `--search skill` returns ≥ 2 matches, all containing
   "skill"
8. `.cos-skill` validator accepts a full set and rejects a
   missing README
9. byte-identical registry JSON across two runs

## v168.0 vs v168.1

| Aspect | v168.0 (this release) | v168.1 (planned) |
|---|---|---|
| Backend | Baked 6-artifact store | HTTPS registry at `registry.creation-os.dev` |
| SHA | FNV-1a 64-bit (16 hex) | Real SHA-256 over packed tarball |
| User reports | Fixed integers | Live ingestion from the fleet |
| Install | In-memory flag flip | Real download + verify + unpack |

## Honest claims

- **Lab demo (C)**: deterministic registry + reputation model
  + σ-gated install policy + fixture validator.
- **Not yet**: real HTTPS registry, real SHA-256, real
  publish pipeline, live user feedback ingestion.
- v168.0 *defines the semantic surface* third parties will
  meet; v168.1 *runs it* over the wire.
