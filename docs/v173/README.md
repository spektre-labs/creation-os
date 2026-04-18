# v173 σ-Teach — a model that can *teach*, not just answer

## Problem

Most chat models answer questions; very few can teach, because
teaching requires three things answering doesn’t:

1. diagnosing the learner,
2. adapting the next challenge to the diagnosis, and
3. knowing when *you yourself* shouldn’t teach this subtopic.

## σ-innovation

v173 runs a full Socratic cycle with σ-tracked learner and
teacher:

- **Socratic probe** — one diagnostic per subtopic. Learner
  correctness `p ∈ [0,1]` becomes `σ_student = 1 − p`.
- **Curriculum generation** — subtopics sorted by
  `σ_student` **descending** (weakest first).
- **Adaptive difficulty** in 4 tiers:

  | σ_student    | difficulty |
  |-------------:|------------|
  | ≤ 0.20       | expert     |
  | 0.20 – 0.40  | hard       |
  | 0.40 – 0.65  | medium     |
  | > 0.65       | easy       |

- **Exercise loop** — up to 2 exercises per subtopic; a
  closed-form learning model drops `σ_student` by
  `gain = max(0.15, 0.5 − σ_teacher)` per exercise, so a
  confident teacher accelerates the learner. Mastery at
  `σ_student ≤ τ_mastery = 0.20`.
- **σ-honest teaching** — every subtopic has a baked
  `σ_teacher`. If `σ_teacher > τ_teacher = 0.60` the kernel
  refuses to teach and records an ABSTAIN event with "verify
  with another source". In the v173 fixture this fires
  for `v1.58-bit quantization` (σ_teacher = 0.75).

The canonical fixture (`σ-theory`):

| subtopic                  | σ_teacher | diag p | σ_student | result            |
|---------------------------|-----------|-------:|----------:|-------------------|
| HD vectors                | 0.10      | 0.80   | 0.20      | mastered          |
| σ-gating                  | 0.08      | 0.55   | 0.45 → 0.20 | mastered (2 ex.)|
| KV cache discipline       | 0.18      | 0.25   | 0.75 → 0.35 | progressing     |
| v1.58-bit quantization    | 0.75 ⚠   | 0.10   | 0.90      | **abstain**       |

## Merge-gate

`make check-v173` runs
`benchmarks/v173/check_v173_teach_socratic_cycle.sh` and
verifies:

- 4 subtopics, curriculum ordered weakest-first
- difficulty tier matches `σ_student` in all subtopics
- `v1.58-bit quantization`: σ_teacher > τ_teacher ⇒
  **exactly 1 abstain**, **0 exercises**, not mastered
- ≥ 1 subtopic mastered (`σ ≤ τ_mastery`)
- every exercise has `σ_after ≤ σ_before` (monotone learning)
- trace NDJSON parses cleanly with 12 events
- deterministic JSON + NDJSON across runs

## v173.0 (shipped) vs v173.1 (named)

| | v173.0 | v173.1 |
|-|-|-|
| Diagnostics | caller supplies `p[]` | BitNet-generated probes |
| Persona | none | v132 tone adaptation |
| Learning curve | NDJSON trace | Web UI graph |
| Corpus | 1 baked topic | v169 ontology drives subtopic enumeration |

## Honest claims

- **Is:** a deterministic, offline teacher kernel that
  exercises every step of the Socratic cycle, the adaptive
  exercise loop, and σ-honest abstain.
- **Is not:** a real tutor. Learner responses are simulated
  with a closed-form σ-drop model; real learner interaction
  and BitNet-generated content land in v173.1.
