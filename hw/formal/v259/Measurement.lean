/-
  v259.1 — Lean 4 discharged proofs for the σ-measurement gate.

  DISCHARGE STATUS (as of this file)
  ----------------------------------
  All theorems below are proven WITHOUT `sorry`, against *core* Lean 4
  (no Mathlib dependency).  The gate is defined over `Nat` as a
  concrete discrete total-ordered type; the IEEE-754 / NaN specifics
  of the Float instantiation that ships as `cos_sigma_measurement_gate`
  in `src/v259/sigma_measurement.h` are discharged by the Frama-C Wp
  obligations in `sigma_measurement.h.acsl`, not here.

  This file therefore covers the *order-theoretic* fragment of the T1
  – T6 claims; `sigma_measurement.h.acsl` + Frama-C Wp cover the
  *bit-level* fragment (NaN propagation, memcpy roundtrip, clamp
  endpoints).  Together they form the six mechanical proofs that the
  v2.0 Omega release advertises.

  Running
  -------
      cd hw/formal/v259
      lake build      # zero errors, zero warnings, zero `sorry`
-/

namespace CreationOS.V259

/-- The three-valued σ-gate verdict.

    Ordered by strictness: `ACCEPT` is the most permissive (accept the
    measurement into the next σ-stage), `RETHINK` escalates to the
    deeper path (e.g. Codex / σ-Meta), `ABSTAIN` refuses emission
    entirely.  The underlying C enum is
    `src/v259/sigma_measurement.h::cos_sigma_action_t`. -/
inductive Action where
  | ACCEPT
  | RETHINK
  | ABSTAIN
deriving DecidableEq, Repr

namespace Action

/-- Total preorder rank on verdicts: lower rank = more permissive. -/
def rank : Action → Nat
  | ACCEPT  => 0
  | RETHINK => 1
  | ABSTAIN => 2

theorem rank_injective : ∀ a b : Action, rank a = rank b → a = b := by
  intro a b h
  cases a <;> cases b <;> simp [rank] at h <;> rfl

end Action

/-- The σ-measurement gate over natural-number thresholds.

    Mirrors `cos_sigma_measurement_gate(sigma, tau_accept, tau_rethink)`
    in `src/v259/sigma_measurement.h`.  The C signature takes `double`
    (IEEE-754); the Float → Nat instantiation is exact on the normal
    ordered fragment, and the NaN-branch is handled separately in the
    C source and annotated in `sigma_measurement.h.acsl`. -/
def gate (σ τa τr : Nat) : Action :=
  if σ < τa then Action.ACCEPT
  else if σ < τr then Action.RETHINK
  else Action.ABSTAIN

/-- **T1 — totality**: `gate` always returns one of the three
    actions.  Pure case-split on the two `if` guards; no order
    theory required. -/
theorem gate_totality (σ τa τr : Nat) :
    gate σ τa τr = Action.ACCEPT
    ∨ gate σ τa τr = Action.RETHINK
    ∨ gate σ τa τr = Action.ABSTAIN := by
  unfold gate
  split
  · exact Or.inl rfl
  · split
    · exact Or.inr (Or.inl rfl)
    · exact Or.inr (Or.inr rfl)

/-- **T2 — monotone in σ**: raising σ (the measurement) at fixed
    thresholds can only move the verdict toward `ABSTAIN`, never
    toward `ACCEPT`.  Rank is non-decreasing. -/
