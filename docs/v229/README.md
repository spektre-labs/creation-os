# v229 — σ-Seed (`docs/v229/`)

Minimal seed architecture — the whole 228-kernel tree compresses to a
five-kernel quintet that re-grows the rest under a σ-gated protocol.

> Canonical Creation OS remote:
> [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os) —
> see [../CANONICAL_GIT_REPOSITORY.md](../CANONICAL_GIT_REPOSITORY.md).

## The problem

Creation OS is 228 kernels.  If you lost everything except five files,
which five would let you regrow the system?  v229 names the seed and
specifies the growth protocol, then proves (at v0) that the regrowth is
deterministic.

## σ-innovation

### The five-kernel seed

| seed kernel | role                         |
|-------------|------------------------------|
| v029        | σ-aggregate (measurement)    |
| v101        | bridge (inference)           |
| v106        | server (API)                 |
| v124        | continual (learning)         |
| v148        | sovereign (RSI loop)         |

Nothing else is primitive.  Every other kernel in the stack re-derives
from these five through a σ-gated growth protocol.

### Growth protocol (v0)

For each candidate in a fixed queue:

1. v029 measures the current system (σ).
2. v133 meta-detects a missing capability (bootstrapped).
3. v146 genesis proposes a candidate kernel.
4. v143 bench / σ-scoring gives σ_raw (risk of this candidate).
5. v162 compose integrates iff σ_growth ≤ τ_growth (0.40).

σ_growth is a closed-form combination:

```
σ_depth  = depth_from_seed · 0.03
σ_growth = 0.60 · σ_raw + 0.40 · σ_depth
accept  iff  σ_growth ≤ τ_growth
             AND parent already accepted
```

Depth penalty says "kernels far from the seed are riskier" — a v0
surrogate for 'distance from first principles'.  v229.1 will drive
σ_raw / σ_depth from live v146 genesis + v143 bench data.

### Deterministic regrowth (1 = 1)

The self-test runs the full growth protocol twice from a fresh state
and asserts `terminal_hash == terminal_hash_verify`.  This is the
offline stand-in for the README's "SHA-256 over the grown system" —
v229 uses the stack-canonical FNV-1a chain so the check runs inside the
C harness with no external hash tool.

## Merge-gate contract

`bash benchmarks/v229/check_v229_seed_regrow_deterministic.sh`

- self-test PASSES
- seed ids equal `{29, 101, 106, 124, 148}`
- `n_accepted ≥ 15` (5 seed + ≥ 10 grown)
- `n_rejected ≥ 1` (σ-gate has teeth)
- every accepted kernel has `σ_growth ≤ τ_growth`
- every accepted non-seed kernel has its parent already accepted
- `terminal_hash == terminal_hash_verify` (repeat-run equality)
- byte-deterministic CLI

## v0 vs v1 split

- **v0 (this tree)** — 13-candidate deterministic queue with fixture
  σ_raw values, closed-form σ_growth, topological parent ordering,
  FNV-1a regrowth proof.
- **v229.1 (named, not implemented)** — live v146 genesis hookup,
  SHA-256 over a real filesystem tree, a `cos seed --verify` CLI that
  end-to-end rebuilds from the seed, and a continuous v229 → v148 RSI
  loop that can also *shrink* the grown set.

## Honest claims

- **Is:** a precise statement that five kernels are enough to regrow
  the stack under a σ-gate, with a byte-identical regrowth proof on a
  fixture.
- **Is not:** a claim that the fixture σ values match the true σ of the
  real v133/v143/v146/... kernels; those get measured in v229.1.

---

*Spektre Labs · Creation OS · 2026*
