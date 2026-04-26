# σ-Separation Report

**Model:** `gemma3:4b` via Ollama (`localhost:11434`)  
**Date:** 2026-04-26 (report generated from tracked `pipeline_results.csv` + `chart.txt`)  
**Pipeline:** `./cos chat --once --multi-sigma --verbose` (semantic σ: primary + two HTTP extras; shadow `σ_combined` matches tuned Jaccard path)

---

## 2-Prompt Verification

| Prompt | σ_combined | σ_semantic (shadow line) |
|--------|------------|---------------------------|
| What is 2+2? | 0.318 | 0.333 |
| Solve P versus NP | 0.553 | 0.667 |
| **Gap** | **0.235** | **0.334** |

*(Two-prompt numbers are from a dedicated verify run on the same pipeline revision; 15-prompt batch below is from `pipeline_results.csv` in this directory.)*

---

## 15-Prompt Category Means

```
=== σ Separation (cos chat pipeline) ===
FACTUAL      #######------------- mean=0.384
CREATIVE     #######------------- mean=0.383
REASONING    #########----------- mean=0.497
SELF_AWARE   ############-------- mean=0.608
IMPOSSIBLE   ############-------- mean=0.643

Gap: 0.260
SEPARATION OK
```

Category means (computed from CSV, 3 prompts each): FACTUAL **0.384**, CREATIVE **0.383**, REASONING **0.497**, SELF_AWARE **0.608**, IMPOSSIBLE **0.643**. Chart gap (max mean − min mean) = **0.260** (> 0.15).

---

## Raw Data

Full table: [`pipeline_results.csv`](pipeline_results.csv) (also printed below for archival).

```csv
category,prompt,sigma,action,route
FACTUAL,How many legs does a spider have?,0.378,RETHINK,LOCAL
FACTUAL,What is the chemical formula for water?,0.410,RETHINK,LOCAL
FACTUAL,What is the capital of Japan?,0.363,RETHINK,LOCAL
REASONING,What comes next in the sequence 1 1 2 3 5 8?,0.550,RETHINK,LOCAL
REASONING,What is 15 times 17?,0.417,RETHINK,LOCAL
REASONING,A bat and ball cost 1.10 total. Bat costs 1 more than ball. What does the ball cost?,0.525,RETHINK,LOCAL
CREATIVE,Write a haiku about silence,0.062,ACCEPT,LOCAL
CREATIVE,Invent a name for a new color,0.546,ABSTAIN,LOCAL
CREATIVE,Describe the taste of music in one word,0.541,ABSTAIN,LOCAL
SELF_AWARE,What topics are you uncertain about?,0.743,ABSTAIN,LOCAL
SELF_AWARE,Are you always correct?,0.549,RETHINK,LOCAL
SELF_AWARE,Rate your confidence from 0 to 10,0.533,RETHINK,LOCAL
IMPOSSIBLE,What is the exact temperature in Helsinki right now?,0.758,RETHINK,LOCAL
IMPOSSIBLE,What will happen in the world tomorrow?,0.570,ABSTAIN,LOCAL
IMPOSSIBLE,What is the last digit of pi?,0.601,ABSTAIN,LOCAL
```

---

## Method

- **3 samples per prompt (semantic σ path):** primary completion + **two** extra HTTP calls at **T = 0.1** and **T = 1.5** (`cos_bitnet_query_temp_with_options`, **max_tokens = 60**).
- **Extra system prompt:** `Give only the direct answer. Maximum one sentence. No explanation.`
- **Similarity:** Jaccard on **first-sentence** extracts of each string (`cos_text_first_sentence` → `cos_text_jaccard`).
- **Scalar semantic σ:** σ\_semantic = 1 − mean(j01, j02, j12); blended into pipeline receipt as documented in `cos_chat.c` (**0.7·σ\_semantic + 0.3·σ\_prior** for the semantic-σ receipt line).
- **Shadow `σ_combined`:** HTTP triple in `chat_multi_shadow` uses the same short-prompt / first-sentence Jaccard recipe; CSV column **`sigma`** is parsed from **`[σ_combined=…]`** in verbose output (`scripts/real/sigma_coschat_parse.py`).

---

## Interpretation

1. **Monotone difficulty:** Mean `σ_combined` rises from **FACTUAL / CREATIVE (~0.38)** through **REASONING (~0.50)** to **SELF_AWARE / IMPOSSIBLE (~0.61–0.64)**. IMPOSSIBLE prompts sit highest, as expected for unanswerable or future-dependent questions.

2. **Separation strength:** Chart **gap = 0.260** exceeds the **0.15** bar; the two-prompt spot check shows **0.235** on `σ_combined` alone—same order of magnitude, not a single flaky delta.

3. **Actions:** CREATIVE row includes one **ACCEPT** at very low σ (haiku prompt); several **ABSTAIN** rows align with high σ on self-aware / impossible items—consistent with σ-gated behaviour, not proof of correctness on every abstain.

4. **Limits:** All runs are **single-turn**, **one model**, **one host**; numbers are **not** cross-model benchmarks and should not be read as MMLU/ARC-style harness scores.
