# v132 — σ-Persona

**Adaptive user profile: expertise tracker + style feedback + TOML persistence.**

## Problem

Every user is different.  A model that answers an expert mathematician the same way it answers a child is wrong in both directions.  Creation OS needs a per-user state that *learns* from σ observations and explicit corrections — without fine-tuning weights on every interaction.

## σ-innovation

Three primitives, all local, all deterministic:

### 1. Expertise staircase from σ

Running EMA of σ_product per topic (α = 0.30) with a neutral 0.50 prior.  Four-level ladder:

| EMA σ range | Level |
|---|---|
| < 0.15 | expert |
| 0.15 – 0.35 | advanced |
| 0.35 – 0.60 | intermediate |
| ≥ 0.60 | beginner |

If you ask the model about math and σ stays low, it answers you at expert depth.  If σ stays high on biology, it switches to plain language.

### 2. Correction-feedback style state

Six directional nudges, one step per feedback:

- `too-long` / `too-short` → `style.length` (terse/concise/normal/verbose)
- `too-technical` / `too-simple` → `style.detail` (simple/neutral/technical)
- `too-direct` / `too-formal` → `style.tone` (direct/neutral/formal)

State clamps at rail ends — no runaway from repeated feedback.  This is state, not fine-tuning; the LoRA side of the loop is v124/v125.

### 3. TOML profile at `~/.creation-os/persona.toml`

Canonical, idempotent layout — humans can read and hand-edit.  Example:

```toml
[persona]
language = "fi"
adaptations = 142

[style]
length = "concise"
tone   = "direct"
detail = "technical"

[expertise]
math    = { level = "expert",   ema_sigma = 0.12, samples = 48 }
biology = { level = "beginner", ema_sigma = 0.78, samples = 12 }
```

Minimal parser (no external TOML library) round-trips every field losslessly (asserted in `check-v132`).

## Locality

v132 state is **per-node**.  It never federates through v129 — personas stay local by construction.  This is a GDPR-friendly design choice: your style profile is a privacy-sensitive signal, not a shared weight.

## Contract

```c
cos_v132_persona_init   (&p)
cos_v132_observe        (&p, topic, σ)                  → new EMA σ
cos_v132_apply_feedback (&p, COS_V132_FEEDBACK_...)     → 0/1 applied
cos_v132_write_toml     (&p, fp)
cos_v132_read_toml      (&p, fp)
cos_v132_to_json        (&p, out, cap)                  → bytes written
cos_v132_level_from_sigma(ema_σ)                        → level enum
```

## What's measured by `check-v132`

- 10 low-σ math observations → level = expert (or advanced under EMA prior).
- 10 high-σ biology observations → level = beginner.
- `too-long` starting at `normal` → `concise`.
- `too-direct` starting at `neutral` → `formal` (step toward less direct).
- TOML round-trip: every topic, level, EMA, sample count preserved.
- `--demo` emits a JSON + canonical TOML fixture.

## What ships now vs v132.1

| v132.0 (this commit) | v132.1 (follow-up) |
|---|---|
| EMA expertise staircase + feedback state + TOML round-trip + JSON | Wire into v106 prompt builder: inject style directives per turn |
| Deterministic self-test + merge-gate | Multi-user switching via v128 mesh (node == user) |
| Local persistence only | v115 memory: rank memories by persona expertise level |