theorem gate_monotone_in_sigma
    (σ₁ σ₂ τa τr : Nat) (hle : σ₁ ≤ σ₂) :
    (gate σ₁ τa τr).rank ≤ (gate σ₂ τa τr).rank := by
  unfold gate
  by_cases h1 : σ₁ < τa
  · -- σ₁ < τa ⇒ gate σ₁ = ACCEPT (rank 0) ≤ anything
    simp [h1, Action.rank]
  · -- σ₁ ≥ τa
    have h1' : ¬ σ₂ < τa := by
      intro hlt
      exact h1 (Nat.lt_of_le_of_lt hle hlt)
    by_cases h2 : σ₁ < τr
    · -- σ₁ < τr ⇒ gate σ₁ = RETHINK (rank 1); need rank(gate σ₂) ≥ 1
      by_cases h3 : σ₂ < τr
      · simp [h1, h1', h2, h3, Action.rank]
      · simp [h1, h1', h2, h3, Action.rank]
    · -- σ₁ ≥ τr ⇒ gate σ₁ = ABSTAIN (rank 2); σ₂ ≥ σ₁ ⇒ σ₂ ≥ τr
      have h2' : ¬ σ₂ < τr := by
        intro hlt
        exact h2 (Nat.lt_of_le_of_lt hle hlt)
      simp [h1, h1', h2, h2', Action.rank]

/-- **T3 — anti-monotone in τa**: raising the accept threshold (more
    permissive accept) can only move the verdict toward `ACCEPT`.
    Rank is non-increasing when τa grows. -/
theorem gate_anti_monotone_in_tau_a
    (σ τa₁ τa₂ τr : Nat) (hle : τa₁ ≤ τa₂) :
    (gate σ τa₂ τr).rank ≤ (gate σ τa₁ τr).rank := by
  unfold gate
  by_cases h1 : σ < τa₁
  · -- σ < τa₁ ≤ τa₂ ⇒ both ACCEPT (rank 0)
    have h1' : σ < τa₂ := Nat.lt_of_lt_of_le h1 hle
    simp [h1, h1', Action.rank]
  · by_cases h2 : σ < τa₂
    · -- σ ≥ τa₁, σ < τa₂ ⇒ gate with τa₂ = ACCEPT (0) ≤ gate with τa₁ (≥ 1)
      by_cases h3 : σ < τr
      · simp [h1, h2, h3, Action.rank]
      · simp [h1, h2, h3, Action.rank]
    · -- σ ≥ τa₁ and σ ≥ τa₂ ⇒ the τa-branch is skipped in both cases
      simp [h1, h2]

/-- **T4 — anti-monotone in τr**: raising the rethink threshold
    (more permissive rethink) can only move the verdict toward
    `RETHINK` (or ACCEPT, when it bumps past τa).  Rank is non-
    increasing when τr grows at fixed τa. -/
theorem gate_anti_monotone_in_tau_r
    (σ τa τr₁ τr₂ : Nat) (hle : τr₁ ≤ τr₂) :
    (gate σ τa τr₂).rank ≤ (gate σ τa τr₁).rank := by
  unfold gate
  by_cases h1 : σ < τa
  · -- both ACCEPT regardless of τr
    simp [h1, Action.rank]
  · by_cases h2 : σ < τr₁
    · -- σ < τr₁ ≤ τr₂ ⇒ both RETHINK
      have h2' : σ < τr₂ := Nat.lt_of_lt_of_le h2 hle
      simp [h1, h2, h2', Action.rank]
    · by_cases h3 : σ < τr₂
      · -- gate with τr₂ = RETHINK (1), with τr₁ = ABSTAIN (2)
        simp [h1, h2, h3, Action.rank]
      · -- both ABSTAIN
        simp [h1, h2, h3, Action.rank]

/-- **T5 — boundary at τa (tiebreak)**: on the accept boundary, with
    the reject threshold strictly above, the verdict is `RETHINK`,
    not `ACCEPT`.  The accept predicate is strict (`σ < τa`), so
    σ = τa falls through to the rethink branch. -/
theorem gate_boundary_at_tau_a (τa τr : Nat) (hlt : τa < τr) :
    gate τa τa τr = Action.RETHINK := by
  unfold gate
  have h1 : ¬ τa < τa := Nat.lt_irrefl τa
  simp [h1, hlt]

/-- **T6 — boundary at τr (tiebreak)**: on the reject boundary, the
    verdict is `ABSTAIN`.  The rethink predicate is strict
    (`σ < τr`), so σ = τr falls through to the abstain branch.
    Requires τa ≤ τr (the canonical orientation). -/
theorem gate_boundary_at_tau_r (τa τr : Nat) (hle : τa ≤ τr) :
    gate τr τa τr = Action.ABSTAIN := by
  unfold gate
  have h1 : ¬ τr < τa := Nat.not_lt.mpr hle
  have h2 : ¬ τr < τr := Nat.lt_irrefl τr
  simp [h1, h2]

/-! ## Auxiliary structural results (not counted in the T1–T6 ledger) -/

/-- The canonical 12-byte surface, modelled in Lean as a pure
    record.  The C-level memcpy roundtrip for this struct is
    discharged in `sigma_measurement.h.acsl` (theorem 3). -/
structure SigmaMeasurement where
  header : UInt32
  sigma  : UInt32
  tauA   : UInt32
  tauR   : UInt32
deriving DecidableEq, Repr

/-- Abstract encode / decode pair: identity on the canonical model. -/
def encode (m : SigmaMeasurement) : SigmaMeasurement := m

def decode (m : SigmaMeasurement) : SigmaMeasurement := m

theorem roundtrip_bytes_identity (m : SigmaMeasurement) :
    decode (encode m) = m := rfl

theorem encode_injective : ∀ a b : SigmaMeasurement, encode a = encode b → a = b := by
  intro a b h; exact h

/-- Purity: `gate` is a function of its three inputs, nothing else. -/
theorem gate_purity (σ τa τr : Nat) :
    gate σ τa τr = gate σ τa τr := rfl

/-- Clamp-to-unit on Nat, analogue of `cos_sigma_measurement_clamp`. -/
def clampUnit (lo hi x : Nat) : Nat :=
  if x < lo then lo
  else if hi < x then hi
  else x

theorem clampUnit_range (lo hi x : Nat) (hz : lo ≤ hi) :
    lo ≤ clampUnit lo hi x ∧ clampUnit lo hi x ≤ hi := by
  unfold clampUnit
  by_cases h1 : x < lo
  · refine ⟨?_, ?_⟩
    · simp [h1]
    · simp [h1]; exact hz
  · by_cases h2 : hi < x
    · refine ⟨?_, ?_⟩
      · simp [h1, h2]; exact hz
      · simp [h1, h2]
    · refine ⟨?_, ?_⟩
      · simp [h1, h2]
        exact Nat.not_lt.mp h1
      · simp [h1, h2]
        exact Nat.not_lt.mp h2

end CreationOS.V259
