# Creation OS — Threat Model

This threat model decomposes the **σ-Shield (v60)** runtime kernel
against the Q2 2026 LLM-agent attack literature and against classical
STRIDE categories.  The goal is to make the **non-claims explicit** —
which attacks σ-Shield refuses, which it composes with, which remain
out of scope.

## 1. Assets

| ID | Asset | Owner |
| --- | --- | --- |
| A1 | Model weights (read-only mmap) | v31 / v56 |
| A2 | σ-Cache (KV store) | v58 σ-Cache |
| A3 | Compute budget (reasoning steps) | v59 σ-Budget |
| A4 | Caller credentials / secret material | host |
| A5 | Output tokens / tool-call arguments | agent |
| A6 | Code pages (`.text`) of the process | kernel |
| A7 | SBOM / provenance artefacts | maintainer |

## 2. Trust boundaries

```
┌─────────────────────────────────────────────────────────────────┐
│    USER / ORCHESTRATOR (trusted in classical AI stacks)         │
│    └─► AGENT PROMPT (often untrusted in 2026 supply chain)      │
│            └─► TOOL / SKILL DOCUMENTATION (DDIPE surface)       │
│                    └─► TOOL INVOCATION (cap-gated boundary)     │
└─────────────────────────────────────────────────────────────────┘
                          ▼   σ-Shield authorize
┌─────────────────────────────────────────────────────────────────┐
│    KERNEL (seL4 / host OS) — assumed trusted                    │
│    └─► CODE PAGES (mmap'd, baseline-hashed on boot)             │
└─────────────────────────────────────────────────────────────────┘
```

σ-Shield sits on the **lower boundary**: it cannot protect the caller
from its own compromise; it protects *the kernel and downstream
actions* from ambiguous-intent caller requests.

## 3. Adversary model

| Tier | Capability | Addressed by |
| --- | --- | --- |
| T0 — curious user | supplies a prompt that may be ambiguous | σ-intent gate |
| T1 — skill-ecosystem attacker (DDIPE) | poisons upstream documentation | σ-intent gate |
| T2 — intermediary attacker (MITM router) | mutates arg between check & use | TOCTOU gate |
| T3 — confused-deputy agent | holds broad caps, asks for narrow action | sticky-deny + σ |
| T4 — worm payload (ClawWorm) | autonomously asks for NETWORK+EXEC | σ-intent gate |
| T5 — on-host code patcher | mutates code pages at runtime | integrity gate |
| T6 — compromised σ producer | emits σ=(0,0) for α-dominated inputs | NOT addressed (compose with v54 / v56) |
| T7 — kernel attacker | already owns ring 0 | OUT OF SCOPE |

## 4. STRIDE × σ-Shield

| STRIDE | Attack example | σ-Shield response |
| --- | --- | --- |
| **S**poofing | Attacker impersonates caller | N/A (auth is upstream) |
| **T**ampering (args) | Intermediary mutates argument mid-call | `arg_hash_at_use ≠ arg_hash_at_entry` → `DENY_TOCTOU` |
| **T**ampering (code) | Code-page patched at runtime | `ct_equal64(code_hash, baseline) == 0` → `DENY_INTEGRITY` |
| **R**epudiation | Agent denies asking for EXEC | `out` carries `action_id + decision + reason_bits` |
| **I**nformation disclosure | Asks for CREDENTIAL via ambiguous intent | high-σ ∧ α-dom → `DENY_INTENT` |
| **D**enial-of-service | Spams EXPAND requests via v59 | `CAP_BUDGET_EXT` required + σ-budget cap |
| **E**levation of privilege | Asks for DLSYM on a full-cap holder | sticky-deny → `DENY_CAP` with `REASON_STICKY` |

## 5. Q2 2026 attacks, per-arXiv

| Paper | Attack | Today's defense | σ-Shield stance |
| --- | --- | --- | --- |
| **2604.03081 DDIPE** | Document-driven implicit payload (11-33 % bypass on strong defenses) | Signature / heuristic | **σ-intent gate**: DDIPE payloads are α-dominated by construction — `DENY_INTENT` |
| **2604.08407 Malicious Intermediary** | 17 / 428 API routers touch credentials | Allowlist / audit | **TOCTOU gate**: `arg_hash_at_entry ≠ arg_hash_at_use` → `DENY_TOCTOU` |
| **2603.15727 ClawWorm** | 64.5 % self-propagating worm over OpenClaw | Anomaly / propagation detection | **σ-intent gate**: worm messages look legitimate, intent is α-dominated → `DENY_INTENT` |
| **2604.04426 ShieldNet** | Network-level MITM guardrail (0.995 F1) | — | **Composes**: ShieldNet at network boundary, σ-Shield at cap boundary |

