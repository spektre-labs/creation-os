# Comparison — Creation OS vs the 2026 local-agent field

*A deliberately honest snapshot.  Last refreshed 2026-04-21.*

This page compares **Creation OS** with the four projects most often
suggested as alternatives for a *local, autonomous, self-improving*
assistant.  The goal is not to "win" on a star count — Creation OS is
smaller by orders of magnitude — but to make explicit **what we
measure that the others do not**, so a reader can pick the right tool
for the right problem.

Every evidence claim below links to the file that substantiates it in
this repo.  See [docs/CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) for
the separation between **measured** numbers
(`make bench`, `make check-truthfulqa`, …), **arithmetic** ratios
(encoded into the kernel headers), and **external** positioning.

---

## At a glance

| Capability                         | Creation OS (v3.0.0)                                | OpenClaw                     | Hermes Agent               | Ollama + Open WebUI        | Dify / n8n                 |
|------------------------------------|-----------------------------------------------------|------------------------------|----------------------------|----------------------------|----------------------------|
| Primary purpose                    | σ-gated cognitive runtime                           | Autonomous local agent       | Self-improving agent (MIT) | Local model server + UI    | Agent / workflow builder   |
| Runs fully local                   | yes (BitNet b1.58 2B, libc-only hot path)           | yes (any model)              | yes (any model)            | yes (any model)            | yes (orchestration only)   |
| **σ per token** (per-reply confidence) | **yes** — `sigma_logprob` + entropy + ppl + consistency ([multi_sigma.c](../src/sigma/pipeline/multi_sigma.c)) | no                           | no                         | no                         | no                         |
| **Conformal risk guarantee**       | **yes** — `α` chosen by user, τ auto-loaded from `~/.cos/calibration.json` ([conformal.c](../src/sigma/pipeline/conformal.c), [SCI-1]) | no                           | no                         | no                         | no                         |
| **ABSTAIN capability**             | **yes** — visible in `cos chat` receipt            | no                           | no                         | no                         | no                         |
| **RETHINK loop**                   | **yes** — bounded by `--max-rethink`, counted in receipt | no                           | no                         | no                         | no                         |
| Formal proofs                      | **Lean 4: 6/6 discharged · Frama-C/Wp: 15/15** ([hw/formal/](../hw/formal/), [docs/FULL_STACK_FORMAL_TO_SILICON.md](FULL_STACK_FORMAL_TO_SILICON.md)) | none                         | none                       | none                       | none                       |
| Self-improving loop                | Ω-loop: engram ↔ ICL ↔ distill ([agi/](../src/agi/), AGI-1..5) | —                            | **yes** — "self-created skills" | —                          | —                          |
| **Reasoning / joule metric**       | **yes** — `cos-benchmark --energy` (ULTRA-7, [benchmarks/final5/energy.txt](../benchmarks/final5/energy.txt)) | no                           | no                         | no                         | no                         |
| **Coherence monitoring**           | **yes** — Lagrangian-style `dσ/dt`, `K_eff`, state ([coherence.c](../src/sigma/pipeline/coherence.c), ULTRA-9) | no                           | no                         | no                         | no                         |
| Open-standard protocols            | **MCP (σ-MCP) + A2A (σ-A2A)** with per-peer trust EMA ([src/sigma/mcp/](../src/sigma/mcp/), [src/sigma/a2a/](../src/sigma/a2a/)) | MCP client                   | none                       | none                       | custom / webhooks          |
| Theory papers (repo-local)         | **77 CC BY-4.0 PDFs (~13 MB)** in [data/corpus/](../data/corpus/INDEX.md) | none                         | none                       | none                       | none                       |
| License                            | SCSL-1.0 OR AGPL-3.0-only                           | MIT                          | MIT                        | MIT / Apache-2.0           | Apache-2.0 (mixed)         |
| Hot path                           | C11 + libm, branchless Q16.16                       | Python / TypeScript          | Python                     | Go                         | Python / TypeScript        |
| Monthly cost                       | **€4.27 at 100 calls/day** ([cos cost](../README.md#measured)) | API-dependent                | API-dependent              | local, model-dependent     | hosted / self-hosted       |
| **GitHub stars (≈2026-04)**        | ~30                                                 | ~302 000                     | ~95 000                    | ~130 000                   | ~90 000                    |

> **Honest framing.**  OpenClaw, Hermes, Ollama and Dify are
> **vastly larger communities**.  Creation OS trades that reach for
> something none of them offer: **an end-to-end σ-gate with formal
> proofs at the base of the stack and an on-disk selective-prediction
> guarantee at the top**.  When those properties matter — high-stakes
> domains, offline deployment, audit-ready evidence — Creation OS is
> the right tool; when they do not, one of the other four is the
> better choice.

---

## What competitors do well

- **OpenClaw** — the broadest autonomous-agent ecosystem.  Every
  model, every platform, huge plugin library, cloud *and* local.
  The right tool when you need an agent that can *do* things; it is
  not the right tool when you need a principled bound on *when* it
  shouldn't.
- **Hermes Agent** — the most ambitious self-improvement loop in
  open source as of 2026-Q2: "self-created skills cut time 40 %".
  Impressive, but no formal bound on correctness of the emergent
  skills and no per-reply uncertainty signal.
- **Ollama** — the cleanest local inference stack.  Install, run,
  done.  It is *exactly* what it claims to be — a runtime — and
  makes no attempt to measure σ, plan, or abstain; that is a
  feature of scope.
- **Dify / n8n** — best-in-class workflow orchestration.  They
  compose agents rather than think, and are the right layer to put
  Creation OS behind as a σ-gated "safety tool".

## Where Creation OS is the only option

1. **"Please do not answer if you are not sure."**
   Only Creation OS exposes ABSTAIN as a first-class terminal action
   (see [cos_chat.c `print_receipt_polished`](../src/cli/cos_chat.c)
   and [reinforce.c](../src/sigma/pipeline/reinforce.c)).
2. **"Give me a risk-controlled τ for this domain."**
   SCI-1 conformal calibration
   ([conformal.c](../src/sigma/pipeline/conformal.c)) computes τ such
   that, on a held-out calibration set, `P(σ ≥ τ | wrong) ≤ 1 − α`.
   We verify this holds on TruthfulQA 817
   ([benchmarks/final5/coverage_curve.json](../benchmarks/final5/coverage_curve.json)).
3. **"Show me the proof."**
   Lean 4: 6/6 measurement-lemma obligations discharged with zero
   `sorry` and zero Mathlib.  Frama-C/Wp: 15/15 pointer / Q16.16
   invariants on the hot path.
   ([docs/FULL_STACK_FORMAL_TO_SILICON.md](FULL_STACK_FORMAL_TO_SILICON.md))
4. **"How many joules did that cost?"**
   ULTRA-7 energy pass
   ([benchmarks/final5/energy.txt](../benchmarks/final5/energy.txt))
   emits reasoning/joule on the same harness as throughput so they
   are commensurable.
5. **"Is the session drifting?"**
   ULTRA-9 coherence conservation
   ([coherence.c](../src/sigma/pipeline/coherence.c)) tracks
   `sigma_mean(1h)` and `dσ/dt`, classifying each session as
   STABLE / DRIFTING / AT_RISK.
6. **"Let me read the theory."**
   77 CC BY-4.0 PDFs vendored at [data/corpus/](../data/corpus/INDEX.md).
   Every kernel's motivation is one `cd` away from the source.

## How to verify the numbers in this page

```bash
# Full stack gate — every claim above has a check target here.
make merge-gate

# Selective-prediction receipts
make check-conformal            # SCI-1 + risk bound
make check-coverage-curve       # TruthfulQA 817 coverage vs σ
make check-multi-sigma          # SCI-5 ensemble

# Energy + suite
make check-ultra                # ULTRA-1..11 including --energy
./cos-benchmark --suite --energy --coverage-curve

# Live demo (no network)
./cos demo                      # 60-second σ-chat showcase
./cos chat --verbose            # full ULTRA stack per turn
```

## Not included in this comparison

- **Hosted LLM APIs** (GPT-*, Claude, Gemini) — different category:
  they are models, not runtimes; using one of them is *the question*
  Creation OS is answering, not a comparable answer.
- **Enterprise agent platforms** (LangGraph Cloud, CrewAI Cloud,
  BentoML, Modal) — different deployment model; Creation OS is a
  kernel, not a platform.

---

> Creation OS is small by design.  We will not inflate the scoreboard
> by counting features we do not ship or proofs we have not run.
> What we *do* count, we count exactly once, with a link to where it
> was counted.  1 = 1.
