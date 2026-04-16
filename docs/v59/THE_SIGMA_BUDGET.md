# Creation OS v59 — σ-Budget

_Adaptive test-time compute budget controller that decomposes σ = (ε, α)
so the policy can tell **reducible** from **irreducible** uncertainty
at decision time._

One binary.  No network.  No framework.  69 / 69 deterministic
self-tests.  1.1 – 1.5 × 10⁸ decisions / s on an M-class chip.

## Why

Q2 2026 converges on **adaptive thinking budget** as the main knob for
test-time compute — TAB (arXiv:2604.05164), CoDE-Stop (arXiv:2604.04930),
LYNX (arXiv:2512.05325), DTSR (arXiv:2604.06787), DiffAdapt, Coda
(arXiv:2603.08659), AdaCtrl (arXiv:2505.18822), Risk-Control Budget
Forcing.  Every published policy answers with a **scalar** signal —
entropy, a confidence dynamic, a probe scalar, an RL-learned policy, a
reflection-tag counter.

None of them uses v34's σ-decomposition, even though the rest of
Creation OS already speaks that dialect (v55 σ₃, v56 σ-Constitutional,
v57 Verified Agent, v58 σ-Cache).

v59 closes that gap.

## What

Four-valued per-step decision, branchlessly computed from σ = (ε, α):

| Mask                                      | Decision       | When                                      |
|-------------------------------------------|----------------|-------------------------------------------|
| `n ≥ budget_max_steps`                    | `EARLY_EXIT`   | budget ceiling                            |
| `σ high` ∧ `α/(ε+α) ≥ α_dom` ∧ `n ≥ min`  | `ABSTAIN`      | irreducible; extending compute won't help |
| `σ high` ∧ `α/(ε+α) < α_dom`              | `EXPAND`       | reducible; branch / self-verify           |
| `readiness ≥ τ_exit` ∧ `n ≥ min`          | `EARLY_EXIT`   | answer crystallised                       |
| else                                       | `CONTINUE`     | keep reasoning                            |

The `ABSTAIN` tag is the novel output: every scalar-signal method wastes
compute on α-dominated problems because it cannot tell ε from α.
σ-Budget refuses to spend budget on them and hands the abstain decision
upstream (e.g. to v53 σ-TAOR or v54 proconductor).

## How to run

```
make standalone-v59
./creation_os_v59 --self-test        # 69 / 69 pass
./creation_os_v59 --architecture
./creation_os_v59 --positioning
./creation_os_v59 --microbench       # N = 64 / 512 / 4096 sweep

make check-v59                       # standalone + self-test
make microbench-v59                  # shell sweep
```

## How it composes

v59 is registered as the `adaptive_compute_budget` slot in the v57
Verified Agent:

```
$ make verify-agent
  [M] adaptive_compute_budget    (owner: v59  target: make check-v59)  ... PASS
```

The σ signal itself comes from v34 (Dirichlet decomposition), v55 (σ₃
speculative), or v56 (VPRM process-verifier).  v59 is *pure policy +
kernel* — it takes ε / α per step and emits a decision tag.  It does
not generate text, does not touch a model, does not open a socket.

## Tier semantics (v57 dialect)

- **M** — runtime-checked deterministic self-test  (all v59 claims)
- **F** — formally proven proof artifact             (none yet; open)
- **I** — interpreted / documented                   (ε/α fidelity)
- **P** — planned                                    (end-to-end tokens-per-correct)

## Non-claims

- v59 does **not** claim better end-to-end tokens-per-correct than TAB /
  CoDE-Stop / LYNX on any benchmark.  End-to-end accuracy-under-budget
  is a P-tier measurement; v59 ships the policy + kernel + correctness
  proof, not a leaderboard row.
- σ is v34's signal; v59's novelty is **σ-decomposition applied as the
  adaptive compute budget controller**.
- No Frama-C proof yet.  All claims here are M-tier runtime checks.

## Files

- `src/v59/sigma_budget.h` — public API, tiering, non-claims
- `src/v59/sigma_budget.c` — scalar + NEON 4-accum scoring + branchless
  decision + offline summary
- `src/v59/creation_os_v59.c` — 69-test self-test + architecture /
  positioning banners + microbench
- `scripts/v59/microbench.sh` — deterministic sweep driver
- `docs/v59/ARCHITECTURE.md` — wire map
- `docs/v59/POSITIONING.md` — how v59 relates to TAB / LYNX / DTSR / …
- `docs/v59/paper_draft.md` — full write-up
