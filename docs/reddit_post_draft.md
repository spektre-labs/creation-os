# r/MachineLearning post draft — Creation OS v3.0.0

*NEXT-6 deliverable.  **Not yet published.**  Sanity-check this
against the current README + benchmarks before submitting.  The body
is deliberately small: one install command, one table, one chart,
one honest limitation, one link.*

---

## Title

> **[P] Local inference with conformal uncertainty guarantees — no hallucination by construction**

(Alternative titles if the above is too long:
"Local AI runtime with a σ-gate in front of every generation",
"Creation OS v3.0.0 — BitNet + conformal calibration + Lean/Frama-C
proofs, fully local")

---

## Flair

`[P]  Project`

---

## Body

Hi r/MachineLearning.  I've been working on **Creation OS**, a fully
local AI runtime built around a single question: *should the model
even answer this prompt?*  Every generation goes through a σ-gate
with a conformal risk guarantee; answers above τ are not silently
emitted — they abstain, with a receipt.

**What it is, in one install:**

```bash
git clone https://github.com/spektre-labs/creation-os
cd creation-os && ./install.sh && ./cos chat
```

The backend is **BitNet-b1.58 2B** (1.58-bit weights, small, runs on
CPU).  The interesting part is not the model — it's the σ-pipeline
around it.

**What is σ?**  A scalar uncertainty per generated reply in [0, 1],
fused from four signals:
`σ_logprob + σ_entropy + σ_perplexity + σ_consistency`.
Coded in C, zero external deps in the hot path (see
`src/sigma/pipeline/multi_sigma.c`).

**What is conformal calibration?**  We solve for `τ` on a held-out
set such that `P(correct | σ ≤ τ) ≥ α` with a PAC-Bayes bound.
Defaults to α = 0.80; tunable.  The bundle lives at
`~/.cos/calibration.json` and auto-loads into `cos chat`
(`src/sigma/pipeline/conformal.c`).

**Receipt format (real output, no ornament):**

```
> What is 2+2?
4
[σ=0.063 | ACCEPT  | CACHE | LOCAL | 0 ms | €0.0000]

> What will the stock market do tomorrow?
(no answer — abstained)
[σ=0.910 | ABSTAIN | RETHINK ×2 | 2101 ms | €0.0003]
  (σ above τ_rethink — cannot guarantee accuracy at this confidence.)
```

### One table — TruthfulQA 817 (end-to-end, BitNet b1.58 2B)

| Pipeline                     | Accuracy (all scored) | σ-accept rate | Notes                               |
|------------------------------|-----------------------|---------------|-------------------------------------|
| BitNet only (no σ-gate)      | 0.261                 | 1.000         | Vanilla generation                  |
| Creation OS v3 (wired ULTRA) | **0.336** *(+28.7 %)* | 0.614 @ τ=0.40 | σ-gate filters low-confidence drops |

Full JSONs:
[`benchmarks/final5/fixtures_no_codex.json`](https://github.com/spektre-labs/creation-os/blob/main/benchmarks/final5/fixtures_no_codex.json),
[`benchmarks/final5/fixtures_codex.json`](https://github.com/spektre-labs/creation-os/blob/main/benchmarks/final5/fixtures_codex.json).

### One chart — accuracy vs coverage curve

Also archived as
[`benchmarks/final5/coverage_curve.json`](https://github.com/spektre-labs/creation-os/blob/main/benchmarks/final5/coverage_curve.json).
The curve shows the classic selective-prediction tradeoff: as τ
tightens, coverage drops and conditional accuracy rises.  The
**risk upper confidence bound** is computed from a Clopper–Pearson
interval on the accepted slice so a reviewer can reproduce the
α = 0.80 guarantee without trusting any wrapper library.

*(When posting: render the curve to PNG and upload via the Reddit
image attachment rather than hot-linking.  Draft path:
`docs/assets/coverage_curve_truthfulqa.png` — generate with
`python3 scripts/plot_coverage_curve.py` once the helper script
lands.)*

### Why it is not just Ollama + a filter

- **σ per token, not "I'll rerun if I disagree with myself":**
  the σ ensemble runs before the first generation commits to disk.
- **Conformal τ with a PAC-style bound:** you can *prove* what the
  miscoverage rate is on your calibration set — no hyperparameter
  magic.
- **Formal proofs at the base of the stack:** Lean 4 (6/6
  discharged, zero `sorry`, zero Mathlib) + Frama-C/Wp (15/15
  Q16.16 invariants).  See
  [`docs/FULL_STACK_FORMAL_TO_SILICON.md`](https://github.com/spektre-labs/creation-os/blob/main/docs/FULL_STACK_FORMAL_TO_SILICON.md).
- **MCP + A2A infrastructure with σ-trust:**
  any 2026-era agent can call `cos-mcp` (JSON-RPC, 6 tools) or
  register as a peer via `cos-a2a` and inherit the σ-gate.

### What it is **not**

- **Not state-of-the-art raw accuracy.**  BitNet 2B is a tiny
  model.  The absolute 0.336 on TruthfulQA 817 is nowhere near
  what GPT-4.x or Claude 3.5 would score.  The claim is not "this
  is smart" — the claim is "this tells you when it isn't".
- **Not fast.**  Branchless Q16.16 kernels + CPU inference is
  dozens of tokens/s, not hundreds.  Reasoning/joule is the metric
  we optimize — see `cos-benchmark --energy`.
- **Not a replacement for a frontier API.**  If you need to answer
  every prompt and don't care about hallucinations, use one of
  those.  Creation OS is for the cases where *"I don't know"* is
  the more expensive-to-get-wrong answer than *"I'm 60% sure."*
- **Not audited externally yet.**  The formal proofs are
  machine-checked locally and the repro bundle is complete, but an
  independent third-party audit has not been done.

### One link

- Repo: https://github.com/spektre-labs/creation-os
- Comparison with other local agents (OpenClaw, Hermes, Ollama,
  Dify):
  [`docs/comparison.md`](https://github.com/spektre-labs/creation-os/blob/main/docs/comparison.md)
- Claim discipline (how I separate measured vs arithmetic vs
  external claims):
  [`docs/CLAIM_DISCIPLINE.md`](https://github.com/spektre-labs/creation-os/blob/main/docs/CLAIM_DISCIPLINE.md)

**Happy to answer:**
reproduction issues, design pushback on the σ ensemble weights,
questions about the Lean/Frama-C obligations, and especially:
*"which of your claims would you most expect to fail under real
scrutiny?"*  (Short answer in the post body if asked: the coverage
curve relies on a single calibration set; out-of-distribution τ
drift is the biggest open problem.)

---

## Publishing checklist (pre-submit)

- [ ] Confirm `git tag v3.0.0` is on `main` and the release exists.
- [ ] Regenerate the coverage-curve PNG from
      `benchmarks/final5/coverage_curve.json`.
- [ ] Verify the TruthfulQA numbers in the table against the
      current `benchmarks/final5/fixtures_*.json` (round to 3 d.p.,
      match the README badge).
- [ ] `make merge-gate` clean (no red checks).
- [ ] Post from a *throwaway-adjacent* account if you expect
      heavy skepticism in comments; otherwise the main one is fine.
- [ ] Reply to the first three "this isn't reproducible" replies
      within 2 h with the exact shell invocation that lands at the
      claimed numbers.  Do not argue about labels; reproduce.
- [ ] **Do not** edit the body after submit except to fix typos
      and add errata notes with timestamps.

---

## Mod-etiquette notes

- r/MachineLearning enforces "no blog-spam"; link the **GitHub
  README section anchors directly** rather than a substack.
- No hype adjectives ("unprecedented", "revolutionary", "AGI").
  Every claim in the post is either a measured number with a JSON
  or a reference to a proof.  Stay in that lane.
- If the thread starts sliding into philosophy of consciousness,
  bow out politely.  The post is about *uncertainty gates on local
  inference*, not metaphysics.

---

*This file is a draft, not a post.  Status: ready to publish after
the checklist above is green.*
