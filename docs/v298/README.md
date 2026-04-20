# v298 — σ-Rosetta

**Universal translatability.**

The Rosetta Stone carries the same text in three scripts. 2000
years later, hieroglyphics could be read again. Code that explains
itself — mathematically, in multiple languages, and in a
human-readable log — survives the same way. v298 types four v0
manifests: every σ measurement carries a reason, the spec exists
in three languages, logs are both binary and text, and the core
definition is a timeless formula.

## σ innovation (what σ adds here)

> **No σ without a reason.** v298 forbids emitting a bare number
> on the wire — every σ carries its domain and an explanation
> long enough to be read by the next developer, whichever
> decade they arrive in.

## v0 manifests

Enumerated in [`src/v298/rosetta.h`](../../src/v298/rosetta.h);
pinned by
[`benchmarks/v298/check_v298_rosetta_self_documenting.sh`](../../benchmarks/v298/check_v298_rosetta_self_documenting.sh).

### 1. Self-documenting σ (exactly 3 canonical emissions)

| σ    | domain  | reason (excerpt)                                      |
|------|---------|-------------------------------------------------------|
| 0.35 | LLM     | entropy high, top_token_prob 0.23, 4 competing tokens |
| 0.15 | SENSOR  | noise floor 0.02, signal 0.13, SNR low                |
| 0.60 | ORG     | disagreement between 3 voters, quorum not met         |

3 DISTINCT domains in order; every row `reason_present == true`
AND `reason_length ≥ reason_min_length = 20`.

### 2. Multi-language spec (exactly 3 canonical bindings)

| language | role      | maintained | semantic_match_to_c |
|----------|-----------|------------|---------------------|
| `C`      | REFERENCE | `true`     | `true`              |
| `Python` | ADOPTION  | `true`     | `true`              |
| `Rust`   | SAFETY    | `true`     | `true`              |

3 DISTINCT roles; if one language dies, two remain; the C
reference binds them.

### 3. Human-readable log format (exactly 3 canonical formats)

| format   | machine_readable | human_readable_forever |
|----------|------------------|------------------------|
| `binary` | `true`           | `false`                |
| `csv`    | `true`           | `true`                 |
| `json`   | `true`           | `true`                 |

All machine-readable; exactly 2 human-readable-forever (CSV and
JSON); exactly 1 not (binary).

### 4. Mathematical specification (exactly 3 canonical invariants)

| name                    | formula                    | formal_expression_present | ages_out |
|-------------------------|----------------------------|---------------------------|----------|
| `sigma_definition`      | `sigma = noise/(signal+noise)` | `true`                | `false`  |
| `pythagoras_2500_yr`    | `a^2 + b^2 = c^2`          | `true`                    | `false`  |
| `arithmetic_invariant`  | `(a + b) + c = a + (b + c)` | `true`                   | `false`  |

Every row has a formal expression; none age out — mathematics
does not expire; σ lives on the same shelf as Pythagoras.

### σ_rosetta (surface hygiene)

```
σ_rosetta = 1 −
  (emit_rows_ok + emit_domain_distinct_ok + emit_reason_ok +
   spec_rows_ok + spec_role_distinct_ok + spec_maintained_ok +
   spec_match_ok +
   fmt_rows_ok + fmt_machine_ok + fmt_human_count_ok +
   math_rows_ok + math_expression_ok + math_ages_out_ok) /
  (3 + 1 + 1 + 3 + 1 + 1 + 1 + 3 + 1 + 1 + 3 + 1 + 1)
```

v0 requires `σ_rosetta == 0.0`.

## Merge-gate contracts

- 3 σ emissions; 3 distinct domains; every emission carries a
  non-empty reason of at least 20 characters.
- 3 language bindings; 3 distinct roles; all maintained;
  all semantic-match to C.
- 3 log formats; all machine-readable; exactly 2
  human-readable-forever; exactly 1 not.
- 3 mathematical invariants; all have a formal expression;
  none age out.
- `σ_rosetta ∈ [0, 1] AND == 0.0`.
- FNV-1a chain replays byte-identically.

## v0 vs vN.1 (claim discipline)

- **v0 (this kernel)** names the emission / binding / format /
  math contracts as predicates.
- **v298.1 (named, not implemented)** is live σ-log
  double-writes (binary + CSV) wired to the v268
  continuous-batch emitter, a `cos --explain` flag on the proxy
  that returns the reason string verbatim, and CI jobs that
  diff the Python / Rust bindings against the C reference on
  every merge.

See [docs/CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
