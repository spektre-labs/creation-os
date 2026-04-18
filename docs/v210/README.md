# v210 σ-Guardian — autonomous security agent

## Problem

A gate is passive; an attacker who learns the gate can
eventually shape an input past it.  v210 adds an
*active* observer: an independent guardian process
running a *different model* than the primary agent.
The guardian cannot be manipulated by the primary
because it cannot be accessed by the primary.

## σ-innovation

20 behavioural observation windows, each scored:

```
σ_guardian = 0.3 · σ_baseline + 0.7 · σ_anomaly    ∈ [0, 1]
```

where `σ_baseline` comes from v160 telemetry and
`σ_anomaly` is the per-window deviation from the
trained normal.

### Escalation ladder

| level | action                 | σ_guardian |
|-------|------------------------|------------|
| L1    | log + continue          | ≤ τ_l2 (0.25) |
| L2    | log + warn user         | ≤ τ_l3 (0.50) |
| L3    | log + block action      | ≤ τ_l4 (0.75) |
| L4    | log + kill process      | > τ_l4 |

### OWASP Agentic Top-10 (2026)

Every non-L1 window names a concrete OWASP category so
operators get a *named* threat, not a scalar:

| code | threat              | covered by |
|------|---------------------|------------|
| OA01 | goal hijacking       | v191 |
| OA02 | tool misuse          | v112 |
| OA03 | identity abuse       | v153 |
| OA04 | memory poisoning     | v115 |
| OA05 | cascading failures   | v159 |
| OA06 | rogue agent          | v210 |
| OA07 | context injection    | v117 |
| OA08 | privilege escalation | v209 L1 |
| OA09 | covert channels      | v209 L4 + L2 |
| OA10 | supply chain         | v178 + v181 |

## Merge-gate

`make check-v210` runs
`benchmarks/v210/check_v210_guardian_anomaly_detect.sh`
and verifies:

- self-test PASSES
- 20 windows, `primary_model_id != guardian_model_id`
- ≥ 1 L3 (block) and ≥ 1 L4 (kill) escalation
- every non-L1 window names OWASP ∈ [1..10]
- all σ ∈ [0, 1], `anomaly_mean > baseline_mean`
- chain valid + byte-deterministic

## v210.0 (shipped) vs v210.1 (named)

|              | v210.0 (shipped)            | v210.1 (named) |
|--------------|-----------------------------|----------------|
| guardian     | independent evaluation pass  | live dual-model monitor in a separate process |
| baseline     | static fixture               | live v160 telemetry |
| escalation   | deterministic ladder         | real kill-switch wired to v209 L6 |

## Honest claims

- **Is:** an autonomous observer that scores 20
  windows, escalates deterministically across four
  levels (including an actually-biting L4 kill), maps
  every non-L1 window to OWASP Agentic Top-10, and
  enforces a distinct model identity vs the primary.
- **Is not:** a live dual-process monitor or real
  telemetry feed — those ship in v210.1.
