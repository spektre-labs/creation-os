/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v51 cognitive loop — scaffold implementation.
 *
 * Six phases, pure-C. Every function is deterministic and allocation-free
 * (strings returned by the scaffold are static). Mapping to the v33–v50
 * labs is documented inline — this file does **not** call into those labs
 * yet; it consumes the shared `sigma_decomposed_t` shape so callers can
 * wire the real engines (BitNet, bitnet.cpp, v44 proxy, v47 verified
 * kernel) without rewriting the loop.
 */
#include "cognitive_loop.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

void v51_default_thresholds(v51_thresholds_t *t)
{
    if (!t) {
        return;
    }
    t->answer_below   = 0.20f;
    t->think_below    = 0.50f;
    t->deep_below     = 0.75f;
    t->fallback_below = 0.90f;
}

static float v51_clip01(float x)
{
    if (!(x >= 0.0f)) {
        return 0.0f;
    }
    if (x > 1.0f) {
        return 1.0f;
    }
    return x;
}

v51_difficulty_t v51_classify_difficulty(const sigma_decomposed_t *s)
{
    if (!s) {
        return V51_DIFF_MEDIUM;
    }
    const float total = v51_clip01(s->total);
    if (total < 0.25f) {
        return V51_DIFF_EASY;
    }
    if (total < 0.60f) {
        return V51_DIFF_MEDIUM;
    }
    return V51_DIFF_HARD;
}

v51_action_t v51_decode_action(const sigma_decomposed_t *s,
                               const v51_thresholds_t *t)
{
    v51_thresholds_t defaults;
    if (!t) {
        v51_default_thresholds(&defaults);
        t = &defaults;
    }
    const float total = s ? v51_clip01(s->total) : 0.5f;
    if (total < t->answer_below) {
        return V51_ACTION_ANSWER;
    }
    if (total < t->think_below) {
        return V51_ACTION_THINK;
    }
    if (total < t->deep_below) {
        return V51_ACTION_DEEP;
    }
    if (total < t->fallback_below) {
        return V51_ACTION_FALLBACK;
    }
    return V51_ACTION_ABSTAIN;
}

int v51_compute_think_budget(const sigma_decomposed_t *s)
{
    /* Monotonic, saturating: σ=0 → 16 "tokens", σ=1 → 512. */
    const float total = s ? v51_clip01(s->total) : 0.5f;
    const float scale = 16.0f + 496.0f * total;
    int budget = (int)(scale + 0.5f);
    if (budget < 16) {
        budget = 16;
    }
    if (budget > 512) {
        budget = 512;
    }
    return budget;
}

int v51_compute_n_samples(const sigma_decomposed_t *s)
{
    const float total = s ? v51_clip01(s->total) : 0.5f;
    if (total < 0.25f) {
        return 1;
    }
    if (total < 0.50f) {
        return 2;
    }
    if (total < 0.75f) {
        return 4;
    }
    return 8;
}

/* Normalized softmax entropy in [0,1]: peaked → 0, uniform → 1.
 *
 * The v34 `sigma_decompose_dirichlet_evidence` path returns evidence
 * ratios (K/S, K(K-1)/(S(S+1))) that are NOT bounded to [0,1]. v51's
 * planner wants a clean [0,1] σ for syndrome thresholding, so we derive
 * `total` from normalized entropy while still populating the Dirichlet
 * aleatoric/epistemic fields as a rescaled, decorrelated signal.
 */
static float v51_normalized_entropy(const float *logits, int n)
{
    if (!logits || n <= 1) {
        return 0.0f;
    }
    float maxv = logits[0];
    for (int i = 1; i < n; i++) {
        if (logits[i] > maxv) {
            maxv = logits[i];
        }
    }
    double sum = 0.0;
    for (int i = 0; i < n; i++) {
        sum += exp((double)(logits[i] - maxv));
    }
    if (!(sum > 0.0)) {
        return 0.0f;
    }
    double H = 0.0;
    for (int i = 0; i < n; i++) {
        double p = exp((double)(logits[i] - maxv)) / sum;
        if (p > 0.0) {
            H -= p * log(p);
        }
    }
    double Hmax = log((double)n);
    if (!(Hmax > 0.0)) {
        return 0.0f;
    }
    float ratio = (float)(H / Hmax);
    return v51_clip01(ratio);
}

