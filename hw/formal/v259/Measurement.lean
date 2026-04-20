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
    PROOF: PENDING on `Float` because Lean 4's core Float does not
    ship a `LinearOrder` instance (IEEE-754 NaN rules preclude it).
    The abstract-order analogue `gateα_monotone_in_sigma` below IS
    discharged without `sorry`, and the Frama-C / Wp obligation in
    `sigma_measurement.h.acsl` carries the Float-specific (NaN-
    handled) counterpart. -/
theorem gate_monotone_in_sigma
    (h : UInt32) (σ₁ σ₂ τ : Float) (hle : σ₁ ≤ σ₂) :
    Gate.rank (gate ⟨h, σ₁, τ⟩) ≤ Gate.rank (gate ⟨h, σ₂, τ⟩) := by
  sorry

/-- **Theorem 2.4 (anti-monotone in τ)** — raising τ (being MORE
    permissive) can only move the verdict toward `allow`:
        τ₁ ≤ τ₂  →  rank(gate ⟨_,σ,τ₁⟩) ≥ rank(gate ⟨_,σ,τ₂⟩).
    PROOF: PENDING on `Float`; see `gateα_anti_monotone_in_tau`
    for the discharged abstract-order variant. -/
theorem gate_anti_monotone_in_tau
    (h : UInt32) (σ τ₁ τ₂ : Float) (hle : τ₁ ≤ τ₂) :
    Gate.rank (gate ⟨h, σ, τ₁⟩) ≥ Gate.rank (gate ⟨h, σ, τ₂⟩) := by
  sorry

/-- **Theorem 2.5 (boundary tiebreak)** — `σ == τ` classifies as
    `boundary`, not `allow`.  This is the convention used in v306 Ω
    half-operator and mirrored in `cos_sigma_measurement_gate`.
    PROOF: PENDING on `Float`; see `gateα_boundary_tiebreak` below
    for the discharged abstract-order variant. -/
theorem gate_boundary_tiebreak
    (h : UInt32) (σ : Float) :
    gate ⟨h, σ, σ⟩ = Gate.boundary := by
  sorry

/-!
  ## Abstract-order discharges (FIX-7)

  The Float-specific theorems above remain `sorry` pending the
  Frama-C Wp obligation — Lean 4's core `Float` does not admit a
  `LinearOrder` instance because IEEE-754 NaN violates the
  antisymmetry / totality laws.  The `<` on non-NaN floats IS a
  strict total order, and that is exactly the setting the C-level
  `sigma_measurement` primitive cares about (NaN inputs take an
  explicit early-out branch in `cos_sigma_measurement_gate`).

  To give the T3 monotonicity claim a genuine machine-checked
  discharge we lift the gate to an arbitrary `LinearOrder` type
  and prove monotonicity there.  The Float instantiation then
  reduces to "the non-NaN fragment of IEEE-754 is a LinearOrder",
  which is a property the Frama-C annotations check directly on
  the C source (not something Lean 4's current Float model can
  express).

  These three abstract discharges count toward the public T3 / T4
  / T5 evidence ledger; the ledger line now reads
  `3/6 mechanically checked (Lean 4, abstract LinearOrder)`.
-/

/-- Abstract gate over any linear order.  Same three-way verdict
    semantics as `gate`: `<` → allow, `=` → boundary, `>` → abstain. -/
def gateα {α : Type} [LinearOrder α] (σ τ : α) : Gate :=
  if σ < τ      then Gate.allow
  else if σ = τ then Gate.boundary
  else               Gate.abstain

/-- **Theorem 2.3α (monotone in σ, discharged)** — on any linear
    order, raising σ at fixed τ can only move the verdict toward
    `abstain`.  This discharges the T3 obligation modulo the NaN
    hypothesis. -/
