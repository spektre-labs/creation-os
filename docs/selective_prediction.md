# Selective prediction and conformal σ-calibration (SCI-4)

**Status:** canonical formalization of the σ-gate.

**Audience:** ML-research readers who want to know what guarantee Creation OS actually delivers, in the language of the field.

**One-sentence summary:** Creation OS is a **selective predictor** (Geifman & El-Yaniv, NeurIPS 2017) whose selection rule is **conformally calibrated** (Vovk, Gammerman, Shafer 2005; Angelopoulos & Bates, arXiv:2107.07511), giving a finite-sample distribution-free upper bound on the selective error rate.

---

## 1. Selective prediction — the framework

A **selective predictor** is a pair \( (f, g) \) where

* \( f : \mathcal{X} \to \mathcal{Y} \) is the base predictor (in our case, BitNet b1.58 2B through `llama-server`),
* \( g : \mathcal{X} \to \{0, 1\} \) is the **selection function** — `g(x) = 1` means *accept* (commit to `f(x)`), `g(x) = 0` means *abstain*.

The **Creation OS σ-gate** instantiates \( g \) as

\[
g_\tau(x) \;=\; \mathbf{1}\!\bigl[\, \sigma(x) \le \tau \,\bigr],
\]

where \( \sigma(x) \) is the per-response uncertainty derived from per-token log-probabilities on the `/v1/chat/completions` endpoint (see [`src/import/bitnet_server.c`](../src/import/bitnet_server.c); in short, \( \sigma_\text{token} = 1 - p_\text{selected} \), \( \sigma_\text{response} = \max_t \sigma_\text{token}(t) \)).

Two quantities characterize a selective predictor:

| Quantity | Definition |
|----------|-----------|
| **Coverage** | \( C(\tau) \;=\; \mathbb{E}\bigl[ g_\tau(X) \bigr] \) |
| **Selective risk** | \( R(\tau) \;=\; \mathbb{E}\bigl[\, \ell(f(X), Y) \,\bigm|\, g_\tau(X) = 1 \,\bigr] \) |

with \( \ell \) the 0-1 loss on scored rows (`correct ∈ {0,1}`). The curve \( \tau \mapsto (C(\tau), 1 - R(\tau)) \) is the **accuracy-coverage trade-off** and is exactly what `cos-coverage-curve` writes into [`benchmarks/coverage_curve.json`](../benchmarks/coverage_curve.json).

**Why this matters:** in selective prediction the operator *chooses* the trade-off — a tighter \( \tau \) yields higher accuracy on accepted rows at the cost of abstaining more. The σ-gate exposes this trade-off as a single runtime knob.

---

## 2. Conformal calibration of \( \tau \) — the guarantee

Classical selective-prediction papers leave \( \tau \) as a free parameter. We calibrate it *with a finite-sample guarantee* by using a held-out labelled set and the **split conformal prediction** scaffolding (Vovk, Gammerman, Shafer 2005) specialised to binary selective-risk control via **Learn-then-Test** (Angelopoulos, Bates, Candès, Jordan 2021, arXiv:2110.01052).

### 2.1 Setup

Let \( \{(\sigma_i, y_i)\}_{i=1}^N \) be an i.i.d. labelled calibration set with \( y_i \in \{0, 1\} \) the 0-1 correctness indicator. Fix a target error rate \( \alpha \in (0, 1) \) and confidence parameter \( \delta \in (0, 1) \).

### 2.2 Empirical risk at \( \tau \)

\[
\hat{R}(\tau) \;=\; \dfrac{1}{k(\tau)} \sum_{i=1}^N \mathbf{1}\!\bigl[\, \sigma_i \le \tau \,\wedge\, y_i = 0 \,\bigr],
\qquad
k(\tau) \;=\; \#\!\bigl\{ i : \sigma_i \le \tau \bigr\}.
\]

### 2.3 Hoeffding upper bound on \( R(\tau) \)

With probability at least \( 1 - \delta \) over the calibration draw,

\[
R(\tau) \;\le\; \hat{R}(\tau) + \sqrt{\dfrac{\log(1/\delta)}{2\,k(\tau)}} \;=:\; \mathrm{UCB}(\tau).
\]

