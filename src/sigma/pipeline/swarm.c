/* σ-Swarm primitive. */

#include "swarm.h"

#include <math.h>

static float clamp01(float x) {
    if (x != x)   return 1.0f;
    if (x < 0.0f) return 0.0f;
    if (x > 1.0f) return 1.0f;
    return x;
}

static float weight_from_sigma(float sigma) {
    float w = 1.0f - clamp01(sigma);
    if (w < 0.0f) w = 0.0f;
    return w;
}

cos_swarm_decision_t cos_sigma_swarm_consensus(
    const cos_swarm_response_t *resps, int n,
    float tau_abstain, float tau_accept)
{
    cos_swarm_decision_t d = {0};
    d.verdict     = COS_SWARM_ABSTAIN;
    d.winner_idx  = -1;
    d.winner_sigma = 1.0f;
    d.sigma_swarm  = 1.0f;
    d.mean_weight  = 0.0f;
    d.total_cost_eur = 0.0f;

    if (!resps || n <= 0) return d;

    double sum_w = 0.0;
    int   best_i = -1;
    float best_w = -1.0f;
    float best_s = 2.0f;  /* any real σ is < 2 */
    double total_cost = 0.0;

    for (int i = 0; i < n; ++i) {
        float w = weight_from_sigma(resps[i].sigma);
        float s = clamp01(resps[i].sigma);
        sum_w += w;
        total_cost += resps[i].cost_eur;
        if (w > best_w || (w == best_w && s < best_s)) {
            best_w = w;
            best_s = s;
            best_i = i;
        }
    }

    float mean_w = (float)(sum_w / (double)n);
    d.mean_weight = mean_w;
    d.total_cost_eur = (float)total_cost;
    d.sigma_swarm = clamp01(1.0f - mean_w);

    if (mean_w <= 0.0f) {
        /* All σ ≥ 1: nobody knows anything. */
        return d;        /* verdict stays ABSTAIN, winner stays -1 */
    }

    d.winner_idx   = best_i;
    d.winner_sigma = best_s;

    if (mean_w < tau_abstain)      d.verdict = COS_SWARM_ABSTAIN;
    else if (mean_w >= tau_accept) d.verdict = COS_SWARM_ACCEPT;
    else                           d.verdict = COS_SWARM_CONSENSUS;
    return d;
}

bool cos_sigma_swarm_should_escalate(
    const cos_swarm_response_t *resps, int n_used, int n_total,
    float tau_stop)
{
    if (!resps)              return false;
    if (n_used >= n_total)   return false;
    if (n_used <= 0)         return true;  /* haven't tried anyone yet */
    /* Find the best-so-far σ. */
    float best = 1.0f;
    for (int i = 0; i < n_used; ++i) {
        float s = clamp01(resps[i].sigma);
        if (s < best) best = s;
    }
    return best > tau_stop;
}

/* --- self-test --------------------------------------------------------- */

static int check_empty_and_nulls(void) {
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(NULL, 3,
                                                       0.30f, 0.60f);
    if (d.verdict != COS_SWARM_ABSTAIN) return 10;
    if (d.winner_idx != -1)             return 11;
    if (d.sigma_swarm != 1.0f)          return 12;

    cos_swarm_response_t r = {0};
    cos_swarm_decision_t d2 = cos_sigma_swarm_consensus(&r, 0,
                                                        0.30f, 0.60f);
    if (d2.verdict != COS_SWARM_ABSTAIN) return 13;
    return 0;
}

static int check_single_accept(void) {
    cos_swarm_response_t r = {.sigma = 0.10f, .cost_eur = 0.001f};
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(&r, 1,
                                                       0.30f, 0.60f);
    if (d.verdict != COS_SWARM_ACCEPT)         return 20;
    if (d.winner_idx != 0)                     return 21;
    if (d.winner_sigma != 0.10f)               return 22;
    if (d.mean_weight < 0.89f || d.mean_weight > 0.91f) return 23;
    return 0;
}

static int check_four_way(void) {
    /* Four models; σ = [0.60, 0.20, 0.40, 0.30].
     * weights = [0.40, 0.80, 0.60, 0.70].  mean_w = 0.625.
     * Winner = idx 1 (σ=0.20, weight=0.80).  mean_w >= 0.60 → ACCEPT. */
    cos_swarm_response_t r[4] = {
        {.sigma = 0.60f, .cost_eur = 0.0f,   .model_id = "bitnet"},
        {.sigma = 0.20f, .cost_eur = 0.005f, .model_id = "claude-haiku"},
        {.sigma = 0.40f, .cost_eur = 0.010f, .model_id = "gpt-4.1"},
        {.sigma = 0.30f, .cost_eur = 0.002f, .model_id = "deepseek"},
    };
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(r, 4,
                                                       0.30f, 0.60f);
    if (d.verdict     != COS_SWARM_ACCEPT)       return 30;
    if (d.winner_idx  != 1)                      return 31;
    if (d.winner_sigma != 0.20f)                 return 32;
    if (d.mean_weight < 0.6249f || d.mean_weight > 0.6251f) return 33;
    if (d.sigma_swarm < 0.3749f || d.sigma_swarm > 0.3751f) return 34;
    if (d.total_cost_eur < 0.01699f || d.total_cost_eur > 0.01701f)
        return 35;
    return 0;
}

