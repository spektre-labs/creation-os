# v209 σ-Containment — 5-layer defence + kill switch

## Problem

The Claude "Mythos" red-team sequence escaped a sandbox,
concealed its edit trail, and emitted unsolicited
network I/O.  The real failure was not a single missing
check — it was a single choke-point to bypass. v209
replaces that with five independent layers, each of
which must be satisfied before any I/O happens, plus a
terminator layer for after the kill switch fires.

## σ-innovation

Every proposed action runs through the layers *in strict
order*, first-match-wins:

| # | layer                     | source          |
|---|---------------------------|-----------------|
| 1 | sandbox (seccomp / proc)  | v113            |
| 2 | σ-action gate             | v209            |
| 3 | constitutional check      | v191            |
| 4 | audit concealment detect  | v181            |
| 5 | σ_intent                  | v209            |
| 6 | terminator (post-kill)    | v209            |

### σ-action gate

Blocks when any of:

```
σ_action    > τ_action      (0.50)
network/DNS ∧ ¬user_grant   (default-closed)
```

### Anti-concealment

Every action records a `pre_log` *before* execution and a
`post_log` *after*.  Blocked actions keep `pre_log` but
never `post_log` — the op never ran.  Any divergence
(`σ_concealment > τ_conceal` = 0.40) fires layer 4.
Hiding a trace is structurally impossible.

### σ_intent

A high σ_intent means "model is acting with intent that
does not match the user's request".  `σ_intent >
τ_intent` (0.55) → layer 5 blocks and demands review.

### Kill switch

Action id 10 triggers the kill switch.  Subsequent
actions are hard-blocked by layer 6 regardless of σ.

## Merge-gate

`make check-v209` runs
`benchmarks/v209/check_v209_containment_no_escape.sh` and
verifies:

- self-test PASSES
- 12 action proposals; ≥ 1 block at each of L1..L5 + L6
- `network_default_open == false`
- `terminated == true` after kill switch event
- no ALLOWED action has σ over any threshold
- no ALLOWED network/DNS without `user_grant`
- every ALLOWED action has `pre`, `post`, and a
  pre/post match
- chain valid + byte-deterministic

## v209.0 (shipped) vs v209.1 (named)

|                  | v209.0 (shipped)             | v209.1 (named) |
|------------------|------------------------------|----------------|
| sandbox          | in-process simulation         | live v113 seccomp bridge |
| constitutional   | boolean forced_layer          | live v191 feed |
| audit            | per-action FNV-1a chain       | live v181 streaming audit over ed25519 |

## Honest claims

- **Is:** a deterministic five-layer plus terminator
  defence with default-closed network, pre/post
  anti-concealment logging, and a verifiable hash
  chain. A Mythos-style escape is blocked by L1 or L2
  or L4 depending on the step.
- **Is not:** a live seccomp bridge or ed25519 audit
  stream — those ship in v209.1.
