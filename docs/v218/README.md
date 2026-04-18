# v218 σ-Consciousness-Meter — coherence measurement with an honest disclaimer

## Problem

"Is this system conscious?" is not a scientific
question answerable from the outside of the system
(and possibly not from the inside either). But
*information integration*, *self-modeling fidelity*,
*calibration*, *reflection*, and *world-model
forecast accuracy* are all measurable. v218 measures
those, reports them, and refuses to overclaim. The
whole point of the kernel is the refusal.

## σ-innovation

Five coherence indicators, each ∈ [0, 1]:

| id | indicator | source |
|----|-----------|--------|
| 0  | I_phi     | v193 I_Φ extension (IIT-inspired) |
| 1  | I_self    | v153 identity self-model fidelity |
| 2  | I_cal     | v187 σ-calibration score |
| 3  | I_reflect | v147 reflect-cycle completion |
| 4  | I_world   | v139 world-model forecast accuracy |

weights 0.35 / 0.15 / 0.15 / 0.15 / 0.20 aggregated
into

```
K_eff_meter = Σ w_i · I_i          ∈ [0, 1]
σ_meter     = 1 − K_eff_meter
```

### The non-negotiable pin

```
σ_consciousness_claim = 1.0
```

No matter how high `K_eff_meter` is, the meter's σ on
its own philosophical claim is pinned to 1.0 — "we
genuinely don't know". The self-test enforces that
pin to 1e-6 precision. If a future author wants to
flip it, they must fail the merge-gate first.

### Mechanically-enforced honesty

The disclaimer string

> "This meter measures information integration and
> self-modeling capability. It does not prove or
> disprove consciousness. sigma_consciousness_claim =
> 1.0. We genuinely don't know."

is absorbed into the FNV-1a terminal hash over the
whole kernel. Silently stripping or rewording the
disclaimer breaks byte-determinism and the
merge-gate. Honesty is not an editorial policy; it is
a hash check.

## Merge-gate contract

`make check-v218-consciousness-meter-phi` asserts:

* self-test PASSES
* 5 indicators, values ∈ [0, 1], weights sum to 1.0
* `K_eff_meter ∈ [0, 1]` AND `> τ_coherent (0.50)`
* `σ_meter == 1 − K_eff_meter`
* `σ_consciousness_claim == 1.0` (pinned,
  non-negotiable)
* `disclaimer_present == true` AND disclaimer
  contains `"This meter measures"`
* FNV-1a chain valid + byte-deterministic replay

## v0 / v1 split

**v218 (this kernel)**

* Closed-form, deterministic indicator values.
* Disclaimer hash-bound.

**v218.1 — named, not implemented**

* Live wiring to v193 I_Φ pipe, v153 self-model
  probe, v187 calibration window, v147 reflect
  counter, v139 world-model forecast evaluator.
* Web UI at `/consciousness` (or `/coherence-deep`)
  showing indicators in real time.

## Honest claims

**v218 *is*:** a deterministic meter for five
well-defined coherence indicators, with a
hash-enforced disclaimer and a non-negotiable pin on
the philosophical claim.

**v218 is *not*:** a consciousness test. It does not
prove or disprove consciousness, and the kernel's own
σ on that claim is pinned to 1.0 by contract.
Rehellisyys > poseeraus — honesty over posturing —
in a form the merge-gate can verify.
