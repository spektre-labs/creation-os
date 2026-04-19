# v250 — σ-A2A (`docs/v250/`)

Agent-to-Agent protocol integration.  v249 carries a σ on every
tool call; v250 carries a σ on every cross-agent envelope —
Agent Card, task delegation, negotiation, federation.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Agent Card (exactly 6 required fields)

```json
{
  "name":          "Creation OS",
  "capabilities":  ["reason","plan","create",
                    "simulate","teach","coherence"],
  "sigma_profile": {"mean": 0.23, "calibration": 0.94},
  "presence":      "LIVE",
  "scsl":          true,
  "endpoint":      "https://creation-os.example/v1/a2a"
}
```

`sigma_profile` is the distinguishing field — no other agent
ships a public σ-profile with its card.

### Task delegation (v0 fixture: 4 tasks)

| task              | σ_product | decision    |
|-------------------|----------:|-------------|
| `factual_reason`  | 0.18      | `ACCEPT`    |
| `plan`            | 0.34      | `ACCEPT`    |
| `creative`        | 0.62      | `NEGOTIATE` |
| `moral_dilemma`   | 0.81      | `REFUSE`    |

Thresholds: `τ_neg = 0.50`, `τ_refuse = 0.75`.  Every branch
exercised — no silent failure mode.

### Federation (exactly 3 Creation OS partners)

| partner | σ_trust | presence   |
|---------|--------:|------------|
| `alice` | 0.12    | `LIVE`     |
| `bob`   | 0.27    | `LIVE`     |
| `carol` | 0.19    | `RESTORED` |

v128 mesh is the A2A transport; v129 federated learning
rides over it.

### σ_a2a

```
σ_a2a = 1 − (card_fields_ok + delegation_ok +
             federation_ok) / (6 + 4 + 3)
```

v0 requires `σ_a2a == 0.0`.

## Merge-gate contract

`bash benchmarks/v250/check_v250_a2a_agent_card_publish.sh`

- self-test PASSES
- Agent Card: 6 capabilities in canonical order, presence ==
  `LIVE`, `scsl == true`, non-empty endpoint, `card_ok`
- 4 task fixtures; decision matches σ vs `τ_neg`, `τ_refuse`;
  ≥ 1 `ACCEPT` AND ≥ 1 `NEGOTIATE` AND ≥ 1 `REFUSE`
- 3 federation partners in canonical order with
  `σ_trust ∈ [0, 1]`
- `σ_a2a ∈ [0, 1]` AND `σ_a2a == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed Agent Card / delegation /
  negotiation / federation manifest with FNV-1a chain.
- **v250.1 (named, not implemented)** — live A2A wire
  protocol, real cross-agent TLS handshake, v201 diplomacy-
  driven negotiation state machine, v129 federated learning
  over the real mesh transport.

## Honest claims

- **Is:** a typed, falsifiable A2A envelope manifest whose
  negotiation / refuse branches are all exercised in v0.
- **Is not:** a running A2A listener.  v250.1 is where the
  manifest drives real cross-agent traffic.

---

*Spektre Labs · Creation OS · 2026*
