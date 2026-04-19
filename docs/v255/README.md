# v255 — σ-Collaborate (`docs/v255/`)

Collaboration protocol.  Human + Creation OS as *partners*,
not assistant / user.  v255 types 5 modes, a σ-driven
role negotiation, a shared workspace with audit, and a
conflict resolver whose branches refuse both sycophancy
and stubbornness.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## σ-innovation

### Collaboration modes (exactly 5, canonical order)

| mode       | who leads                          | upstream   |
|------------|------------------------------------|------------|
| `ASSIST`   | human leads, model helps (default) | —          |
| `PAIR`     | alternate (pair programming)       | —          |
| `DELEGATE` | human delegates, model does        | v148       |
| `TEACH`    | model teaches                      | v252, v254 |
| `LEARN`    | model learns from human            | v124       |

### Role negotiation (4 fixtures, ≥ 3 distinct modes)

| scenario                  | σ_human | σ_model | chosen     |
|---------------------------|--------:|--------:|------------|
| `known_domain_help`       | 0.12    | 0.45    | `ASSIST`   |
| `offload_rebuild`         | 0.62    | 0.14    | `DELEGATE` |
| `pair_programming`        | 0.22    | 0.19    | `PAIR`     |
| `unknown_domain_learn`    | 0.78    | 0.12    | `TEACH`    |

### Shared workspace (3 audited edits)

| path                  | actor | tick | accepted |
|-----------------------|-------|-----:|:--------:|
| `docs/draft.md`       | HUMAN | 1    | true     |
| `src/module.c`        | MODEL | 2    | true     |
| `src/module_test.c`   | HUMAN | 3    | true     |

Ticks strictly ascending, both actors represented.  v181
audit is the lineage layer; v242 filesystem is the
backing store.

### Conflict resolution (2 fixtures, both branches fire)

Rule: `σ_disagreement ≤ τ_conflict=0.30` → `ASSERT`; else → `ADMIT`.

| topic                     | σ_disagreement | decision |
|---------------------------|---------------:|----------|
| `api_contract_semantics`  | 0.18           | `ASSERT` |
| `ambiguous_requirement`   | 0.58           | `ADMIT`  |

Anti-sycophancy and anti-stubbornness are both gated:
either branch missing ⇒ merge-gate fails.

### σ_collaborate

```
σ_collaborate = 1 − (modes_ok + negotiation_ok +
                     workspace_ok + conflict_ok) /
                    (5 + 4 + 3 + 2)
```

v0 requires `σ_collaborate == 0.0`.

## Merge-gate contract

`bash benchmarks/v255/check_v255_collaborate_role_negotiation.sh`

- self-test PASSES
- 5 modes in canonical order, every `mode_ok`
- 4 negotiation fixtures; every σ ∈ [0, 1]; ≥ 3 distinct
  modes chosen across fixtures
- 3 workspace edits with strictly ascending ticks, both
  actors present, every `accepted`
- 2 conflict fixtures, decision matches σ vs `τ_conflict`,
  ≥ 1 `ASSERT` AND ≥ 1 `ADMIT`
- `σ_collaborate ∈ [0, 1]` AND `σ_collaborate == 0.0`
- chain valid + byte-deterministic

## v0 vs v1 split

- **v0 (this tree)** — typed mode / negotiation /
  workspace / conflict manifest with FNV-1a chain.
- **v255.1 (named, not implemented)** — live mode
  switcher wired to v243 pipeline, live shared FS via
  v242 with per-edit σ, v181 audit streamed to the
  lineage store, v201 diplomacy-driven multi-round
  conflict resolution.

## Honest claims

- **Is:** a typed, falsifiable collaboration manifest
  where the σ-gate enforces both assertion *and*
  admission branches.
- **Is not:** a running collaboration surface.  v255.1 is
  where the manifest drives a live shared workspace.

---

*Spektre Labs · Creation OS · 2026*
