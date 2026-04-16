# Creation OS v51 — Cognitive Architecture (scaffold)

> **Tier (honest):** integration scaffold (C) + interpretive diagram (I).
> This document describes the *wiring shape* for Creation OS v51. It does
> **not** claim "AGI-complete" as a measured artifact — the measured
> components live in v33–v50 and their individual `check-v*` self-tests.
> See [`../WHAT_IS_REAL.md`](../WHAT_IS_REAL.md) before citing anything
> in this file as a product claim.

## The six-phase loop

`src/v51/cognitive_loop.c` exposes one scaffold function:

```c
void v51_cognitive_step(const char *query,
                        const float *logits, int n_logits,
                        const v51_thresholds_t *thresholds,
                        v51_cognitive_state_t *out);
```

Phases (pure-C, deterministic, no I/O):

| # | Phase     | What it does (scaffold)                                                    | Upstream lab    |
|---|-----------|----------------------------------------------------------------------------|-----------------|
| 1 | Perceive  | Dirichlet-evidence σ from logits, or synthetic σ=0.5 when `logits == NULL` | v34 decompose   |
| 2 | Plan      | σ → action ∈ {ANSWER, THINK, DEEP, FALLBACK, ABSTAIN} + budget + N-samples | v40 syndrome    |
| 3 | Execute   | Static response string selected by action/difficulty                       | v41 / v44 proxy |
| 4 | Verify    | Defense-blocked when ABSTAIN; final σ = initial σ (until engine wired)     | v47 / v48 / v49 |
| 5 | Reflect   | `calibration_gap = |final - initial|`; correction flag when THINK/DEEP      | v45             |
| 6 | Learn     | Scalar reward `= 1 - σ_total` (0.5 on ABSTAIN); experience logged          | v42 buffer      |

Thresholds are ordered and monotonic in [0, 1]:

```
answer_below = 0.20   < think_below = 0.50
                      < deep_below  = 0.75
                      < fallback_below = 0.90
                      ≤ 1.00 (abstain)
```

## σ-gated agent

`src/v51/agent.c` wraps the cognitive loop into a bounded agent:

- `v51_agent_run()` stops immediately on `ACTION_ABSTAIN` (no "try anyway" loop).
- Each step evaluates at most one candidate tool through
  `v51_sandbox_check()`:
  - deny when `σ.total ≥ sandbox.sigma_deny_above` (default 0.75)
  - deny unknown tools when a policy list is set (fail-closed)
  - otherwise allow
- `(tool, σ, success)` triples are ring-buffered into `v51_memory_t`
  (cap = `V51_AGENT_MEMORY_MAX = 64`).
- "Goal reached" is a stub predicate keyed off σ falling below
  `answer_below` — the scaffold explicitly does not pretend to evaluate
  arbitrary goals.

## One-picture view

```
╔══════════════════════════════════════════════════════════════╗
║                    CREATION OS v51                           ║
║           AGI-complete integration scaffold                  ║
╠══════════════════════════════════════════════════════════════╣
║                                                              ║
║  ┌─────────────┐  ┌──────────────┐  ┌──────────────────┐     ║
║  │ chat CLI    │  │ web UI       │  │ OpenAI API       │     ║
║  │ (aspirat.)  │  │ (static HTML)│  │ (v44 proxy)      │     ║
║  └──────┬──────┘  └──────┬───────┘  └────────┬─────────┘     ║
║         └────────────────┼───────────────────┘               ║
║                          ▼                                   ║
║  ┌───────────────────────────────────────────────────────┐   ║
║  │              COGNITIVE LOOP (v51)                     │   ║
║  │  Perceive → Plan → Execute → Verify → Reflect → Learn │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║    ┌─────────────────────┼─────────────────────┐             ║
║    ▼                     ▼                     ▼             ║
║  ┌──────────┐  ┌──────────────────┐  ┌──────────────┐        ║
║  │ System 1 │  │ System 2         │  │ Deep Think   │        ║
║  │ (fast)   │  │ (budget forcing) │  │ (best-of-N)  │        ║
║  │ σ < 0.20 │  │ 0.20 ≤ σ < 0.50  │  │ σ ≥ 0.50     │        ║
║  └──────────┘  └──────────────────┘  └──────────────┘        ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              σ-MEASUREMENT LAYER                      │   ║
║  │  entropy │ top-margin │ dirichlet │ stability │ CLAP  │   ║
║  │  v34 epistemic/aleatoric decomposition                │   ║
║  │  v45 introspection + calibration                      │   ║
║  │  v40 threshold / syndrome decoder                     │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              DEFENSE LAYER (v48 + v49)                │   ║
║  │  input filter │ σ-gate │ anomaly │ sandbox │ audit    │   ║
║  │  red-team harness │ fail-closed │ DO-178C artifacts   │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              INFERENCE ENGINE (external)              │   ║
║  │  BitNet 2B4T via bitnet.cpp │ ~0.4 GB │ CPU-only      │   ║
║  │  v46: SIMD σ-from-logits │ v35: σ-guided spec decode  │   ║
║  │  (lab-tier energy numbers in docs/BENCHMARK_PROTOCOL) │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              LEARNING LAYER (scaffold)                │   ║
║  │  v42: σ-guided self-play │ v43: σ-guided distillation │   ║
║  │  v51: ring experience buffer (cap 64)                 │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              INTEGRATION LAYER                        │   ║
║  │  v36: MCP σ server │ v33: agent router │ v44: proxy   │   ║
║  │  Claude Desktop · Cursor · Cline · any MCP client     │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              HARDWARE LAYER (labs)                    │   ║
║  │  v37: FPGA σ-pipeline │ v38: ASIC tile (post-Efabless)│   ║
║  │  v39: memristor mapping │ SymbiYosys formal at depth  │   ║
║  └───────────────────────┬───────────────────────────────┘   ║
║                          │                                   ║
║  ┌───────────────────────┼───────────────────────────────┐   ║
║  │              VERIFICATION LAYER                       │   ║
║  │  v47: Frama-C / ACSL σ-kernel │ v49: DO-178C-aligned  │   ║
║  │  v50: benchmark rollup │ MC/DC driver │ binary audit  │   ║
║  │  `make certify` = one command runs the assurance pack │   ║
║  └───────────────────────────────────────────────────────┘   ║
║                                                              ║
║  Foundation: K_eff = (1 − σ) · K                             ║
║  One equation. Every layer. Paper to silicon (lab tier).     ║
║  Spektre Labs · Helsinki · ORCID 0009-0006-0903-8541         ║
╚══════════════════════════════════════════════════════════════╝
```

## What v51 is / is not

- **Is:** a scaffolded integration layer (`src/v51/cognitive_loop.*`,
  `src/v51/agent.*`, `config/v51_experience.yaml`, `src/v51/ui/web.html`,
  `scripts/v51/install.sh`) plus this architecture diagram. Verified by
  `make check-v51` (self-test only).
- **Is not:** a running transformer, a signed public binary, a hosted
  service, or a regulator certification. The one-line installer is a
  *dry-run* until a signed release ships.
- **Tier discipline:** every claim in this document maps to an entry in
  [`docs/WHAT_IS_REAL.md`](../WHAT_IS_REAL.md). Do not screenshot this
  diagram without citing the tier table.

## Reproduce

```bash
make check-v51          # v51 integration scaffold self-test
./creation_os_v51 --architecture   # prints the banner
make v50-benchmark      # upstream rollup (STUB eval slots)
make certify            # upstream DO-178C-aligned assurance pack
```