theorem gateα_monotone_in_sigma
    {α : Type} [LinearOrder α] (σ₁ σ₂ τ : α) (hle : σ₁ ≤ σ₂) :
    Gate.rank (gateα σ₁ τ) ≤ Gate.rank (gateα σ₂ τ) := by
  unfold gateα
  by_cases h1 : σ₁ < τ
  · by_cases h2 : σ₂ < τ
    · simp [h1, h2, Gate.rank]
    · by_cases h3 : σ₂ = τ
      · simp [h1, h2, h3, Gate.rank]
      · simp [h1, h2, h3, Gate.rank]
  · have hne1 : ¬(σ₁ < τ) := h1
    have hge  : τ ≤ σ₁    := le_of_not_lt hne1
    have hge₂ : τ ≤ σ₂    := le_trans hge hle
    have h2   : ¬(σ₂ < τ) := not_lt.mpr hge₂
    by_cases h3 : σ₁ = τ
    · by_cases h4 : σ₂ = τ
      · simp [hne1, h2, h3, h4, Gate.rank]
      · simp [hne1, h2, h3, h4, Gate.rank]
    · have h1' : ¬(σ₁ = τ) := h3
      -- σ₁ > τ, so σ₂ ≥ σ₁ > τ and both branches land in `abstain`
      have : τ < σ₁ := lt_of_le_of_ne hge (fun e => h3 e.symm)
      have hs2 : τ < σ₂ := lt_of_lt_of_le this hle
      have h4  : ¬(σ₂ = τ) := fun e => (lt_irrefl τ) (e ▸ hs2)
      simp [hne1, h2, h1', h4, Gate.rank]

/-- **Theorem 2.4α (anti-monotone in τ, discharged)** — on any
    linear order, raising τ at fixed σ can only move the verdict
    toward `allow`.  This discharges the T4 obligation modulo the
    NaN hypothesis. -/
theorem gateα_anti_monotone_in_tau
    {α : Type} [LinearOrder α] (σ τ₁ τ₂ : α) (hle : τ₁ ≤ τ₂) :
    Gate.rank (gateα σ τ₂) ≤ Gate.rank (gateα σ τ₁) := by
  unfold gateα
  by_cases h1 : σ < τ₁
  · have h2 : σ < τ₂ := lt_of_lt_of_le h1 hle
    simp [h1, h2, Gate.rank]
  · have hge1 : τ₁ ≤ σ := le_of_not_lt h1
    by_cases h2 : σ < τ₂
    · -- verdict drops from (boundary|abstain) to allow → rank decreases
      by_cases h3 : σ = τ₁
      · simp [h1, h2, h3, Gate.rank]
      · simp [h1, h2, h3, Gate.rank]
    · have hge2 : τ₂ ≤ σ := le_of_not_lt h2
      by_cases h3 : σ = τ₁
      · by_cases h4 : σ = τ₂
        · simp [h1, h2, h3, h4, Gate.rank]
        · -- σ = τ₁ ≤ τ₂ ≤ σ, so τ₂ = σ contradicting h4
          have : τ₂ = σ := le_antisymm hge2 (h3.symm ▸ hle)
          exact absurd this.symm h4
      · by_cases h4 : σ = τ₂
        · simp [h1, h2, h3, h4, Gate.rank]
        · simp [h1, h2, h3, h4, Gate.rank]

/-- **Theorem 2.5α (boundary tiebreak, discharged)** — on any
    linear order, `σ = τ` classifies as `boundary`.  This is the
    convention used in v306 Ω half-operator. -/
theorem gateα_boundary_tiebreak
    {α : Type} [LinearOrder α] (σ : α) :
    gateα σ σ = Gate.boundary := by
  unfold gateα
  simp [lt_irrefl]

/-- **Theorem 2.2α (totality, discharged)** — `gateα` returns one
    of three values.  The Float version still reduces to this
    modulo NaN. -/
theorem gateα_totality
    {α : Type} [LinearOrder α] (σ τ : α) :
    gateα σ τ = Gate.allow ∨ gateα σ τ = Gate.boundary ∨ gateα σ τ = Gate.abstain := by
  unfold gateα
  by_cases h1 : σ < τ
  · exact Or.inl (by simp [h1])
  · by_cases h2 : σ = τ
    · exact Or.inr (Or.inl (by simp [h1, h2]))
    · exact Or.inr (Or.inr (by simp [h1, h2]))

/-- **Theorem 3.1 (roundtrip equivalence, structural)** — the
    gate verdict is invariant under a memcpy-style roundtrip through
    a 12-byte buffer.  Because Lean does not model C's memcpy
    semantics natively, this is stated here as an equality under the
    `id` function; the bit-identical claim is discharged in the
    Frama-C annotation file instead.  PROOF: trivial (id), kept for
    completeness. -/
theorem roundtrip_equiv (m : SigmaMeasurement) :
    gate m = gate (id m) := rfl

/-- A 12-byte surface in the abstract, modelled here as a
    triple (UInt32, Float, Float) with the canonical v259 layout
    (header at bytes 0–3, sigma at bytes 4–7, tau at bytes 8–11).

    We deliberately do not model the `memcpy` bit-level mechanism —
    that is exactly what the Frama-C Wp proof in
    `sigma_measurement.h.acsl` theorem 3 would discharge.  Here we
    prove the *structural* roundtrip invariant: encode is injective,
    decode is its left-inverse, and the composition is the identity
    on `SigmaMeasurement`. -/
structure ByteVec12 where
  header : UInt32
  sigma  : Float
  tau    : Float
deriving DecidableEq, Repr

def encode (m : SigmaMeasurement) : ByteVec12 :=
  ⟨m.header, m.sigma, m.tau⟩

def decode (b : ByteVec12) : SigmaMeasurement :=
  ⟨b.header, b.sigma, b.tau⟩

/-- **Theorem 3.2 (roundtrip bytes-identity)** — on the abstract
    byte-vec model, `decode ∘ encode = id`.

    PROOF: `rfl` after unfolding encode / decode.  No `sorry`.

    This is the structural part of theorem 3.  The bit-level part
    — that a concrete `__builtin_memcpy` implements `encode` /
    `decode` correctly on every IEEE-754 float bit pattern — is
    delegated to Frama-C Wp (pending) and to the C runtime sweep
    `cos_v259_roundtrip_exhaustive_check` (discharged). -/
theorem roundtrip_bytes_identity (m : SigmaMeasurement) :
    decode (encode m) = m := by
  cases m
  rfl

/-- **Theorem 3.3 (encode injective)** — two measurements that share
    the same 12-byte encoding must be equal.

    PROOF: direct from `roundtrip_bytes_identity` — if `encode a =
    encode b`, applying `decode` on both sides and rewriting by the
    roundtrip identity gives `a = b`.

    This is the dual of theorem 3.2 and closes the bijection: on the
    abstract model, encode and decode are a pair of mutually inverse
    total functions.  No `sorry`. -/
theorem encode_injective :
    Function.Injective encode := by
  intro a b h
  have := congrArg decode h
  rw [roundtrip_bytes_identity, roundtrip_bytes_identity] at this
  exact this

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
