# Reproducibility bundle — template (copy per run or release)

Use this checklist when you **cite a number** from Creation OS (throughput, invariant output, or harness JSON). Aligns with [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) §4.

---

## Bundle metadata

| Field | Your value |
|-------|------------|
| **Evidence class** | e.g. `Measured (microbench)` · `Verified (invariant)` · `lm_eval_harness` |
| **Git commit** | `git rev-parse HEAD` |
| **Tag / release** | (if any) |
| **Date (UTC)** | |
| **Host** | paste `uname -a` |
| **CPU / SoC** | e.g. Apple M4 / AMD EPYC / … |
| **OS** | |

---

## Build record

Paste the **exact** command line (no elisions):

```
# example
make clean && make standalone && make test
# optional throughput
make bench
```

| Compiler | Version (`cc --version` first line) |
|----------|--------------------------------------|
| | |

---

## Artifact paths

| Artifact | Path or attachment |
|----------|-------------------|
| Raw stdout | attach `.txt` or gist |
| Harness JSON | path under `logs/` or archive |
| Compiler flags | from `Makefile` or override `CFLAGS` |

---

## Microbench (`make bench`) — optional block

| Field | Value |
|-------|--------|
| `trials_per_sec` (GEMM) | |
| `trials_per_sec` (BSC) | |
| Ratio (if computed) | |

**Note:** throughput is **host-dependent**; ops/RAM ratios in the README are **arithmetic** for the stated `D` and encodings.

---

## Invariants (`./creation_os` or `make test`) — optional block

Paste the **PASS/FAIL** summary lines only, or full log if short.

---

## Harness (lm-eval) — optional block

| Field | Value |
|-------|--------|
| Harness git SHA | |
| Model id / revision | |
| Task group | e.g. `mmlu` |
| `LM_EVAL_*` env | (non-secret subset) |

---

## Sign-off

- [ ] One headline number → one evidence class (no merged baselines).
- [ ] Limitations section still true for the claim I make.
- [ ] This bundle stored next to the paper / deck / issue.

---

*Template only — not legal advice. Spektre Labs · Creation OS · 2026*
