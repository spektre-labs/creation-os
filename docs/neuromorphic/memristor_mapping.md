# Memristor mapping — BitNet b1.58 ternary weights ↔ crossbar conductance (Creation OS v39)

This note is **I-tier positioning** plus a **T-tier** engineering sketch: it is not an in-repo measured memristor characterization.

## Why this matters for σ

Commercial and research neuromorphic stacks overwhelmingly implement **vector–matrix multiply (VMM)** in analog crossbars: a dense matmul stage without an explicit “should I abstain?” output layer. Creation OS treats **σ** as the missing *decision hygiene* layer across substrates: the same *algebraic role* (down-weight incoherent continuation via \(K_{\mathrm{eff}}=(1-\sigma)\cdot K\)) can be instantiated in software, FPGA/ASIC gates, and—here—as **hardware uncertainty** from analog compute limits.

## BitNet {-1, 0, +1} ↔ memristor states (sketch)

| Weight | Memristor story (cartoon) | Resistance / conductance intuition |
|---:|---|---|
| **+1** | Strong **LRS** (“on”) in the forward polarity | ~**10 kΩ** order-of-magnitude planning anchor (not measured here) |
| **0** | **HRS / effectively off** (line disconnected for that product) | **> 1 MΩ** “open-ish” planning anchor |
| **−1** | **LRS** with **complementary polarity / complementary cell** in a signed accumulation scheme | same order as +1, but subtracts in the column summation story |

This is the sense in which **ternary weights are a natural language for memristive arrays**: the discrete {-1,0,+1} alphabet maps to *three meaningful physical regimes* (two active polarities + off), rather than forcing a post-hoc “fake ternary” from FP32.

## Crossbar GEMM (analog) vs XNOR-popcount (digital)

- **Analog crossbar VMM:** column currents sum Kirchhoff contributions from many row drivers interacting with stored conductances—**one-shot matmul energy** at the physical layer (idealized story).
- **Digital XNOR+popcount:** exact discrete similarity / binding energy in Boolean silicon.

They answer different engineering questions. The analog path introduces **device physics noise** as a first-class contributor to uncertainty.

## σ_hardware — “the analog remainder”

Analog crossbars accumulate **non-ideality**:

- conductance drift / aging,
- sneak paths in passive arrays,
- read noise (thermal + shot),
- write variability (cycle-to-cycle and device-to-device).

In Creation OS v39, these are grouped conceptually as **σ_hardware**: a substrate-local uncertainty term that is **not** the same object as token-level softmax entropy, but is intended to sit in the **same σ accounting** as aleatoric/epistemic software decomposition.

The repo ships a **digital toy model** in `hdl/neuromorphic/crossbar_sim.sv` (Verilator smoke) plus a **scalar estimator** in `src/sigma/decompose_v39.c`. Neither is a measured memristor array.

## Roadmap honesty (P-tier contacts)

Potential collaboration *vectors* (not initiated from this repo alone) are listed under **Projections** in `docs/SIGMA_FULL_STACK.md`. Treat them as **P** until there is a signed collaboration + data archive.