Hoeffding's one-sided bound dominates the mean of i.i.d. \( [0,1] \)-valued random variables and is strictly conservative (Clopper-Pearson is tighter but requires the inverse regularized incomplete beta — out of scope for a libm-only kernel).

### 2.4 Risk-controlling threshold

Creation OS picks

\[
\tau^{*} \;=\; \max \Bigl\{ \tau : \mathrm{UCB}(\tau) \le \alpha \,\wedge\, k(\tau) \ge k_{\min} \Bigr\},
\]

where \( k_{\min} = 10 \) is the minimum support size guarding against vacuous bounds.

**Guarantee.** With probability at least \( 1 - \delta \) over the calibration draw,

\[
\boxed{\;P\bigl(Y = 0 \,\bigm|\, g_{\tau^{*}}(X) = 1\bigr) \;\le\; \alpha.\;}
\]

Equivalently: *among accepted responses, the error rate is at most \( \alpha \)*, with high probability over the calibration sample.

### 2.5 Abstain-by-default

If no \( \tau \) satisfies the UCB bound (typically because \( N \) is too small for the requested \( (\alpha, \delta) \)), the calibration returns `valid = 0` and the runtime gate falls back to abstaining on every query. This is the conservative choice: we never *claim* a guarantee we cannot prove.

---

## 3. Adaptive \( \tau \) under distribution shift (SCI-2)

Split conformal assumes i.i.d. (or exchangeable) data between calibration and deployment. When the deployment stream drifts — which is the norm, not the exception — we add **Adaptive Conformal Inference** (Gibbs & Candès, NeurIPS 2021, arXiv:2106.00170):

\[
\tau_{t+1} \;=\; \mathrm{clip}_{[0,1]}\bigl(\tau_t + \eta \cdot (\,\mathbf{1}[\sigma_t > \tau_t] - \alpha\,)\bigr),
\]

with learning rate \( \eta \) (default 0.05). Gibbs-Candès show this update preserves the *long-run* marginal coverage \( \mathbb{E}[\mathbf{1}(\sigma_t > \tau_t)] \to \alpha \) under **arbitrary** distribution drift, no mixing or stationarity assumption required.

The kernel exposes the update rule as a pure function `cos_conformal_aci_update` so the runtime gate can persist τ across sessions (the wire-in is scheduled for a later commit; today ACI is available as a library surface with a self-test that drives τ to the (1−α)-quantile on synthetic uniform data).

Per-domain calibration (also SCI-2) partitions the calibration set by a deterministic prompt classifier (`cos_conformal_classify_prompt`) into {`factual`, `code`, `reasoning`, `other`} and runs the Hoeffding/LtT procedure independently on each partition. At deployment time the τ used for a given query is the one calibrated on its matching domain.

---

## 4. Mapping to Creation OS artifacts

| Concept | Where it lives |
|---------|-----------------|
| \( g_\tau \) — σ-gate decision | [`src/sigma/pipeline/pipeline.c`](../src/sigma/pipeline/pipeline.c) |
| σ measurement (logprobs) | [`src/import/bitnet_server.c`](../src/import/bitnet_server.c) |
| \( \tau^{*} \) — risk-controlled threshold | [`src/sigma/pipeline/conformal.{h,c}`](../src/sigma/pipeline/conformal.h) |
| ACI online update | `cos_conformal_aci_update` in the same file |
| Prompt classifier | `cos_conformal_classify_prompt` in the same file |
| Coverage-curve sweep | [`src/sigma/pipeline/coverage_curve_main.c`](../src/sigma/pipeline/coverage_curve_main.c) |
| Calibration CLI | `./cos-calibrate` (see `--help`) |
| Sweep CLI | `./cos-coverage-curve` (see `--help`) |
| Persisted bundle | `~/.cos/calibration.json`, schema `cos.conformal.v1` |
| Persisted curve | [`benchmarks/coverage_curve.json`](../benchmarks/coverage_curve.json), schema `cos.coverage_curve.v1` |
| Live TruthfulQA evidence | [`benchmarks/pipeline/truthfulqa_817.json`](../benchmarks/pipeline/truthfulqa_817.json) (aggregates) and [`benchmarks/pipeline/truthfulqa_817_detail.jsonl`](../benchmarks/pipeline/truthfulqa_817_detail.jsonl) (per-row). |

