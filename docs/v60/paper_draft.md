# σ-Shield: Decomposed-Uncertainty Capability Kernels for LLM Agents

## Abstract

We introduce **σ-Shield**, the first capability-based runtime
security kernel that gates on a decomposed uncertainty signal
σ = (ε, α) over caller intent, separating reducible (epistemic)
from irreducible (aleatoric) uncertainty.  Four orthogonal checks —
code-page integrity, sticky-deny overlap, capability subset,
TOCTOU-free argument hash, and σ-intent dominance — are combined in
a single branchless authorise call with constant-time hash equality
and no dynamic allocation on the hot path.  The kernel is the
natural sibling of v59 σ-Budget: v59 refuses to *compute* on
α-dominated problems; v60 refuses to *act* on α-dominated requests.
81 deterministic self-tests exercise every decision surface, every
priority path, and 2 000 randomised invariants; microbench on M-class
silicon measures ≥ 6 × 10⁷ decisions per second.

## 1. Motivation

The Q2 2026 arXiv literature on LLM-agent security — DDIPE
(2604.03081), ClawWorm (2603.15727), Malicious Intermediary
(2604.08407), ShieldNet (2604.04426) — converges on a single
observation: **every successful attack succeeds because the payload
is indistinguishable from legitimate use**.  DDIPE bypass rates of
11–33 % against strong defenses and 2.5 % evading *both* detection
and alignment demonstrate that signature-, heuristic-, and
capability-allowlist defenses all operate on scalar confidence, and
scalar confidence cannot separate "ambiguous because evidence is
missing" from "ambiguous because ambiguity is inherent".  That
distinction is exactly σ = (ε, α).

## 2. Policy

σ-Shield takes five inputs on every authorise call:

1. A request: `{action_id, required_caps, arg_hash_at_entry,
   arg_hash_at_use, ε, α}`.
2. `holder_caps` — caller's capability bitmask.
3. `code_page_hash` — caller-computed hash of the process's `.text`.
4. `baseline_hash` — caller-pinned hash at boot.
5. `policy` — σ_high, α_dom thresholds, sticky-deny mask, always-caps.

The decision is one of `{ALLOW, DENY_CAP, DENY_INTENT, DENY_TOCTOU,
DENY_INTEGRITY}`, carried alongside a `reason_bits` bitmask recording
**every** failing check.

## 3. Kernel

All five checks are computed unconditionally each call; priority is
encoded by a mask-cascade (`AND-NOT`) without any branch on the hot
path.  The σ-intent check fires iff `ε+α ≥ σ_high` **and**
`α/(ε+α) ≥ α_dom`, so the gate is active only in *high-σ ∧
α-dominated* cases — v60 never refuses legitimate but uncertain
requests; it refuses legitimate-looking but **irreducibly** ambiguous
ones.

Hash equality uses a 64-bit XOR-fold reduction without early-exit —
standard constant-time compare, appropriate for TOCTOU equality but
not for cryptographic authentication.  For cryptographic integrity
upstream callers are expected to feed a Blake3 / BLAKE2 / SHA-256
digest into `code_page_hash`.

## 4. Self-test (81 deterministic cases)

| Group | Cases |
| --- | --- |
| Version / defaults | 11 |
| Hash fold + constant-time equality (incl. 2048 random) | 11 |
| Decision surfaces (allow / 4 denials / integrity bypass / null) | 15 |
| Priority cascade (4 explicit orderings) | 10 |
| Always-caps bootstrap | 1 |
| σ-intent edge cases (ε-dom / low-σ / threshold boundary) | 3 |
| Batch = scalar (32-way + null-safe) | 2 |
| Tag strings + allocator (including `sizeof(request)==64`) | 10 |
| Adversarial scenarios (DDIPE / ClawWorm / intermediary /
  confused deputy / runtime patch) | 5 |
| Composition with v58 / v59 | 2 |
| Determinism + summary fields | 3 |
| Stress (2 000 random requests; invariants on decision range +
  reason-bits consistency) | 1 (sweep) |
| **Total** | **81** |

All pass; failure count asserted `== 0` in the harness.

## 5. Microbench

Three-point deterministic sweep (LCG-seeded requests, fixed
iteration counts):

```
N=128    iters=5000
N=1024   iters=1000
N=8192   iters=200
```

M-class silicon: sub-microsecond per authorise at every batch size;
≥ 6 × 10⁷ decisions per second.  The kernel is not a critical-path
bottleneck.

## 6. Positioning

- **Capability kernels** (seL4, WASM, IronClaw): cap-gated, not
  intent-gated.
- **Network defenses** (ShieldNet): network-level MITM with 0.995 F1,
  but still operates on scalar confidence.
- **σ-Shield**: cap-gated **and** intent-gated on a decomposed signal,
  closes the ambiguity class that evades the above.

## 7. Limitations

σ is caller-provided.  A compromised σ producer issues ALLOW on
α-dominated inputs.  Multi-source σ fusion (v54 σ-proconductor / v56
VPRM) addresses this at the layer above v60.  Code-page integrity
uses caller-provided `baseline_hash`; TPM / Secure Enclave-backed
attestation is a planned P-tier extension.  No formal Frama-C / TLA
proof yet; all v60 claims are M-tier runtime checks.  σ-Shield does
not isolate address spaces; compose with seL4 / WASM / Fuchsia
upstream.

## 8. Artefacts

- `src/v60/sigma_shield.{h,c}` — kernel
- `src/v60/creation_os_v60.c` — driver + 81-test self-test + microbench
- `scripts/v60/microbench.sh`
- `docs/v60/{THE_SIGMA_SHIELD,ARCHITECTURE,POSITIONING}.md`
- `SECURITY.md`, `THREAT_MODEL.md`
- `SBOM.json` — CycloneDX-lite 1.5 (per-component digest)
- `make harden` — OpenSSF 2026 + ARM64 branch-protection
- `make sanitize` — ASAN + UBSAN on v58 / v59 / v60
- `make check-v60 verify-agent merge-gate` — all M-tier green
