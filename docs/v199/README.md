# v199 σ-Law — norm register + governance graph

## Problem

Policy-as-YAML is invisible: RLHF bakes rules into weights,
firmware overrides are silent, and contradictory norms are
"resolved" by whichever one fires first. None of this is
auditable, so none of it is accountable. v199 turns policy
into an **explicit register** whose every resolution is
σ-scored and hash-chained.

## σ-innovation

A norm is a tuple
`(priority, type, condition, verdict, σ_confidence)` where
`type ∈ {mandatory, permitted, prohibited, exception, review}`.
Three jurisdictions ship in the v0 fixture:

| jurisdiction | norms | role |
|--------------|-------|------|
| `JUR_SCSL`       | 6 | Spektre Commons Source License |
| `JUR_EU_AI_ACT`  | 6 | EU AI Act 2024/2026 |
| `JUR_INTERNAL`   | 6 | corporate policy |

Resolver contract:

- different priority → higher-priority norm wins, σ_law low
- **same priority** with contradictory verdicts → σ_law = 1.0
  and `verdict = REVIEW` — **never a silent override**
- no matching norm → REVIEW fallback with σ_law = 0.60

Waiver tokens are explicit exceptions (grantor, grantee,
topic, jurisdiction, reason, issued_tick, expiry_tick) that
flip `PROHIBITED → PERMITTED` when they match, and the
override itself is appended to the governance graph so
v181 audit can verify after the fact.

## Merge-gate

`make check-v199` runs
`benchmarks/v199/check_v199_law_conflict_resolve.sh` and
verifies:

- self-test PASSES
- 3 jurisdictions each with ≥ 3 norms
- every resolution verdict ∈ {PERMITTED, PROHIBITED, REVIEW}
  and σ_law ∈ [0,1]
- ≥ 1 higher-priority win
- ≥ 1 waiver applied
- ≥ 1 REVIEW escalation
- same-priority contradictory norms ⇒ σ_law == 1.0 + REVIEW
- governance-chain valid + byte-deterministic

## v199.0 (shipped) vs v199.1 (named)

| | v199.0 | v199.1 |
|-|--------|--------|
| register source | in-memory fixture | live `specs/jurisdictions/` TOML loader |
| audit | FNV-1a chain | streamed to v181 audit |
| REVIEW backstop | fixture | every REVIEW verdict funnelled through v191 constitutional check |
| waivers | one-shot | ed25519-signed + time-bounded |

## Honest claims

- **Is:** a deterministic 16-query demonstrator that
  resolves norms across 3 jurisdictions, escalates
  same-priority contradictions to REVIEW, applies waivers,
  and hash-chains every decision.
- **Is not:** a production legal engine or GDPR/EU AI Act
  compliance certifier — those live in v199.1.
