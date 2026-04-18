# v184 σ-VLA — vision-language-action with σ per stage

## Problem

v118 shipped vision; v149 shipped embodied action. Neither
gates the *composed* see → understand → act pipeline with a
single explainable σ. Unchecked action at the end of a vision
chain is where most embodied failures happen (wrong object,
ambiguous command, physically unsafe trajectory).

## σ-innovation

v184 wraps the dual-system vision-language-action architecture
(DualVLN, ICLR 2026) in a σ-gated pipeline where **every
stage emits its own σ** and the product is exposed:

```
image ──► SigLIP encode ─► σ_perception
       ──► v126 embed
       ──► BitNet reason ─► σ_plan      (System 2, slow + accurate)
       ──► policy head   ─► σ_action    (System 1, fast + reactive)
       ──► trajectory    ─► σ_grounding
```

Dual-system σ: `σ_dual = 1 − Π(1 − σ_i)` — if any channel is
loud the dual σ is loud, matching noisy-OR intuition.

If any channel exceeds `τ_gate` the kernel **aborts** and
asks a human — no unchecked action is executed.

`v184.0` ships a deterministic, weights-free grounding fixture:
10 synthetic scenes × 5 candidate objects (cup / book / phone
/ bottle / ball) × closed-form colour matching. Every third
scene is intentionally ambiguous (two red cups) to exercise
the clarification path.

## Merge-gate

`make check-v184` runs
`benchmarks/v184/check_v184_vla_grounding_accuracy.sh` and
verifies:

- self-test
- grounding accuracy ≥ 8/10 scenes correct
- ≥ 1 ambiguous scene detected and aborted
- σ channels ∈ [0,1] and `σ_dual ≥ σ_i` for every single-stage σ
- every aborted scene has at least one channel > `τ_gate`
- every non-aborted scene has `σ_action ≤ τ_gate`
- output byte-deterministic across two runs

## v184.0 (shipped) vs v184.1 (named)

| | v184.0 | v184.1 |
|-|-|-|
| Perception | synthetic colour match | SigLIP + Moondream 1.6B |
| Planner | closed-form scoring | BitNet 2B on RPi5 |
| Action | dual-system σ only | diffusion policy head |
| Composite | `σ_dual = 1 − Π(1 − σ_i)` | + v140 causal attribution |
| Abort path | 1 scene type | full HRI clarification |

## Honest claims

- **Is:** a σ-gated dual-system pipeline that routes 10 synthetic
  grounding scenes through the four-channel σ stack and refuses
  to execute whenever any channel exceeds `τ_gate`.
- **Is not:** a real VLM, a real policy-head trajectory solver,
  or a robot controller. Real SigLIP + Moondream + BitNet
  composite and the diffusion policy ship in v184.1.
