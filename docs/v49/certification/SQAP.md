# Software Quality Assurance Plan (SQAP) — v49 lab

## Quality objectives

- **No silent tier mixing:** public claims must match `docs/WHAT_IS_REAL.md`
- **Fail-closed documentation:** if a tool is missing, `make certify` must **SKIP** loudly (never pretend green proofs)

## Audits

- **Automated:** `make certify` aggregates scripts + tests
- **Manual:** review diffs on σ-critical path files; scrutinize any change to thresholds / abstention policy

## Defect handling

Security-sensitive findings should be reported per `SECURITY.md` (if applicable) and fixed with a regression test when feasible.
