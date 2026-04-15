# Benchmark protocol — §7 / `make bench` (GEMM vs BSC)

**Evidence classes involved:** **Arithmetic (structure)** for memory and op-proxy ratios; **Measured (microbench)** for wall-clock seconds and trials/sec.

**Source of truth:** `core/gemm_vs_bsc.c` (`cos_gemm_bench_main`), linked from `bench/gemm_vs_bsc.c` and invoked via `make bench`.

---

## 1. Fixed parameters (do not silently change when comparing to README)

| Symbol | Value | Role |
|--------|-------|------|
| `D` | 4096 | Bit dimensionality of each HV lane in the BSC path |
| `W` | 64 | `D/64` — number of `uint64_t` words per vector |
| `trials` | 100000 | Outer repeat count for both loops |

Float vectors `fa`, `fb` each hold **D** `float` elements. Bit vectors `ba`, `bb` each hold **W** words.

---

## 2. What is counted (op proxy — not hardware PMU)

**GEMM / cosine proxy per trial (printed line `op_proxy_per_trial GEMM`):** inner loop implements dot + accumulation into `na`, `nb` in one pass over `i ∈ [0, D)`. The printed proxy uses **D×6** as the documented multiply-add style story (see stdout: three accumulates per `i` in the listing — match code when auditing).

**BSC path per trial:** for each trial, XOR each of `W` words and POPCOUNT; printed proxy **W×2** (XOR + popcount story per word).

**Memory per vector (printed `memory_bytes_vector`):** GEMM `D×4` bytes (float32); BSC `D/8` bytes (packed bits). Ratio **32×** follows from these encodings.

**Ratios 192× (ops)** and **32× (RAM)** are **arithmetic** claims tied to this counting story — independent recount must agree or the prose must change.

---

## 3. Timing methodology

- **Clock:** `CLOCK_MONOTONIC` when available; else `clock()` (see `mono_sec()` in source).
- **Order:** full GEMM loop timed, then full BSC loop timed (same process, same `trials`).
- **Compiler sink:** results written to `volatile` accumulators `gr`, `br` so naive DCE does not erase work.
- **RNG:** `time(NULL)` seeds `uint64_t rng` for vector fill — **not** a fixed seed; repeat runs differ in inputs but structure is identical.

**Host dependence:** `wall_s`, `trials_per_sec_*`, and ratio **GEMM/BSC wall** are **Measured (microbench)** — archive per [REPRO_BUNDLE_TEMPLATE.md](REPRO_BUNDLE_TEMPLATE.md).

---

## 4. Minimum reproduction archive (when citing throughput)

1. Git commit SHA (this repo).  
2. Exact command: `make bench` or path to built `gemm_vs_bsc` + invocation.  
3. Compiler and flags (Makefile defaults: `cc -O2 -march=native -Wall -std=c11`).  
4. `uname -a` and CPU model string.  
5. **Full stdout** including `core/gemm_vs_bsc: trials=…` header and all printed lines.

---

## 5. What this benchmark does *not* prove

- **Semantic equivalence** between float cosine and σ (different precision and task semantics — README *Limitations*).  
- **End-to-end LM latency** or **training throughput**.  
- **Optimality** of either implementation vs vendor BLAS or hand-vectorized kernels.

---

## 6. Falsifiers (microbench class)

- Reproducible build where printed **memory_bytes_vector** or **op_proxy** lines disagree with an independent line-by-line recount of the source.  
- Documented `trials` or `D` changed in code but README table still cites old constants without update.

---

*Spektre Labs · Creation OS · 2026*
