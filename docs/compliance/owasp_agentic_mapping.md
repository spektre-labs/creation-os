# OWASP Agentic risks ↔ Creation OS (informative mapping)

**Purpose:** give security reviewers a **compliance-oriented** bridge from OWASP-style agentic risk themes to mechanisms shipped or demonstrated in this tree. This is **not** a third-party certification, penetration test report, or guarantee of coverage.

The OWASP naming for agentic AI evolves; rows below use the **ASI01–ASI10** style labels requested by integrators. Adjust identifiers to match your internal OWASP release when auditing.

| OWASP ASI | Risk | Creation OS mitigation (informative) |
|-----------|------|----------------------------------------|
| ASI01 | Goal hijacking | σ-gate and semantic checks surface off-topic or unstable completions; policy layers can tie goals to signed prompts. |
| ASI02 | Tool misuse | σ-gated tool path (`cos chat --tools`), risk tiers, and operator confirmation for high-risk commands. |
| ASI03 | Memory poisoning | Engram store with σ at write time; decay / consolidation policies reduce weight of unreliable memories. |
| ASI04 | Cascading failures | Pipeline `ABSTAIN` / `RETHINK` propagation; circuit-style refusal instead of unbounded retries. |
| ASI05 | Rogue agents | State ledger hooks and drift signals (for example σ drift → `RECALIBRATE` workflows in documented demos). |
| ASI06 | Identity abuse | Audit identifiers and hashed artifacts in serve / receipt paths (see proof receipt and audit row wiring in `cos serve`). |
| ASI07 | Prompt injection | High-entropy / inconsistent completions tend to raise σ; combine with external hardening (system prompt isolation, tool sandboxing). |
| ASI08 | Data exfiltration | Codex constitutional checks on emitted text; **not** a DLP replacement — pair with network egress controls. |
| ASI09 | Model inversion | Local inference path treats the model as a black box for σ measurement; no weight export from Creation OS. |
| ASI10 | Supply chain | SCSL / AGPL licensing in-tree; prefer vendored or pinned dependencies for your deployment bundle. |

**Limitations:** mapping rows are **architectural**, not empirical security scores. For claim discipline on measured numbers, see [CLAIM_DISCIPLINE.md](../CLAIM_DISCIPLINE.md).
