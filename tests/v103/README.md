# v103 σ-τ-Sweep — Tests

v103 is **not a C kernel**; it is a post-hoc analysis layer on top of
v101 + v102.  It has two test surfaces:

1. **Synthetic RC-metric sanity check** — built into
   `benchmarks/v103/check_v103.sh` and run by `make check-v103`.
   Feeds 500 synthetic documents into `compute_rc_metrics.py`'s
   curve-building primitives, asserts that an oracle signal (σ low on
   correct, σ high on wrong) yields AURCC ≤ 0.10 and Cov@0.95 ≥ 0.60,
   while a random signal yields AURCC > 0.15.  This is the invariant
   that `compute_rc_metrics.py` must preserve regardless of dataset
   size or seed: an oracle must beat a coin flip by at least 5 × on
   AURCC.
2. **Real σ-log smoke** — exercised by
   `bash benchmarks/v103/run_sigma_log.sh --limit 50`.  It:
     - builds `creation_os_v101` in real mode (needs
       `COS_V101_BITNET_REAL=1`-linked libs),
     - runs lm-eval on a 50-doc prefix of one MC task through the
       v103 backend,
     - writes the σ sidecar JSONL and the lm-eval samples JSONL,
     - runs `compute_rc_metrics.py` on the output.
   The smoke is not part of `make merge-gate` because it requires the
   full BitNet GGUF + venv; it lives under `make bench-v103` along
   with the full 500-doc × 3-task run.

## Running the checks

```sh
# merge-gate subset (always green on any C11 host)
make check-v103

# real smoke (needs .venv-bitnet + BitNet GGUF + built creation_os_v101)
bash benchmarks/v103/run_sigma_log.sh --limit 50
.venv-bitnet/bin/python benchmarks/v103/compute_rc_metrics.py \
    --results benchmarks/v103/results \
    --tasks arc_easy

# full run (≈ 2 h on Apple M3 / Metal / bitnet.cpp i2_s)
make bench-v103
```

## Invariants asserted by the synthetic sanity check

| condition                                   | value                |
|----------------------------------------------|----------------------|
| oracle AURCC (σ low → correct)              | ≤ 0.10               |
| random AURCC                                | > 0.15 (floor = 1 − acc) |
| oracle Cov@0.95                             | ≥ 0.60               |
| random Cov@0.95                             | (uncontrolled — varies by seed) |

If the invariants break, `compute_rc_metrics.py`'s curve / integral /
threshold math has regressed.  Investigate before trusting any real
RESULTS.md numbers.
