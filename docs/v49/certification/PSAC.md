# Plan for Software Aspects of Certification (PSAC) — σ-critical path (v49 lab)

## Purpose

Define the **intended assurance activities** for the small, in-repo σ-critical path:

`logits → (entropy / margin / Dirichlet decomposition) → calibrated epistemic gate → {EMIT, ABSTAIN}`

## Scope (explicit)

**In scope (this repo):**

- `src/v47/sigma_kernel_verified.c` (+ header)
- `src/v49/sigma_gate.c` (+ header)
- Formal replay harness: `hdl/v47/sigma_pipeline_extended.sby` (RTL datapath; separate artifact)
- Tooling scripts under `scripts/v49/`

**Out of scope (not claimed as certified here):**

- Full `creation_os_proxy`, Python stacks, MLX, model weights, cloud deployments
- Any regulatory approval or Design Approval

## Assurance strategy

1. **Requirements** (HLR/LLR) with bidirectional trace hooks (`trace_manifest.json`)
2. **Verification**: unit/property tests + optional Frama-C/WP + optional SymbiYosys
3. **Structural coverage**: instrumented builds (`scripts/v49/run_mcdc.sh`)
4. **Binary hygiene**: best-effort checks (`scripts/v49/binary_audit.sh`)
5. **Adversarial posture**: `make red-team` (v48 harness; SKIPs are explicit)

## Non-claim

This PSAC is a **template aligned to DO-178C language** for engineering discipline. It is **not** a submitted PSAC to a certification authority.