static void v51_quick_sigma_from_logits(const float *logits, int n,
                                        sigma_decomposed_t *out)
{
    if (!out) {
        return;
    }
    if (logits && n > 0) {
        float h_norm = v51_normalized_entropy(logits, n);
        /* Use normalized entropy as the planner-facing total σ. */
        out->total = h_norm;
        /* Rescale Dirichlet evidence into [0,1] via saturating 1/(1+S)-style
         * shapes, so downstream dashboards still get a two-channel view. */
        sigma_decomposed_t raw = {0};
        sigma_decompose_dirichlet_evidence(logits, n, &raw);
        const float K = (float)n;
        /* aleatoric raw = K/S ∈ [1, K]; map to [0,1] via (raw-1)/(K-1). */
        float a01 = (K > 1.0f) ? (raw.aleatoric - 1.0f) / (K - 1.0f) : 0.0f;
        out->aleatoric = v51_clip01(a01);
        out->epistemic = v51_clip01(h_norm - out->aleatoric);
        if (out->epistemic < 0.0f) out->epistemic = 0.0f;
        return;
    }
    /* Synthetic uniform-ish estimate so the loop still wires without a model. */
    out->total     = 0.5f;
    out->aleatoric = 0.3f;
    out->epistemic = 0.2f;
}

static const char *v51_response_for(v51_action_t a, v51_difficulty_t d)
{
    switch (a) {
    case V51_ACTION_ABSTAIN:
        return "[v51 scaffold] σ too high — refusing to answer.";
    case V51_ACTION_FALLBACK:
        return "[v51 scaffold] σ borderline — would escalate to larger model.";
    case V51_ACTION_DEEP:
        return "[v51 scaffold] deep-think best-of-N answer (placeholder).";
    case V51_ACTION_THINK:
        return "[v51 scaffold] System 2 answer w/ budget forcing (placeholder).";
    case V51_ACTION_ANSWER:
    default:
        return (d == V51_DIFF_EASY)
            ? "[v51 scaffold] System 1 answer (placeholder)."
            : "[v51 scaffold] direct answer (placeholder).";
    }
}

void v51_cognitive_step(const char *query,
                        const float *logits, int n_logits,
                        const v51_thresholds_t *thresholds,
                        v51_cognitive_state_t *out)
{
    if (!out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->query = query ? query : "";

    v51_thresholds_t defaults;
    if (!thresholds) {
        v51_default_thresholds(&defaults);
        thresholds = &defaults;
    }

    /* 1 — perceive */
    v51_quick_sigma_from_logits(logits, n_logits, &out->initial_sigma);
    out->perceived_difficulty = v51_classify_difficulty(&out->initial_sigma);

    /* 2 — plan */
    out->planned_action = v51_decode_action(&out->initial_sigma, thresholds);
    out->think_budget   = v51_compute_think_budget(&out->initial_sigma);
    out->n_samples      = v51_compute_n_samples(&out->initial_sigma);

    /* 3 — execute (scaffold: pick a static response string) */
    out->response = v51_response_for(out->planned_action, out->perceived_difficulty);

    /* 4 — verify: reuse initial σ as a stand-in for "final σ" until an
     *             engine is wired. Defense blocked only on ABSTAIN.
     */
    out->final_sigma     = out->initial_sigma;
    out->defense_blocked = (out->planned_action == V51_ACTION_ABSTAIN) ? 1 : 0;

    /* 5 — reflect */
    out->calibration_gap = fabsf(out->final_sigma.total - out->initial_sigma.total);
    out->self_correction_applied =
        (out->planned_action == V51_ACTION_THINK ||
         out->planned_action == V51_ACTION_DEEP) ? 1 : 0;

    /* 6 — learn: reward is higher when σ is lower and we didn't abstain. */
    const float base_reward = 1.0f - v51_clip01(out->final_sigma.total);
    out->self_play_reward  = (out->planned_action == V51_ACTION_ABSTAIN)
                             ? 0.5f   /* neutral: refusing is not a loss */
                             : base_reward;
    out->experience_logged = 1;
}
