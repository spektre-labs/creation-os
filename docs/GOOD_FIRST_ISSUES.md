# Good first issues

Pick any of the tickets below. Each is scoped to a single PR, has
a clear merge-gate target, and has been vetted by the maintainer to
be achievable in an afternoon by a competent C / Python engineer.

## Scope-contained, kernel-additive

1. **Add a new σ-channel: kurtosis-excess**
   `src/v101/sigma.c` already computes `σ_kurtosis`; add
   `σ_kurtosis_excess` = max(0, σ_kurtosis − 3.0) as the ninth
   channel, wired into the geomean in `σ_product`. Update
   `check-v101-sigma-channels`. **Merge-gate.** The 9-channel
   geomean must remain in `[0,1]` and the existing Bonferroni
   numbers in `benchmarks/v111/` must not regress.

2. **Port v134 σ-spike to WebAssembly**
   `src/v134/spike.c` is pure C11 with no OS calls on the hot path.
   Add a `wasm/v134/` subdirectory with an `emcc` build target and
   a simple `index.html` stub that shows a live σ-spike bar.
   **Merge-gate.** `make wasm-v134` produces a `spike.wasm` <
   64 KiB.

3. **Add Phi-3 to v109 multi-GGUF regression**
   `src/v109/registry.c` already supports BitNet + Llama; add a
   Phi-3 specialist entry, wire it into the v109 merge-gate, and
   confirm routing falls back gracefully when the GGUF file is
   absent. **Merge-gate.** `check-v109` continues to pass with
   and without a Phi-3 GGUF on disk.

4. **Translate v153 identity assertions to Finnish**
   `src/v153/identity.c` has 10 English assertions. Add a locale
   table keyed by ISO-639 `fi` (Finnish) in a NEW file
   `src/v153/identity_locale_fi.c` and expose a `--locale fi` CLI
   flag. **Merge-gate.** `check-v153` passes identically for both
   locales.

5. **Add a `make benchmark-report` target**
   Aggregate the JSON outputs of `check-v111`, `check-v143`,
   `check-v152`, and `check-v154` into a single Markdown file
   under `benchmarks/REPORT.md`. **Merge-gate.** The target is
   idempotent (rerunning produces byte-identical output) and
   deterministic.

## Scope-contained, infra

6. **Pre-commit hook: language-policy enforcement**
   Add a pre-commit hook that runs a stdlib-only Python script
   which rejects tracked prose containing non-English characters
   outside allow-listed regions (CITATION, author names). Wire it
   into `.pre-commit-config.yaml`.

7. **CI matrix: compile with `-fsanitize=undefined`**
   Add a GitHub Action that compiles the top ten flagship kernels
   with `-fsanitize=undefined,address` and runs their self-tests.
   The merge-gate must remain green.

## How to claim one

1. Read [`GOVERNANCE.md`](../GOVERNANCE.md) and
   [`CONTRIBUTING.md`](../CONTRIBUTING.md).
2. Comment on the corresponding GitHub issue saying "I'll take
   this." (If no issue exists, open one with the title copied from
   the item above.)
3. Open a PR when `make merge-gate` exits 0 on your branch.
4. Be specific about what you measured vs what you synthesized.

## Not a good first issue

- Refactoring any existing kernel.
- Changing any σ-threshold (`τ_*`) without a matching RFC.
- Any change that touches `LICENSE`, `LICENSE-SCSL-1.0.md`,
  `COMMERCIAL_LICENSE.md`, or `CLA.md`.
- Any change whose description starts with "just clean up …".
