# Software Design Description (SDD) — σ-critical path (v49 lab)

## Components

### `src/v47/sigma_kernel_verified.c`

Implements stable-softmax Shannon entropy, top-1/top-2 margin (clamped), and Dirichlet-evidence style decomposition consistent with `src/sigma/decompose.c` (float path).

### `src/v49/sigma_gate.c`

Implements a **fail-closed** scalar gate:

- Non-finite inputs ⇒ `ABSTAIN`
- `epistemic > threshold` ⇒ `ABSTAIN`
- Else ⇒ `EMIT`

### `tests/v49/mcdc_tests.c`

Deterministic unit tests used as a **coverage driver** for gcov/lcov.

## Data contracts

- Inputs are **logits** (pre-softmax), not probabilities, for the v47 kernel surface (see v47 header comments).
