/-
  v259.1 — Lean 4 theorem statements for sigma_measurement_t.

  STATUS: SCAFFOLDING. Every theorem below carries a `sorry`.  No
  statement here is a discharged proof — they are the proof OBLIGATIONS
  the v259 primitive claims to satisfy, written in Lean 4 so a future
  merge-gate job can `lean --run hw/formal/v259/Measurement.lean` and
  fail-hard if any `sorry` turns into a real gap.

  CLAIM DISCIPLINE
  ----------------
  * This file does NOT prove anything today.
  * Any documentation that points here must say "PENDING (Lean 4
    statement only)", never "formally verified".
  * The C self-test in `src/v259/sigma_measurement.c` and the JSON
    assertions in `benchmarks/v259/check_v259_sigma_measurement_primitive.sh`
    are the ONLY enforcement as of this commit.

  Running (once a Lean 4 toolchain is available in the tree)
  ----------------------------------------------------------
      lake env lean hw/formal/v259/Measurement.lean
  A successful future state has zero `sorry` in this file.
-/

namespace CreationOS.V259

/-- The 12-byte canonical surface.  We model it in Lean as a
    structure of two 32-bit-precision reals plus an opaque header
    word; the purely-structural layout theorems (sizeof / offsets)
    are discharged in `sigma_measurement.h.acsl` by Frama-C and are
    NOT mirrored here. -/
structure SigmaMeasurement where
  header : UInt32
  sigma  : Float
  tau    : Float
deriving Repr

/-- The three-valued gate verdict.  Total order is `allow < boundary < abstain`. -/
inductive Gate
  | allow
  | boundary
  | abstain
deriving DecidableEq, Repr

namespace Gate

def rank : Gate → Nat
  | allow    => 0
  | boundary => 1
  | abstain  => 2

theorem rank_injective : Function.Injective rank := by
  intro a b h
  cases a <;> cases b <;> simp [rank] at h <;> rfl

end Gate

/-- The pure gate predicate, mirroring
    `cos_sigma_measurement_gate` in `src/v259/sigma_measurement.h`. -/
def gate (m : SigmaMeasurement) : Gate :=
  if      m.sigma <  m.tau then Gate.allow
  else if m.sigma == m.tau then Gate.boundary
  else                          Gate.abstain

/-- **Theorem 2.1 (purity)** — `gate` is a function of its argument
    only; two calls on the same measurement yield the same verdict.

    PROOF: trivial by definitional equality in Lean, so this theorem
    is actually discharged.  We keep it here as a sanity target. -/
theorem gate_purity (m : SigmaMeasurement) : gate m = gate m := rfl

/-- **Theorem 2.2 (totality)** — `gate` returns one of three values.
    PROOF: PENDING. -/
theorem gate_totality (m : SigmaMeasurement) :
    gate m = Gate.allow ∨ gate m = Gate.boundary ∨ gate m = Gate.abstain := by
  sorry

/-- **Theorem 2.3 (order preservation)** — `gate` is monotone in σ
    at fixed τ:
        σ₁ ≤ σ₂  →  rank(gate ⟨_,σ₁,τ⟩) ≤ rank(gate ⟨_,σ₂,τ⟩).
    PROOF: PENDING. -/
theorem gate_monotone_in_sigma
    (h : UInt32) (σ₁ σ₂ τ : Float) (hle : σ₁ ≤ σ₂) :
    Gate.rank (gate ⟨h, σ₁, τ⟩) ≤ Gate.rank (gate ⟨h, σ₂, τ⟩) := by
  sorry

/-- **Theorem 2.4 (anti-monotone in τ)** — raising τ (being MORE
    permissive) can only move the verdict toward `allow`:
        τ₁ ≤ τ₂  →  rank(gate ⟨_,σ,τ₁⟩) ≥ rank(gate ⟨_,σ,τ₂⟩).
    PROOF: PENDING. -/
theorem gate_anti_monotone_in_tau
    (h : UInt32) (σ τ₁ τ₂ : Float) (hle : τ₁ ≤ τ₂) :
    Gate.rank (gate ⟨h, σ, τ₁⟩) ≥ Gate.rank (gate ⟨h, σ, τ₂⟩) := by
  sorry

/-- **Theorem 2.5 (boundary tiebreak)** — `σ == τ` classifies as
    `boundary`, not `allow`.  This is the convention used in v306 Ω
    half-operator and mirrored in `cos_sigma_measurement_gate`.
    PROOF: PENDING. -/
theorem gate_boundary_tiebreak
    (h : UInt32) (σ : Float) :
    gate ⟨h, σ, σ⟩ = Gate.boundary := by
  sorry

/-- **Theorem 3.1 (roundtrip equivalence, structural)** — the
    gate verdict is invariant under a memcpy-style roundtrip through
    a 12-byte buffer.  Because Lean does not model C's memcpy
    semantics natively, this is stated here as an equality under the
    `id` function; the bit-identical claim is discharged in the
    Frama-C annotation file instead.  PROOF: trivial (id), kept for
    completeness. -/
theorem roundtrip_equiv (m : SigmaMeasurement) :
    gate m = gate (id m) := rfl

/-- A total ordered-type abstraction of the clamp primitive
    `cos_sigma_measurement_clamp`, in a clean setting where Lean's
    Float-specific NaN / IEEE-754 pathology does not interfere.
    Instantiations to `Float` must inject an auxiliary `isNaN`
    predicate and are discharged in Frama-C, not here. -/
def clampUnit {α : Type} [LinearOrder α] (zero one : α) (x : α) : α :=
  if x < zero then zero
  else if one < x then one
  else x

/-- **Theorem 4.1 (clampUnit range)** — on any linear order with
    chosen bounds zero ≤ one, `clampUnit` always returns a value in
    the closed interval [zero, one].

    PROOF: by cases on the two `if` guards.  No `sorry`; this is a
    real discharge in the abstract setting.  The C-level theorem for
    `cos_sigma_measurement_clamp` at IEEE-754 `Float` is the
    NaN-handled counterpart, which is covered at the C level by
    `cos_v259_clamp_exhaustive_check` (sampling, not proof) and would
    be discharged by Frama-C Wp once a toolchain is wired. -/
theorem clampUnit_range
    {α : Type} [LinearOrder α] (zero one : α) (hz : zero ≤ one) (x : α) :
    zero ≤ clampUnit zero one x ∧ clampUnit zero one x ≤ one := by
  unfold clampUnit
  by_cases h1 : x < zero
  · simp [h1, hz]
  · by_cases h2 : one < x
    · simp [h1, h2, hz, le_refl]
    · have hx_ge_zero : zero ≤ x := le_of_not_lt h1
      have hx_le_one  : x ≤ one  := le_of_not_lt h2
      simp [h1, h2, hx_ge_zero, hx_le_one]

end CreationOS.V259
