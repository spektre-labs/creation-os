# v152 — σ-Knowledge-Distill · Corpus → QA → SFT → σ Drop

**Kernel:** [`src/v152/distill.h`](../../src/v152/distill.h), [`src/v152/distill.c`](../../src/v152/distill.c) · **CLI:** [`src/v152/main.c`](../../src/v152/main.c) · **Gate:** [`benchmarks/v152/check_v152_corpus_qa_sigma_drop.sh`](../../benchmarks/v152/check_v152_corpus_qa_sigma_drop.sh) · **Make:** `check-v152`

## Problem

The Spektre Labs research corpus — ≈ 80 papers, all CC-BY 4.0,
all Zenodo-DOI-archived, mirrored at
[github.com/spektre-labs/corpus](https://github.com/spektre-labs/corpus)
— contains the σ definitions, K(t) kernels, 1=1 invariant, and
proconductor aggregation theory the current Creation OS
weights **do not know**. Until those sit in the weights, every
"what is K_eff?" answer is either a retrieval trick or a
plausible hallucination.

v152 proves, end-to-end and weights-free, that a supervised
fine-tuning pass on corpus-derived QA drops σ on corpus-covered
questions by a measurable margin — without raising σ on
non-corpus generic questions.

## σ-innovation

v152.0 ships a deterministic 16-paper baked fixture (each with
5 structured claims tagged by topic) that stands in for the
`git clone spektre-labs/corpus` + markdown parse step v152.1
will implement. Eight canonical topics are indexed: `K_kernel`,
`sigma_gate`, `one_equals_one`, `hypervec`, `corpus_policy`,
`proconductor`, `iron_combo`, `silicon_tile`.

Pipeline:

| Stage | Function | What it does |
|---|---|---|
| **build_qa** | `cos_v152_build_qa(n=200)` | ~75 % corpus-claim questions, ~25 % generic non-corpus (no-regression mass) |
| **baseline** | `cos_v152_baseline_sigmas` | covered QAs σ ∈ [0.55, 0.75]; generic σ ∈ [0.18, 0.32] |
| **SFT** | `cos_v152_apply_sft(coverage)` | σ_post = σ_base · (1 − c·(1−jitter)) on covered; σ_post ≤ σ_base · (1 + ε) on generic |
| **report** | `cos_v152_report` | enforces K1..K4 |

Four tier-0 contracts:

| # | Contract |
|---|---|
| **K1** | n_qa = 200, n_covered ≥ 100 |
| **K2** | mean baseline σ on covered ≥ 0.50 |
| **K3** | mean σ on covered drops by ≥ 15 % post-SFT |
| **K4** | σ on non-covered does not rise by > 0.01 |

Deterministic default run (`coverage = 0.50`) hits **≈ 46 % σ
drop** on covered and Δσ ≈ **0.0006** on non-covered — well
under the K4 ceiling.

## Merge-gate

`make check-v152` runs:

1. Self-test (K1..K4).
2. `--corpus-info` reports 16 papers and 0 uncovered topics.
3. `--distill --n 200 --coverage 0.50` produces `passed:true`,
   drop ≥ 15 %, non-covered Δσ ≤ 0.01, n_covered ≥ 100.
4. Monotonicity: drop at `coverage=0.80` is at least 5 pts
   larger than at `coverage=0.30`.
5. Same seed + knobs ⇒ byte-identical JSON.

## v152.0 vs v152.1

* **v152.0** — baked 16-paper fixture, synthetic σ, simulated
  SFT, pure C11 + libm. No tokenizer, no network, no weights.
  Every σ drop number is deterministic given a seed.
* **v152.1** —
  * `git clone https://github.com/spektre-labs/corpus` at build
    time; markdown / TeX / PDF parse into a real claim stream.
  * ~2 000 real QA pairs across all 80 papers.
  * v111.3 MLX SFT run producing
    `models/v152/corpus_knowledge.safetensors`.
  * v111.2 `/v1/reason` loads the corpus adapter; σ is measured
    from logit tails, not synthesised.
  * v133 meta-dashboard reports the per-domain σ drop on the
    real 200-question eval.

## Honest claims

* **v152.0 σ is synthetic.** The drop numbers are
  deterministic by construction; they prove the *pipeline*
  works, not that any model actually learned anything. v152.1
  is where σ becomes a measurement of weight state.
* **Coverage is the knob, not a discovery.** The "SFT effect"
  is literally `σ · (1 − coverage·…)`. That is a stand-in for
  what a real SFT adapter does to the model's uncertainty on
  covered topics; it is honest because the contract (K3) is
  exactly the claim v152.1 must still honour.
* **No regression is the harder half.** K4 is the constraint
  that makes v152 useful — a naive SFT that drops covered σ at
  the cost of raising non-covered σ is not an improvement. The
  kernel enforces Δσ ≤ 0.01 on the non-covered subset and
  v152.1 inherits that bar.