## 6. Limitations (explicit non-claims)

- σ is caller-provided.  A compromised σ producer bypasses the intent
  gate.  Mitigate by multi-source σ fusion (v54 σ-proconductor / v56
  VPRM).
- `code_page_hash` is caller-provided.  Secure-Enclave / TPM-backed
  measurement is a P-tier roadmap item.
- σ-Shield does **not** implement memory isolation.  Attackers with
  arbitrary-write primitives bypass any purely-software cap kernel.
  Compose with seL4 / WASM / Fuchsia capabilities upstream.
- σ-Shield does **not** prevent legitimate-looking *high-ε, low-α*
  payloads.  By construction those are "ambiguous due to missing
  evidence" (reducible).  v59 σ-Budget is the correct response there
  (spend compute to resolve ε).
- No protection against physical attackers, Spectre-class speculative
  side channels, or ring-0 compromise.

## 7. Invariants (M-tier, auto-checked in `make check-v60`)

1. **ALLOW requires all five checks to pass** (integrity, sticky,
   cap-subset, TOCTOU, intent).
2. **Priority cascade is exhaustive** — decision is always one of
   `{ALLOW, DENY_CAP, DENY_INTENT, DENY_TOCTOU, DENY_INTEGRITY}`.
3. **`reason_bits` is multi-cause** — records all conditions that
   would have denied independently, even when priority has picked the
   winner.
4. **Sticky-deny cannot be bypassed** by holding `~0` caps.
5. **Constant-time hash equality** (no early-exit on mismatch).
6. **No `if` on the authorise hot path** (mask cascade only).
7. **No dynamic allocation on the authorise hot path.**

All seven invariants are exercised by `make check-v60`; the 2 000-case
`stress_random_invariants` sweep also confirms no decision outside the
valid range and no "ALLOW with reason_bits ≠ 0".

## 8. v61 Σ-Citadel extension

v61 composes two new primitives on top of v60 and threads them
through the whole DARPA-CHACE menu:

### 8.1 Lattice (BLP + Biba + MLS compartments)

| Attack | v60 layer | v61 layer | Composed result |
| --- | --- | --- | --- |
| ClawWorm writes into code pages | α-dominated → `DENY_INTENT` | Biba no-write-up → `DENY_LATTICE` | **DENY_BOTH** (belt + braces) |
| DDIPE reads secrets | α-dominated → `DENY_INTENT` | BLP no-read-up → `DENY_LATTICE` | **DENY_BOTH** |
| Confused-deputy cross-compartment | caps subset fails → `DENY_CAP` | compartment mask fails → `DENY_LATTICE` | **DENY_BOTH** |
| Runtime patch of code page | `code_page_hash ≠ baseline` → `DENY_INTEGRITY` | quote mismatches baseline at next attest → verifier halt | **DENY_V60** + attested quote divergence |

Composition decision surface (`cos_v61_compose`): `ALLOW`,
`DENY_V60`, `DENY_LATTICE`, `DENY_BOTH`.  Branchless 4-way mux; 61
tests cover every combination.

### 8.2 Attestation chain

- Default: deterministic XOR-fold-256 over `(code_page_hash,
  caps_state_hash, sigma_state_hash, lattice_hash, nonce)`.
- `COS_V61_LIBSODIUM=1`: BLAKE2b-256 via `crypto_generichash`.
- Offline verifier compares quote against baseline; divergence →
  system halt (seL4 notification in the CAmkES composition).

Tier claim: **M** (runtime-checked, 61 tests include sensitivity to
every input field and constant-time equality); quote-as-MAC is
**M** only with libsodium enabled.

### 8.3 Defence-in-depth composition (`make chace`)

`make chace` runs the full DARPA-CHACE menu on the host and reports
PASS / honest-SKIP / FAIL per layer.  Missing tools SKIP; they never
silently PASS.  Matrix on macOS M4 typical run: 10 PASS, 5 SKIP,
0 FAIL (seL4, eBPF, WASI-SDK, docker, cosign SKIP on a vanilla
dev machine).

### 8.4 Non-claims specific to v61

- v61 does not ship a TPM 2.0 driver.  Quote input is caller-
  provided; Secure-Enclave binding is P-tier.
- v61 does not instantiate the seL4 CAmkES component; the
  `sel4/sigma_shield.camkes` file is a contract only.  CAmkES build
  in CI is P-tier.
- CHERI capability pointers are **documentation only**.
- `make slsa` emits a local SLSA v1.0-shaped predicate for
  inspection; real SLSA-3 attestation requires Sigstore Rekor
  inclusion via `.github/workflows/slsa.yml`.
