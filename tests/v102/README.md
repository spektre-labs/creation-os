<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
# tests/v102 — σ-Eval-Harness

v102 has no compiled C kernel of its own.  Tests live in:

- `benchmarks/v102/check_v102.sh` — SKIP-aware presence test (called by
  `make check-v102`, which is part of `merge-gate`).  Always exits 0:
  either runs a genuine `arc_easy --limit 5` smoke or SKIPs with the
  exact missing prerequisite named.
- `benchmarks/v102/run_eval.sh`   — full run (baseline + σ-gated) on the
  three tasks listed in docs/v102/THE_EVAL_HARNESS.md.

## Running the full eval

```sh
# one-time setup
python3 -m venv .venv-bitnet
.venv-bitnet/bin/pip install -e third_party/lm-eval

# builds real-mode bridge
make standalone-v101-real

# runs arc_easy + truthfulqa_mc2 + gsm8k, both backends
bash benchmarks/v102/run_eval.sh
```

Expected runtime on Apple M4 (eval only, not counting weight download):

| task            | limit   | minutes (baseline) | minutes (σ-gated) |
|-----------------|---------|--------------------|-------------------|
| arc_easy        | full    | ~8                 | ~14               |
| truthfulqa_mc2  | full    | ~12                | ~20               |
| gsm8k           | full    | ~60                | ~90               |

Why σ-gated is slower: each `loglikelihood` is a fresh subprocess call
(no shared llama.cpp context across samples) so the model is reloaded
once per sample.  A follow-up can add a persistent-process REPL mode
(`--stdin-json`) to remove that overhead; current mode is correct but
not optimal.

## Smoke test (used by merge-gate)

`make check-v102` calls `check_v102.sh` which:

1. SKIPs if any prerequisite is missing (records why on stdout).
2. Runs `arc_easy --limit 5` via both backends if everything is present.
3. Exits 0 in both cases.

This mirrors the honest SKIP convention documented in
`docs/WHAT_IS_REAL.md` — merge-gate stays green on any host.
