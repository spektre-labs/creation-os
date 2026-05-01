<!-- SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
     SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
     Source:        https://github.com/spektre-labs/creation-os-kernel
     Website:       https://spektrelabs.org
     Commercial:    spektre.labs@proton.me
     License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt -->
# v104 — Operator Search Tests

v104 has **no C kernel**; it is three pure-Python analysers
(`compute_operator_rcc.py`, `channel_analysis.py`, `hybrid_sweep.py`)
on top of v103's persisted σ-sidecar JSONL.  Two test surfaces:

1. **Operator-registry + synthetic oracle sanity** — built into
   `benchmarks/v104/check_v104.sh` and run by `make check-v104`.
   * imports `benchmarks/v104/operators.py` and asserts that the ten
     pre-registered candidates + `entropy` + `random` are present,
   * runs the RC-curve primitive through a 500-doc synthetic fixture
     where σ is an oracle (low on correct, high on wrong),
   * asserts oracle AURCC ≤ 0.10 and random AURCC > 0.15.
   This invariant guarantees the downstream analysis code has not
   regressed, independent of any real σ data.
2. **Preliminary run on v103 n=500 sidecar** — hand-runnable:
   ```sh
   export PYTHONPATH="$PWD/third_party/lm-eval:$PWD/benchmarks/v104:$PWD/benchmarks/v103"
   .venv-bitnet/bin/python benchmarks/v104/compute_operator_rcc.py
   .venv-bitnet/bin/python benchmarks/v104/channel_analysis.py
   .venv-bitnet/bin/python benchmarks/v104/hybrid_sweep.py --second sigma_max_token
   ```
   These three commands run in <60 s total on the existing v103
   sidecars and reproduce the preliminary tables in
   [docs/v104/RESULTS.md](../../docs/v104/RESULTS.md).  The n=5000
   tables come from `bash benchmarks/v104/run_n5000_sigma_log.sh`
   followed by the same three analysers pointed at the n=5000
   results directory.

## Merge-gate policy

`check-v104` runs only the synthetic sanity because it must work on
hosts without `.venv-bitnet`, without `lm-eval`, without the BitNet
GGUF, and without v103 sidecars.  Real numbers live in
`docs/v104/RESULTS.md` after `make bench-v104`.  Use
`COS_V104_PYTHON=...` to override the Python used by `check_v104.sh`
if `.venv-bitnet/bin/python` is not on your PATH.

## Debug tips

- `operator_comparison.md` reports "HURTS" in the `sig?` column if a
  σ candidate is **statistically significantly worse** than entropy
  after Bonferroni.  This is a useful signal that the operator is
  *anti*-informative — e.g. in v103 preliminary numbers, the
  `logit_std` single channel hurts arc_easy at p=0.0005.
- If every row in `operator_comparison.md` shows `p ≈ 2.0`, the
  bootstrap loop did not iterate — check `--n-boot` and the sidecar
  JSONL format.
- `channel_analysis.py` always reports `delta_vs_entropy = 0` for
  `entropy_norm` (channel 0) — that is the reference, not a bug.
