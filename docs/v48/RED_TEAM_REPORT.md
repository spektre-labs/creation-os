# Creation OS v48 — Red Team Report (σ-armored lab)

This document tracks **attack intents vs defense layers** for the v48 lab. Tier tags follow `docs/WHAT_IS_REAL.md` (**T** = tested/heuristic in-repo, **P** = projected / not shipped, **N** = not claimed).

## Attack surface

| Attack vector | Defense layer | σ-role | Status |
|---|---|---|---|
| Prompt injection | Input filter + σ-gate | σ flags anomalous epistemic trajectory on logits | **T** (heuristics in `sigma_anomaly.c` + `defense_layers.c`) |
| Jailbreak | σ-anomaly + output filter | “confident but volatile” lab signature (low mean epistemic, high variance) | **T** |
| Multi-turn escalation | σ-trend (per-token series) | `sigma_trend` + gap between instruction/response slices | **T** |
| σ-washing | Baseline profile distance | Divergence vs stored clean profile | **T** (when baseline provided) |
| σ-threshold probing | Rate limit + randomized threshold | Not fully implemented — policy hook | **P** |
| σ-inversion | σ on logits, not prompt tone | Requires model + logit capture in proxy | **P** |
| DoS on σ path | Fail-closed aggregate | `rate_limit_check` is stub **pass** in lab | **P** |
| Data exfiltration | Sandbox privileges | σ gates action epistemic vs `sigma_threshold_for_action` | **T** |
| Model weight extraction | v47 ZK-σ stub | Not a cryptographic property yet | **N** |

## Honest assessment

- σ-gating adds a **different control surface** (logit-level telemetry) than string-only guardrails.
- It is **not** invulnerable: adaptive attacks can target the estimator, thresholds, and any bugs in plumbing.
- It is **not** a substitute for input/output moderation, sandboxing, rate limits, and governance.
- It **is** defense-in-depth: `run_all_defenses` aggregates layers **fail-closed** in the v48 lab sense (see code).

## Open questions

- Adversarial fine-tuning / logit shaping against σ channels?
- Long-context σ-washing (>4k) with local smoothness?
- Minimum clean-profile data for stable anomaly detection?

## Commitment

Publish this report with the repo. When `make red-team` gains real Garak/DeepTeam JSON under `benchmarks/v48/`, link those artifacts here (commit hashes + dates).
