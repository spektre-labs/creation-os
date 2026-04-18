# v131 — σ-Temporal

**Time-aware reasoning: timeline, σ-trend, decay, spikes, deadline-σ.**

## Problem

Until v131 Creation OS treats every prompt independently.  "What did we talk about last week?" had no answer; "is the model learning?" couldn't be measured over time; old memories couldn't age gracefully.

## σ-innovation

Five primitives:

### 1. Session timeline
Every interaction carries a `ts_unix` and a `topic`.  Window retrieval `recall_window(since, until, topic?)` → array of events.

### 2. σ-trend per topic (OLS slope)
Linear regression of σ_product vs time.  **Negative slope = learning working** (v124 continual is doing its job).  Positive slope → v133 meta-benchmark's regression alarm fires.  Reported per second *and* per day for human readability.

### 3. σ-weighted exponential decay
```
w(age, σ) = exp(-age / half_life · (1 + α · σ))
```
Confident-old memories survive; uncertain-old memories vanish.

At `half_life = 7d`, `age = 30d`:
- σ = 0.10 → weight ≈ 0.009
- σ = 0.90 → weight ≈ 2.9 × 10⁻⁴

→ **uncertain 30-day-old memories decay 30× faster** than confident ones.

### 4. Spike detection
Flags indices where σ jumps by ≥ threshold between consecutive same-topic events.  Feeds v10 causal-surgery with "what changed at step N?"

### 5. Deadline-σ prediction
```
σ̂ = clamp01(baseline + slope · time_fraction_used)
```
v121 planning consumes this: tight deadline → higher expected σ → more replan headroom reserved.

## Contract

```c
cos_v131_timeline_init/append/free(&t, ...)
cos_v131_recall_window     (&t, since, until, topic?, out, max)   → count
cos_v131_sigma_trend       (&t, topic, since, until, &n_used)     → σ/sec
cos_v131_decay_weight      (age_sec, σ, half_life_sec, α)         → weight
cos_v131_detect_spikes     (&t, topic?, threshold, out_idx, max)
cos_v131_predict_sigma_under_pressure(&ds, time_fraction_used)
```

## What's measured by `check-v131`

- Window recall: 8 events in first week, 7 math events across 14 days.
- Math (learning) slope is **negative**; biology (regression) slope is **positive**.
- Decay: low-σ old weight > high-σ old weight (confirms σ-weighting).
- High-σ 30-day weight < 0.01.
- Spike detection catches a deliberate σ jump from ~0.05 to 0.90.
- Deadline σ rises monotonically with time-fraction-used.

## What ships now vs v131.1

| v131.0 (this commit) | v131.1 (follow-up) |
|---|---|
| In-memory timeline (caller manages lifecycle) | SQLite binding to v115 memory for persistent timelines |
| OLS slope + σ-weighted decay + spikes + deadline-σ | Wire spike detector into v10 causal-surgery prompt |
| Deterministic synthetic data in merge-gate | Replay real v124 / v125 session logs to measure slope |
