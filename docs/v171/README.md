# v171 σ-Collab — explicit human↔AI collaboration protocol

## Problem

Most LLM products ship one of two postures: **autonomous** (the
model just does things) or **obedient** (the model sycophantically
agrees). Neither is honest about *who is contributing what with
what confidence*, and neither has a principled hand-off when the
model is out of its depth.

## σ-innovation

v171 formalises the collaboration and makes it machine-legible:

- **Explicit mode** — `[collaboration] mode = "pair" | "lead" | "follow"`.
  Each mode has its own handoff threshold (τ_pair = 0.60,
  τ_lead = 0.75, τ_follow = 0.40).
- **Four actions per turn**, resolved in strict priority order:
  1. **anti_sycophancy** — human issued a shaky claim (σ_human
     high), the model *would* semantically agree, yet its own
     σ_model ≥ τ_sycophancy. The kernel flags the glossy yes
     and forces "I appear to agree but my σ says I’m uncertain."
  2. **debate** — human claim ≠ model view and
     `|σ_model − σ_human| > τ_disagree`. Both stances get
     published with their σ.
  3. **handoff** — σ_model > τ for the current mode.
  4. **emit** — default.
- **Contribution audit** — one NDJSON line per turn recording
  `{human_input, model_contribution, σ_model, σ_human,
    sycophancy_suspected, human_validated}` so after the fact
  anyone can ask *who decided what with how much confidence*.

A baked 6-turn scenario exercises every action at least once
under `mode=pair`.

## Merge-gate

`make check-v171` runs
`benchmarks/v171/check_v171_collab_handoff.sh` and verifies:

- self-test
- 6-turn scripted scenario in pair mode contains `emit`,
  `handoff`, `debate` **and** `anti_sycophancy`
- `anti_sycophancy` turn has `sycophancy_suspected = true`
- `debate` turn has `σ_disagreement > τ_disagree = 0.25`
- mode-dependent handoff: `follow` produces ≥ 1 handoff and
  `handoffs_follow ≥ handoffs_lead`
- NDJSON audit has exactly n_turns lines of valid JSON
- JSON and NDJSON are byte-identical across runs (determinism)

## v171.0 (shipped) vs v171.1 (named)

| | v171.0 | v171.1 |
|-|-|-|
| Mode source | CLI flag | `config.toml [collaboration]` block |
| Sycophancy detector | deterministic "agrees + σ ≥ τ" gate | v125 DPO trained against sycophancy |
| Debate | σ_Δ threshold | v150 full debate protocol with reviewer |
| Audit sink | stdout NDJSON | v167 audit log + v115 memory |

## Honest claims

- **Is:** a deterministic collaboration-protocol kernel that
  exercises all four action paths and emits a complete
  contribution audit trail under three modes.
- **Is not:** a UX. v0 emits the records; real editors/chat
  clients consume them in v171.1.
