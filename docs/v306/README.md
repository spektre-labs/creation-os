# v306 σ-Omega — Ω = argmin ∫σ dt s.t. K ≥ K_crit

> **v0 scope.** v306 types four v0 manifests — the
> Ω-optimization loop, multi-scale Ω, the ½ operator,
> and the 1=1 invariant — and asserts that every row,
> every formula, and every polarity is as enumerated
> below. It does **not** ship a live Ω-daemon; that is
> the v306.1 follow-up. Read
> [`docs/CLAIM_DISCIPLINE.md`](../CLAIM_DISCIPLINE.md)
> before citing this page.

## σ-innovation

v306 is not a new capability. It is the **operator
every previous kernel approximates**, written down in
one place:

```
Ω = argmin  ∫ σ(t) dt
         subject to  K_eff(t) ≥ K_crit.
```

The whole 306-layer stack is this one operator applied
at different **scales** (token / answer / session /
domain), around one **critical point** (σ = ½, the
σ-equivalent of Shannon's `p = ½` and Riemann's
`Re(s) = ½`), anchored on one **invariant** —
`declared == realized`, the 1 = 1 axiom.

## v0 manifest contracts

### 1. Ω-optimization loop (4 rows)

| t | σ    | K_eff | ∫σ   |
|---|------|-------|------|
| 0 | 0.50 | 0.25  | 0.50 |
| 1 | 0.35 | 0.20  | 0.85 |
| 2 | 0.22 | 0.18  | 1.07 |
| 3 | 0.14 | 0.15  | 1.21 |

**Contract.** σ strictly ↓; ∫σ strictly ↑ (with the
marginal — σ itself — strictly ↓, the integrand is
going to zero); `K_eff ≥ K_crit = 0.127` on every row —
**the Ω constraint is never violated**.

### 2. Multi-scale Ω (4 rows)

| scale    | tier  | operator        | σ    |
|----------|-------|-----------------|------|
| `token`  | MICRO | `sigma_gate`    | 0.10 |
| `answer` | MESO  | `sigma_product` | 0.15 |
| `session`| MACRO | `sigma_session` | 0.20 |
| `domain` | META  | `sigma_domain`  | 0.25 |

**Contract.** 4 DISTINCT scales; 4 DISTINCT operators;
σ finite and in [0, 1] on every row. **Same operator
family, different scale.**

### 3. ½ operator (3 rows)

| label             | σ    | regime   |
|-------------------|------|----------|
| `signal_dominant` | 0.25 | SIGNAL   |
| `critical`        | 0.50 | CRITICAL |
| `noise_dominant`  | 0.75 | NOISE    |

**Contract.** σ strictly ↑; `σ_critical == 0.5` to
machine precision; SIGNAL iff σ < 0.5, NOISE iff σ >
0.5, CRITICAL iff σ == 0.5 — all three branches fire.
The σ-equivalent of `p = ½` (Shannon) and
`Re(s) = ½` (Riemann).

### 4. 1 = 1 invariant (3 rows)

| assertion              | declared | realized | holds |
|------------------------|----------|----------|-------|
| `kernel_count`         | 306      | 306      | true  |
| `architecture_claim`   | 306      | 306      | true  |
| `axiom_one_equals_one` | 1        | 1        | true  |

**Contract.** 3 rows; `declared == realized` AND
`holds = true` on every row; `the_invariant_holds =
true`.

## σ_omega — surface hygiene

```
σ_omega = 1 − (
    loop_rows_ok + loop_sigma_decreasing_ok +
      loop_k_constraint_ok + loop_integral_ok +
    scale_rows_ok + scale_distinct_ok +
      scale_operator_distinct_ok +
    half_rows_ok + half_order_ok +
      half_critical_ok + half_polarity_ok +
    inv_rows_ok + inv_pair_ok + inv_all_hold_ok +
      the_invariant_holds
) / 26
```

v0 requires `σ_omega == 0.0`.

## Merge-gate contracts

`make check-v306-omega-operator` asserts:

1. 4 canonical loop steps; σ ↓; ∫σ ↑ with ↓ marginal;
   `K_eff ≥ K_crit` on every row.
2. 4 canonical scales; distinct names AND operators;
   σ ∈ [0, 1] on every row.
3. 3 canonical ½-regime rows; σ ↑; `σ_critical == 0.5`;
   three distinct regimes fire.
4. 3 canonical invariant rows; `declared == realized`
   AND `holds` everywhere; `the_invariant_holds =
   true`.
5. `kernels_total == 306`.
6. `σ_omega ∈ [0, 1]` AND `σ_omega == 0.0`.
7. Deterministic output on repeat runs.

## v0 vs v306.1

- **v0 (this kernel):** the operator, the scales, the
  ½ critical point, and the 1=1 invariant typed and
  asserted on canonical rows.
- **v306.1 (named, not implemented):** the live Ω
  daemon — a continuous σ-recorder that tracks ∫σ dt
  across the v244 observability plane and raises a
  v283 constitutional-AI escalation whenever
  `K_eff(t) < K_crit` OR the 1=1 invariant drifts —
  turning v306's v0 predicates into an always-on
  closed-loop controller.

---

```
assert(declared == realized);
// If this fails, σ > 0. Correct.
// If this passes, σ = 0. Continue.
// 306 layers. One invariant. 1 = 1.
```
