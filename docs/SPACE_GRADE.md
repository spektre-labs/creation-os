# Creation OS — Space-Grade Engineering (Lab Framing)

This note is **aspirational documentation**: it links flight-software *ideas* (safe mode,
redundancy, conservatism under uncertainty) to Creation OS’s **σ-gate verdicts** and
multi-probe **cascade**. It is **not** a DO-178C / NASA certification, **not** a statement that
the repository has completed independent IV&V, and **not** a substitute for your program’s
qualification evidence. For headline and benchmark claims see `docs/CLAIM_DISCIPLINE.md`.

## Safe mode and ABSTAIN

In mission operations, entering a conservative **safe mode** means the vehicle defers to a small,
trusted envelope and waits for ground input when autonomy is uncertain.

In Creation OS (lab), the σ-gate’s **ABSTAIN** band is the **stop-and-wait** outcome: refuse to
act on a high-σ read until the envelope is clarified. **ACCEPT** is nominal continuation;
**RETHINK** is a degraded path that still permits constrained progress. This is a *control
analogy*, not a claim of formal hazard analysis parity with a flight project.

## Redundancy, voting, and σ-cascade

**ARBITER**-style architectures combine asynchronous redundant observations and vote or filter
before committing an action. Creation OS exposes a **σ-score cascade** (entropy, dependency /
ICR-family channels, optional LSD, spectral summaries, etc.) that **aggregates** independent probes
before a pooled verdict. The mapping is **illustrative**: probe wiring is lab-configurable and the
Python stack is not a radiation-hardened multi-core voter.

## “Power of 10” checklist (engineering culture, not auto-proof)

Gerard Holzmann’s *Power of 10* rules are a **C coding discipline** for high-consequence software.
The table below relates *themes* to Creation OS **targets**. Only items backed by the actual tree
(e.g. `sigma_gate.h` style checks, `make check`) should be treated as **repo facts**; the rest are
**design goals** for contributors.

| # | Power-of-10 theme | Creation OS pointer |
|---|-------------------|---------------------|
| 1 | Restrict control flow | Prefer structured control in kernel paths; avoid ad-hoc `goto` in `sigma_gate.h` |
| 2 | Bounded loops | Prefer fixed iteration caps in hot paths; `python/cos/space_grade.py` warns on some `while` shapes |
| 3 | No unbounded dynamic growth in critical paths | **Target** for kernel-adjacent C; Python host probes are heap-allocated by design |
| 4 | Small routines | **Target** for `sigma_gate.h` helpers; Python modules vary widely |
| 5 | Assertions / invariants | σ clipped to `[0,1]` on the lite path; extend invariants deliberately in C |
| 6 | Limited data visibility | Prefer minimal linkage surfaces in C headers |
| 7 | Check every return | Treat gate `verdict` as part of the API contract at call sites |
| 8 | Few preprocessor features | Keep `sigma_gate.h` portable C89 |
| 9 | Restrict indirect control | Kernel-style direct calls preferred in the header narrative |
| 10 | Compile clean with warnings | Use repo `make check` / CI gates on what this tree actually enforces |

## Python “space-check” (static sniff only)

`cos space-check` runs **heuristic** AST checks (function length, `exec`/`eval`, naive `while`
shape). It does **not** prove behavior, timing, or coverage; use it as a **local reminder**, not a
qualification artifact.

## Triple Modular Redundancy (TMR) toy

`SpaceGradeChecker.tmr_check` implements a **majority vote** across several equal-length probe
lists. It illustrates the *redundancy narrative* above, not hardware TMR.

---

*Spektre Labs · lab framing only · 2026*
