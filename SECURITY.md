# Security policy — Creation OS (`creation-os`)

## Scope

This repository ships **C reference code**, a **Makefile**, and **documentation**. Threat models include:

- Malicious inputs to future networked wrappers (not shipped as mandatory here).
- Supply-chain tampering with binaries built from this source.
- **Misrepresentation** of benchmark or safety properties (epistemic / reputational risk — mitigated by [docs/CLAIM_DISCIPLINE.md](docs/CLAIM_DISCIPLINE.md)).

## Supported versions

| Line | Support |
|------|---------|
| Current `main` / default branch | Active maintenance |
| Older tags | Best-effort; prefer upgrading |

## Reporting a vulnerability

1. **GitHub:** Use **Security advisories** (private disclosure) for this repository if the feature is enabled.
2. **Otherwise:** Open a **confidential** issue only if your org policy allows; do not post exploit details publicly until coordinated disclosure.

Include: affected commit or version, build flags, minimal reproducer, and impact assessment.

## Out of scope

- Theoretical disagreements with BSC / σ design (not a security defect).
- Performance of toy demos vs frontier LMs (see README *Limitations*).

---

*Spektre Labs · Creation OS · 2026*
