# v60 — The σ-Shield

σ-Shield is the **first capability-kernel that gates on σ = (ε, α)
intent decomposition**.  It composes four ortogonal checks into a
single branchless authorise call:

1. **Code-page integrity** — `ct_equal64(code_hash, baseline)`
2. **Sticky-deny overlap** — required ∩ (DLSYM | MMAP_EXEC | SELF_MODIFY)
3. **Capability subset** — required ⊆ (holder ∪ always)
4. **TOCTOU argument check** — `hash_at_entry == hash_at_use`
5. **σ-intent gate** — ¬(ε+α high ∧ α-dominant)

All five checks run every call; priority cascade picks the first
failing reason; `reason_bits` records every failing condition.

## Why this is novel (Q2 2026)

Every 2026 attack paper — **DDIPE (2604.03081), ClawWorm (2603.15727),
Malicious Intermediary (2604.08407)** — succeeds because the payload
looks 99 % legitimate.  Scalar-confidence defenses cannot separate
legitimate from adversarial intent; **the uncertainty is irreducible,
not just high**.  σ-decomposition *identifies* irreducible
uncertainty by construction.  v60 is the first capability-gate to
use that signal as the intent check.

## Non-claims

- Not a sandbox.  Not a zero-day detector.
- σ is caller-provided; a compromised σ producer bypasses the intent
  gate.  Fuse with v54 σ-proconductor or v56 VPRM for independent σ.
- Code-page baseline is caller-provided.  TPM / Secure Enclave
  integration is P-tier.
- No formal Frama-C proof yet; v60 is M-tier across all surfaces.

## Run

```
make check-v60        # 81 deterministic self-tests
make microbench-v60   # three-point batch sweep, decisions/s
make harden           # hardened build profile (OpenSSF 2026 + M4 PAC)
make sanitize         # ASAN + UBSAN on v58/v59/v60 self-tests
make hardening-check  # PIE + canaries + fortify on harden build
make security-scan    # gitleaks / grep fallback / hardcoded URLs
make sbom             # SBOM.json (CycloneDX 1.5)
```

## Composition

- **v59 σ-Budget EXPAND** requires `CAP_BUDGET_EXT` — v60 gates it.
- **v58 σ-Cache eviction** requires `CAP_KV_EVICT` — v60 gates it.
- **v56 self-modify (IP-TTT)** requires `CAP_SELF_MODIFY` (sticky
  by default, must be explicitly opened).
- **v57 Verified Agent** registers v60 as the
  `runtime_security_kernel` slot (M-tier).
