<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
-->

# Creation OS — Compliance mapping (organizational)

**Purpose:** map major governance frameworks to **in-repo components** for architects and integrators.

**Not legal advice.** This file does **not** assert EU AI Act, NIST, or OWASP **certification**. Creation OS is an open-source research and integration stack; your deployment needs its own legal, security, and risk review.

**Claim discipline:** do not treat prose here as a substitute for measured artifacts. Read [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) before citing accuracy, AUROC, or benchmark numbers — never merge harness metrics with microbench or lab-demo claims in one headline sentence.

---

## EU AI Act (high-level themes)

| Theme | In-repo hooks (illustrative) |
|--------|------------------------------|
| Risk management | σ-gate and layered safety modules (continuous **lab** scoring, not a substitute for your risk register) |
| Data governance | Local-first defaults; no mandatory cloud in core paths; see privacy / offline docs in tree |
| Transparency | Evidence ladder in [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md); open source licenses (SCSL-1.0 / AGPL-3.0) |
| Human oversight | Tier 3 tool policy in [python/cos/tool_safety.py](../python/cos/tool_safety.py) (`HUMAN_REQUIRED`); constitution probe in [python/cos/zkp.py](../python/cos/zkp.py) |
| Logging and traceability | [python/cos/observe.py](../python/cos/observe.py), audit structures in tool safety and σ receipts (lab) |
| Technical documentation | [README.md](../README.md), [docs/DOC_INDEX.md](DOC_INDEX.md), architecture notes |
| Accuracy and performance | **Scoped** statements only: separate **harness** vs **lab demo** evidence per [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md) §1 and §6 — no uncited leaderboard shorthand in compliance tables |

---

## OWASP LLM Top 10 (2025) — mitigation mapping (illustrative)

| # | Risk | In-repo mitigations (pointers) |
|---|------|--------------------------------|
| 1 | Prompt injection | [python/cos/prompt_guard.py](../python/cos/prompt_guard.py), σ-aware scanning (lab) |
| 2 | Insecure output handling | σ-gate on model / tool outputs; Fabric trace hooks |
| 3 | Training data poisoning | Federated σ-filter **lab** ([python/cos/federated.py](../python/cos/federated.py), [python/cos/sigma_federated.py](../python/cos/sigma_federated.py)) — not a proof against adaptive poisoning |
| 4 | Model denial of service | Deployment-specific; server and rate limits are operator concerns |
| 5 | Supply chain | SPDX headers in sources; dependency policy in packaging docs |
| 6 | Sensitive information disclosure | Tool tier policy, prompt guard; operator PII policies required |
| 7 | Insecure plugin design | MCP / tool paths σ-gated in integration layers ([python/cos/mcp.py](../python/cos/mcp.py), fabric) |
| 8 | Excessive agency | Tool tiers, approval paths, planner modules (lab) |
| 9 | Overreliance | Evidence ladder; **NOT AGI ACHIEVED** statements in repo policy |
| 10 | Model theft | License (SCSL-1.0 / AGPL-3.0); integrity hooks are **lab** (e.g. hash commitments in [python/cos/zkp.py](../python/cos/zkp.py)) — not DLP |

---

## NIST AI RMF — function mapping (illustrative)

| Function | In-repo orientation |
|----------|---------------------|
| **GOVERN** | Constitution / policy texts ([python/cos/zkp.py](../python/cos/zkp.py)); licenses; [AGENTS.md](../AGENTS.md) |
| **MAP** | [python/cos/fabric.py](../python/cos/fabric.py) `layer_status`, self-model and graph modules where loaded |
| **MEASURE** | σ-gate probes, eval harnesses (where run), [python/cos/observe.py](../python/cos/observe.py) — each claim needs an evidence class |
| **MANAGE** | Drift, red team, formal hooks, tool safety tiers — all **integration / lab** unless you archive production evidence |

---

*Spektre Labs · Creation OS · 2026*