---

## 5. Empirical connection to v111.1 results

The v111.1 pre-registered σ-vs-entropy parity matrix (see [`benchmarks/v111`](../benchmarks/v111/)) reported

* \( \Delta \mathrm{AURCC} = -0.0447 \), \( p = 0.0005 \) (Bonferroni-corrected)

where AURCC is the **area under the risk-coverage curve** — *exactly* the selective-prediction integral discussed above, but across N hyper-parameter grids. A more negative ΔAURCC means the σ-gated curve lies strictly below the entropy-gated curve over the entire coverage range, i.e. σ dominates entropy as a selective-risk signal with multiple-comparison-corrected significance. The conformal calibration of §2 pins one operating point on that curve with a coverage guarantee; the v111.1 matrix stats say the whole curve is better than the natural entropy baseline.

These two results live in **different evidence classes** (see [CLAIM_DISCIPLINE.md](CLAIM_DISCIPLINE.md)): v111.1 is a pre-registered statistical test, §2 here is a runtime-reproducible per-call guarantee. Never merge them into one sentence. The σ-gate story is the *intersection* — it is simultaneously the empirically-dominant selective predictor (v111.1) and the calibrated-guarantee one (§2).

---

## 6. What conformal calibration does *not* give you

Failure modes to name explicitly:

1. **Conditional guarantees.** Hoeffding + LtT controls *marginal* selective risk. It does **not** certify per-instance accuracy, per-category accuracy (unless you calibrate per category — see §3), or worst-case behavior on adversarial prompts.
2. **Distribution shift.** Split conformal needs exchangeability between calibration and deployment. When that breaks, fall back to ACI (§3). If the shift is catastrophic (new language, new task family), the ACI long-run guarantee is still marginal — you can be *right on average over time* while being *wrong on today's distribution*.
3. **Label noise.** The Hoeffding bound is only as good as the `correct` field in the calibration JSONL. TruthfulQA's substring-match scoring (`cos-chat` harness) is conservative on the numerator; rows whose generated text contains neither a correct nor an incorrect string are dropped, not counted as errors. See [BENCHMARK_PROTOCOL.md](BENCHMARK_PROTOCOL.md).
4. **Coverage-accuracy Pareto.** A very small \( \alpha \) often forces \( \tau^{*} \) so low that `valid = 0` (abstain-by-default). This is a finite-sample regime where the Hoeffding radius dominates. The remedy is either a larger calibration set, a tighter UCB (Clopper-Pearson, Wilson), or a relaxed \( \alpha \). The calibration JSON records the empirical risk alongside the UCB so the operator can see the regime.
5. **"Selected but uncalibrated" trap.** Running `cos chat` *without* `cos-calibrate` uses a static `tau_accept` fallback from `cos_sigma_defaults_t`; the conformal guarantee does **not** apply in that mode. The production recipe is: calibrate once on a representative set, persist `~/.cos/calibration.json`, re-calibrate when the data distribution drifts.

---

## 7. Further reading

* Geifman, Y., & El-Yaniv, R. (2017). *Selective classification for deep neural networks.* NeurIPS.
* Vovk, V., Gammerman, A., & Shafer, G. (2005). *Algorithmic learning in a random world.* Springer.
* Angelopoulos, A. N., Bates, S. (2021). *A gentle introduction to conformal prediction and distribution-free uncertainty quantification.* arXiv:2107.07511.
* Angelopoulos, A. N., Bates, S., Candès, E. J., Jordan, M. I., Lei, L. (2021). *Learn then test: calibrating predictive algorithms to achieve risk control.* arXiv:2110.01052.
* Gibbs, I., Candès, E. J. (2021). *Adaptive conformal inference under distribution shift.* NeurIPS. arXiv:2106.00170.

---

*Spektre Labs · Creation OS · 2026*
