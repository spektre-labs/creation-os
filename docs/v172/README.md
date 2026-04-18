# v172 σ-Narrative — long-horizon context across sessions

## Problem

v115 holds *within-session* short-term memory. It cannot answer
"where did we leave off on Tuesday?" after a reboot; it knows
nothing about goals, milestones or the people you keep
mentioning.

## σ-innovation

v172 stitches sessions into a durable story, σ-scoring every
piece so low-confidence recall is flagged instead of smoothed
over.

1. **Session summary per session**, with `σ_coverage` ≤
   `τ_coverage = 0.30`. The gate means only "well-covered"
   summaries enter the thread; shaky ones are surfaced for a
   follow-up.
2. **Chained narrative thread** — `[0] week 1 → [1] week 2 →
   [2] week 3` concatenated, yielding a single coherent story
   that grows monotonically.
3. **Goal tracking** with `σ_progress` per goal. The
   **primary** goal is the open goal with the lowest σ (most
   trusted progress).
4. **Relationship memory** — `{name, role, last_session_id,
   last_context, σ_ident}` so `lauri` stays `owner` and
   `topias` stays `editor` across every resume.
5. **Resumption protocol** — a deterministic opener:
   > Last session: week 3 (σ=0.22). Last progress: week 3:
   > v139..v163 shipped; topias merged v154; v164..v168
   > staged. Active goal: Creation OS v1.0 release — done:
   > v84..v163 + v164..v168 staged; pending: v169..v173
   > (σ=0.17). Awaiting instructions.

No "Hi, how can I help?" — v172 cold-starts directly on the
right story beat.

## Merge-gate

`make check-v172` runs
`benchmarks/v172/check_v172_narrative_resumption.sh` and
verifies:

- 3 sessions × 4 facts, 3 goals, 4 people (fixture shape)
- every session has `σ_coverage ≤ τ_coverage`
- narrative thread contains every session label (week 1/2/3)
- resume line references the last session **and** the primary
  goal, and ends with "Awaiting instructions"
- people lookup: `lauri=owner`, `topias=editor` @ session 2
- primary goal is the open goal with minimum σ_progress
- determinism of JSON, thread and resume across runs

## v172.0 (shipped) vs v172.1 (named)

| | v172.0 | v172.1 |
|-|-|-|
| Sessions | baked 3-week fixture | real v115 memory iteration |
| Summaries | hand-written with computed σ | BitNet-generated |
| Thread growth | rebuild from scratch | incremental append, never rewrite |
| Goals | baked list | extracted from conversations |

## Honest claims

- **Is:** a deterministic, offline narrative engine that
  demonstrates σ-scored session summaries, chained threads,
  goal selection and a reproducible resume opener.
- **Is not:** a summariser. v0 uses hand-written summaries so
  the resumption logic can be audited in isolation; generation
  is v172.1.