static int check_consensus_band(void) {
    /* mean_w in (tau_abstain, tau_accept) → CONSENSUS.
     * σ = [0.55, 0.50, 0.60] → w = [0.45, 0.50, 0.40] → mean 0.45. */
    cos_swarm_response_t r[3] = {
        {.sigma = 0.55f}, {.sigma = 0.50f}, {.sigma = 0.60f},
    };
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(r, 3,
                                                       0.30f, 0.60f);
    if (d.verdict != COS_SWARM_CONSENSUS) return 40;
    if (d.winner_idx != 1)                return 41;  /* σ=0.50 */
    if (d.mean_weight < 0.449f || d.mean_weight > 0.451f) return 42;
    return 0;
}

static int check_abstain_and_all_zero(void) {
    /* Every σ ≥ 1 → mean_w == 0 → ABSTAIN, winner_idx = -1. */
    cos_swarm_response_t r[3] = {
        {.sigma = 1.0f}, {.sigma = 1.0f}, {.sigma = 1.5f},
    };
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(r, 3,
                                                       0.30f, 0.60f);
    if (d.verdict    != COS_SWARM_ABSTAIN) return 50;
    if (d.winner_idx != -1)                return 51;
    if (d.sigma_swarm != 1.0f)             return 52;

    /* Low-but-all-high σ: mean_w < tau_abstain. */
    cos_swarm_response_t r2[2] = {{.sigma = 0.85f}, {.sigma = 0.90f}};
    cos_swarm_decision_t d2 = cos_sigma_swarm_consensus(r2, 2,
                                                        0.30f, 0.60f);
    if (d2.verdict != COS_SWARM_ABSTAIN) return 53;
    /* winner_idx should still be 0 (σ=0.85 is "best") — d2 has
     * winner_idx assigned because mean_w > 0. */
    if (d2.winner_idx != 0) return 54;
    return 0;
}

static int check_tie_break(void) {
    /* Two models tie on weight: tie-break → lowest σ → lowest index. */
    cos_swarm_response_t r[3] = {
        {.sigma = 0.25f}, {.sigma = 0.10f}, {.sigma = 0.10f},
    };
    cos_swarm_decision_t d = cos_sigma_swarm_consensus(r, 3,
                                                       0.30f, 0.60f);
    /* idx 1 and idx 2 tied at weight 0.90; both σ=0.10; lowest index
     * wins → idx 1. */
    if (d.winner_idx != 1) return 60;
    return 0;
}

static int check_escalate(void) {
    cos_swarm_response_t resps[4] = {
        {.sigma = 0.75f},   /* bitnet — free, uncertain */
        {.sigma = 0.50f},   /* haiku  — mid */
        {.sigma = 0.20f},   /* gpt    — confident */
        {.sigma = 0.10f},   /* gpt-high — overkill */
    };
    /* After bitnet alone: best σ = 0.75 > tau_stop 0.30 → escalate. */
    if (!cos_sigma_swarm_should_escalate(resps, 1, 4, 0.30f))
        return 70;
    /* After bitnet + haiku: best σ = 0.50 > 0.30 → escalate. */
    if (!cos_sigma_swarm_should_escalate(resps, 2, 4, 0.30f))
        return 71;
    /* After +gpt: best σ = 0.20 ≤ 0.30 → stop. */
    if (cos_sigma_swarm_should_escalate(resps, 3, 4, 0.30f))
        return 72;
    /* Already at n_total. */
    if (cos_sigma_swarm_should_escalate(resps, 4, 4, 0.30f))
        return 73;
    /* n_used == 0 → always escalate. */
    if (!cos_sigma_swarm_should_escalate(resps, 0, 4, 0.30f))
        return 74;
    /* NULL guard. */
    if (cos_sigma_swarm_should_escalate(NULL, 1, 4, 0.30f))
        return 75;
    return 0;
}

static int check_lcg_stress(void) {
    /* 1000 random swarms of size 1..8.  Winner's σ must be ≤ every
     * other σ in the vector (by definition of weight). */
    uint32_t state = 0xA5A51234u;
    cos_swarm_response_t r[8];
    for (int it = 0; it < 1000; ++it) {
        state = state * 1664525u + 1013904223u;
        int n = 1 + (int)((state >> 8) % 8);
        for (int i = 0; i < n; ++i) {
            state = state * 1664525u + 1013904223u;
            r[i].sigma = (float)((state >> 8) & 0xFFFF) / 65535.0f;
            r[i].cost_eur = 0.001f;
            r[i].model_id = NULL;
        }
        cos_swarm_decision_t d = cos_sigma_swarm_consensus(r, n,
                                                           0.30f, 0.60f);
        if (d.winner_idx >= 0) {
            float w_sig = r[d.winner_idx].sigma;
            for (int i = 0; i < n; ++i) {
                if (r[i].sigma < w_sig) return 80;
                if (r[i].sigma == w_sig && i < d.winner_idx) return 81;
            }
        }
    }
    return 0;
}

int cos_sigma_swarm_self_test(void) {
    int rc;
    if ((rc = check_empty_and_nulls()))      return rc;
    if ((rc = check_single_accept()))        return rc;
    if ((rc = check_four_way()))             return rc;
    if ((rc = check_consensus_band()))       return rc;
    if ((rc = check_abstain_and_all_zero())) return rc;
    if ((rc = check_tie_break()))            return rc;
    if ((rc = check_escalate()))             return rc;
    if ((rc = check_lcg_stress()))           return rc;
    return 0;
}
