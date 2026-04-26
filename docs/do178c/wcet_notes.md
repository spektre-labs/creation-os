# WCET notes (σ stack — informative)

DO-178C-style safety programs often require **bounded execution time** evidence for on-path software. Creation OS does **not** ship a WCET certificate or a schedulability proof for host deployments (heterogeneous CPUs, thermal throttling, OS jitter).

## What maintainers can do

1. **Measure** wall time or cycle deltas on a **named** target using fixed compiler flags (see [../REPRO_BUNDLE_TEMPLATE.md](../REPRO_BUNDLE_TEMPLATE.md) for measured-claim hygiene).
2. **Budget** end-to-end latency in the chat pipeline using existing adaptive compute hooks (`adaptive_compute`, time budgets in `cos_chat`) rather than pretending a single microsecond constant fits all hosts.
3. **Formal / static** evidence for specific kernels remains scoped to what the tree actually proves (for example v138 σ-gate ACSL shape + optional Frama-C WP per `benchmarks/v138/check_v138_prove_frama_c_wp.sh`).

## Host-specific constants

Defining a literal `COS_SIGMA_WCET_US` without per-CPU calibration would be **misleading** on laptops, servers, and ASIC simulators. Any future in-tree WCET instrumentation should be gated behind an explicit compile flag (for example `CREATION_OS_WCET_INSTRUMENT`) and documented with measurement methodology, not asserted as a universal bound.
