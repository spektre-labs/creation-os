# v58 — A2A Agent Card (σ extension) + σ-Swarm (HAP scaffold)

<!--
SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
Copyright (c) 2024-2026 Lauri Elias Rainio and Spektre Labs Oy.
-->

## Code

| File | Role |
|------|------|
| `python/cos/sigma_a2a_card.py` | `build_sigma_verifier_agent_card` — JSON-serialisable Agent Card with extended `sigma_profile` (signals, verdict types, `gate_version`, `kernel_size_bytes`). Optional `measured_auroc` / `measured_calibration_mean` only if you pass them (repro-backed). |
| `python/cos/sigma_swarm.py` | `SigmaSwarm` — gate each agent output before it enters the shared result map; σ-weighted consensus over accepted rows only. |
| `docs/v58/sigma_verifier_agent_card.example.json` | Example card (no headline benchmark numbers). |

## Protocol layering (orientation)

- **MCP (Anthropic):** agent ↔ tool. The σ-gate MCP server scores tool-side responses; see `docs/MCP_SIGMA.md`.
- **A2A (Google / LF):** agent ↔ agent. Extended Agent Cards advertise verifier capabilities; `cos-a2a` adds σ-trust at the peer level — see `docs/A2A_COS_TRUST.md`.

Both layers can consult the **same** Q16 verdict machine (`python/cos/sigma_gate_core.py` / `sigma_gate.h`).

## Hallucination Amplification Prevention (HAP)

**Hallucination amplification** means an incorrect claim is echoed across agents until informal “consensus” reinforces it. The swarm scaffold mitigates this **only at the policy hook you wire**: **ABSTAIN** rows are omitted from the map passed to consensus, and consensus weights accepted outputs by **low σ (Q16) → higher weight** — not a simple vote count.

This is a **lab scaffold**, not proof that a deployed fleet cannot amplify errors through channels you do not gate. Bind any AUROC, cost, or stop-rate claims to `docs/CLAIM_DISCIPLINE.md` and archived repro artifacts.

## Next (v59–v61 in the lab roadmap)

Combined MCP↔A2A gateway, cascade cost routing in A2A, then living-weights / TTT / self-modification modules — each as its own small change set with tests.
