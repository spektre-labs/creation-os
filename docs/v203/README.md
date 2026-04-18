# v203 σ-Civilization — institution registry + K_eff collapse detector

## Problem

Long-horizon coordination across organisations,
jurisdictions, and communities needs a measurable signal
for institutional health and an explicit ledger for the
public-good vs private-good split. v203 provides both,
and it is the layer where the SCSL licence strategy
becomes first-class.

## σ-innovation

Six institutions × three licence classes in the v0 fixture:

| id | name               | licence | archetype           |
|----|--------------------|---------|---------------------|
| 0  | spektre_labs       | SCSL    | stable              |
| 1  | open_commons_dao   | SCSL    | collapsed + recovered |
| 2  | aicorp_enterprise  | CLOSED  | stable              |
| 3  | fragile_corp       | CLOSED  | permanent collapse  |
| 4  | personal_os        | PRIVATE | stable              |
| 5  | legacy_provider    | CLOSED  | collapsed + recovered |

Collapse detector (v193 coherence signal):
`K_crit = 0.60`, 4-tick window.
Four consecutive ticks with `σ > K_crit` flag a **collapse**;
four consecutive ticks back below `K_crit` register a
**recovery**.

Continuity score (closed form):

```
stable                  → 1.00
recovered               → 0.40 + 0.50 · recoveries/collapses
permanently_collapsed   → 0.10
```

Public-good ledger:

| licence | public share | private share |
|---------|--------------|---------------|
| SCSL    | 100 %        | 0 %           |
| CLOSED  | 20 %         | 80 %          |
| PRIVATE | 0 %          | 100 %         |

SCSL public-ratio strictly exceeds CLOSED in the fixture
by `≥ 0.10` — the 1=1 strategy is a property of the
ledger, not a policy.

Inter-layer contradiction check: four queries cross v199
verdicts against v202 profile-practice verdicts. When
they disagree, `σ_contradiction = 0.73` and the query
escalates to REVIEW.

## Merge-gate

`make check-v203` runs
`benchmarks/v203/check_v203_civilization_collapse_detect.sh`
and verifies:

- self-test PASSES
- 6 institutions, 3 licence classes present
- ≥ 1 collapse + ≥ 1 recovery
- continuity strictly orders `stable > recovered >
  permanent_collapse`
- ≥ 1 inter-layer contradiction escalated to REVIEW
- SCSL public-ratio strictly > CLOSED public-ratio + 0.10
- chain valid + byte-deterministic

## v203.0 (shipped) vs v203.1 (named)

| | v203.0 | v203.1 |
|-|--------|--------|
| registry | in-memory | v115 memory-backed |
| revenue | synthetic | live SCSL revenue streaming |
| σ trace | fixture | live v193 coherence link |
| contradictions | 4 fixture queries | live v199 norm feed |

## Honest claims

- **Is:** a deterministic 6-institution demonstrator that
  detects collapses, classifies continuity, escalates
  cross-layer contradictions, and proves the SCSL
  public-good ratio dominates the closed-licence ratio.
- **Is not:** a live institutional registry, SCSL revenue
  processor, or global civilisation dashboard — those
  ship in v203.1.
