# Creation OS — Security Policy

Creation OS is an **offline, zero-telemetry, formally-verified-where-possible**
AGI kernel.  Security is not a layer; it is the architecture.  This document
states the reporting process, the supported surface, the guarantees we
actually make (and the ones we do not), and the active hardening controls.

## Supported versions

The merge-gate guarantees runtime-checked correctness (M-tier) on the
main branch, head-of-tree.  Older tags are **not** maintained for
security fixes; if you depend on an older tag, pin the commit, own the
patch.  The σ-labs (v56, v57, v58, v59, v60) have no network code path
by construction.

| Version | Status | Merge-gate | Security lab |
| --- | --- | --- | --- |
| `main` | supported | yes | σ-Shield (v60) M-tier |
| tagged v59 and earlier | reproducible only | yes at tag time | — |

## Reporting a vulnerability

Write the repository owner; do not open a public issue.  Include:

1. A minimal reproducer (as a `make`-runnable target if possible).
2. Commit hash you reproduced on.
3. Your threat-model assumption (what capability the attacker has).
4. Impact (denial-of-service? capability escape? intent-gate bypass?).

We respond within 72 h with an acknowledgement and an advisory tier.
No bug-bounty program, no coordinated-disclosure SLA beyond that
72 h acknowledgement — we are a small kernel team.

## What is guaranteed (and what is not)

### Tier semantics (v57 dialect)

| Tier | Meaning |
| --- | --- |
| **F** — Formally proven | Frama-C / TLA+ / Coq artefact; machine-checked |
| **M** — Runtime-checked | Deterministic self-test in `make check-*` |
| **I** — Interpreted | Documented invariant, not auto-checked |
| **P** — Planned | Roadmap item; no claim today |

### Active guarantees (M-tier minimum)

- **v60 σ-Shield** (`make check-v60`, 81 tests, every decision surface).
  Branchless capability authorise + σ-gated intent + TOCTOU-free args +
  code-page integrity.  Priority cascade is exhaustive and `reason_bits`
  is multi-cause honest.
- **v47 Frama-C architecture** (F-tier, where active).
- **v48 red-team harness** (M-tier; 0/342 bypasses at last run).
- **v49 DO-178C DAL-A artefacts** (I-tier; generated on demand).
- **Hardened build profile** (`make harden`): OpenSSF 2026 hardening
  flags + `-mbranch-protection=standard` + PIE.
- **Sanitizer matrix** (`make sanitize`): ASAN on v58 / v59 / v60 +
  UBSAN on v60, all passing their own self-tests under sanitizer.
- **Layered secret scan** (`make security-scan`): gitleaks when
  installed, grep-only fallback always; allowlist in `.gitleaks.toml`.
- **SBOM** (`make sbom` → `SBOM.json`): CycloneDX-lite 1.5 per
  component (`src/v*/`), SHA-256 pinned.

### Explicit non-guarantees

- **Not a sandbox in itself.**  v60 σ-Shield is a capability *gate*.
  The process still runs in its host address space; for sandbox-grade
  isolation compose with seL4 / WASM / Fuchsia upstream.
- **Not a zero-day detector.**  σ-Shield gates on **caller-provided
  σ, caller-provided required-caps, caller-provided arg-hash**.  A
  compromised σ producer can still issue an ALLOW; that is why v56
  VPRM and v54 σ-proconductor exist — producing σ with independent
  evidence lines.
- **No TPM / Secure Enclave** integration yet (P-tier).  Code-page
  baseline hashes are caller-provided.
- **No formal proof of σ-Shield** yet (F-tier).  Frama-C annotations
  are a planned follow-up.
- **No network code path** on any σ-lab (v56-v60).  If a future lab
  needs network, it will ship with ShieldNet-style MITM + σ gating as
  a separate module.

## Threat model

See [`THREAT_MODEL.md`](THREAT_MODEL.md) for the STRIDE decomposition
against Q2 2026 attack classes (DDIPE, ClawWorm, Malicious
Intermediary).  Summary: v60 refuses α-dominated intent *regardless*
of capability, closing the class of "looks-legitimate" attacks that
beat signature defenses.

## Local-dev hardening (developer quick-start)

```
make harden            # OpenSSF 2026 flags + ARM64 branch-protection
make sanitize          # ASAN + UBSAN matrix runs v58/v59/v60 self-tests
make hardening-check   # verify PIE / canaries / fortify on harden build
make security-scan     # gitleaks + grep-fallback + hardcoded URL check
make sbom              # emit SBOM.json (CycloneDX 1.5)
make reproducible-build  # double-build digest compare
```

Pre-commit hooks:

```
pip install pre-commit
pre-commit install
pre-commit run --all-files
```

The hook runs `scripts/security/scan.sh`, a minimal whitespace / EOL
sweep, and a reject-`.env` guard.

## Supply-chain posture

- **AGPL-3.0-or-later** on every source file (SPDX headers).
- **No runtime dependencies** on any v56-v60 lab (pure C11 + libc).
- **SBOM** ships per-component in `SBOM.json` (CycloneDX 1.5).
- **Reproducible builds**: `make reproducible-build` double-builds and
  compares SHA-256.  SLSA L3 provenance (sigstore + Rekor + isolated
  CI build) is on the P-tier roadmap.
- **.gitleaks.toml** allowlists only explicit docs / fixtures; any
  accidental secret blocks the pre-commit hook.
