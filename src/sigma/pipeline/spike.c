/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Spike — LIF implementation of the σ-gate. */

#include "spike.h"

#include <math.h>
#include <string.h>

static float clampf(float x, float lo, float hi) {
    if (x != x) return lo;
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

int cos_sigma_spike_init(cos_spike_neuron_t *n, float threshold,
                         float decay, float reset_potential)
{
    if (!n)                                  return -1;
    if (!(threshold > 0.0f))                 return -2;
    if (!(decay >= 0.0f && decay <= 1.0f))   return -3;
    if (!(reset_potential < threshold))      return -4;
    memset(n, 0, sizeof *n);
    n->threshold       = threshold;
    n->decay           = decay;
    n->reset_potential = reset_potential;
    n->membrane_potential = reset_potential;
    return 0;
}

int cos_sigma_spike_step(cos_spike_neuron_t *n, float input) {
    if (!n) return -1;
    if (input != input) return -1;

    n->membrane_potential = n->decay * n->membrane_potential + input;
    n->n_steps++;
    if (n->membrane_potential >= n->threshold) {
        n->membrane_potential = n->reset_potential;
        n->spiked = 1;
        n->n_spikes++;
        return 1;
    }
    n->spiked = 0;
    return 0;
}

float cos_sigma_spike_sigma(const cos_spike_neuron_t *n) {
    if (!n) return 1.0f;
    if (n->spiked) return 0.0f;
    float ratio = n->membrane_potential / n->threshold;
    /* σ is monotone with distance-to-threshold: V near 0 → σ≈1
     * (far from accept), V near V_th → σ≈0 (near accept).   */
    float sigma = 1.0f - ratio;
    return clampf(sigma, 0.0f, 1.0f);
}

float cos_sigma_spike_population_sigma(const cos_spike_neuron_t *pop,
                                       int n, int window_len, int rate_max)
{
    if (!pop || n <= 0 || window_len <= 0 || rate_max <= 0) return 1.0f;
    /* We use the lifetime spike count over the last `window_len`
     * steps.  If the neurons have seen fewer steps than the
     * window, use whatever they have.                           */
    long total_spikes = 0;
    int  total_steps  = 0;
    for (int i = 0; i < n; ++i) {
        int steps = pop[i].n_steps < window_len ? pop[i].n_steps
                                                : window_len;
        total_steps  += steps;
        total_spikes += pop[i].n_spikes;
    }
    if (total_steps <= 0) return 1.0f;
    /* Population rate: spikes per (steps · rate_max).  A fully
     * saturated population (rate_max per window per neuron)
     * gives σ ≈ 0; silence gives σ ≈ 1.                         */
    float rate = (float)total_spikes
               / ((float)n * (float)window_len);
    float normalised = rate / (float)rate_max;
    float sigma = 1.0f - normalised;
    return clampf(sigma, 0.0f, 1.0f);
}

/* ---------- self-test ---------- */

static float fabsf7(float x) { return x < 0.0f ? -x : x; }

static int check_guards(void) {
    cos_spike_neuron_t n;
    if (cos_sigma_spike_init(NULL, 1.0f,  0.9f, 0.0f) == 0) return 10;
    if (cos_sigma_spike_init(&n,   0.0f,  0.9f, 0.0f) == 0) return 11;
    if (cos_sigma_spike_init(&n,   1.0f, -0.1f, 0.0f) == 0) return 12;
    if (cos_sigma_spike_init(&n,   1.0f,  0.9f, 2.0f) == 0) return 13;
    return 0;
}

static int check_fire(void) {
    cos_spike_neuron_t n;
    cos_sigma_spike_init(&n, /*V_th=*/1.0f, /*decay=*/1.0f,
                         /*V_reset=*/0.0f);
    /* Feed 0.3 three times → V reaches 0.9, no spike. */
    for (int i = 0; i < 3; ++i) {
        if (cos_sigma_spike_step(&n, 0.3f) != 0) return 20;
    }
    if (fabsf7(n.membrane_potential - 0.9f) > 1e-5f) return 21;
    if (n.n_spikes != 0)                              return 22;
    /* One more input of 0.2 crosses the threshold → spike. */
    if (cos_sigma_spike_step(&n, 0.2f) != 1) return 23;
    if (n.n_spikes != 1)                      return 24;
    if (n.membrane_potential != 0.0f)         return 25;
    return 0;
}

static int check_sigma_mapping(void) {
    cos_spike_neuron_t n;
    cos_sigma_spike_init(&n, 1.0f, 1.0f, 0.0f);
    /* Silent, V=0 → σ=1. */
    if (fabsf7(cos_sigma_spike_sigma(&n) - 1.0f) > 1e-5f) return 30;
    cos_sigma_spike_step(&n, 0.5f);
    /* V=0.5, threshold=1.0 → σ=0.5. */
    if (fabsf7(cos_sigma_spike_sigma(&n) - 0.5f) > 1e-5f) return 31;
    cos_sigma_spike_step(&n, 0.9f);
    /* V=1.4 → spike fires, σ=0 on the spike step. */
    if (fabsf7(cos_sigma_spike_sigma(&n) - 0.0f) > 1e-5f) return 32;
    return 0;
}

static int check_equivalence_with_digital(void) {
    /* Equivalence claim: spike fires on step i
     *   ⇔  cumulative input with decay crosses threshold.
     * We simulate a digital σ-gate with τ_accept = V_th and
     * verify the same time-step produces ACCEPT (σ < τ) digitally
     * and FIRE neuromorphically. */
    cos_spike_neuron_t n;
    cos_sigma_spike_init(&n, 1.0f, 0.95f, 0.0f);

    float inputs[6]  = { 0.1f, 0.2f, 0.3f, 0.1f, 0.2f, 0.5f };
    int   fired_at   = -1;
    int   accepted_at = -1;
    float V = 0.0f;
    for (int i = 0; i < 6; ++i) {
        /* Spike side. */
        int s = cos_sigma_spike_step(&n, inputs[i]);
        if (s == 1 && fired_at < 0) fired_at = i;
        /* Digital side: same leaky integrator, same threshold. */
        V = 0.95f * V + inputs[i];
        float sigma_digital = 1.0f - V / 1.0f;
        int accept = sigma_digital < 0.0f;     /* σ < 0 ⇒ V > V_th */
        if (accept && accepted_at < 0) accepted_at = i;
        if (V >= 1.0f) V = 0.0f;                /* mirror reset    */
    }
    if (fired_at != accepted_at) return 40;
    if (fired_at < 0)             return 41;
    return 0;
}

static int check_population_sigma(void) {
    cos_spike_neuron_t pop[8];
    for (int i = 0; i < 8; ++i)
        cos_sigma_spike_init(&pop[i], 1.0f, 0.9f, 0.0f);
    /* Step every neuron with input 0.1 for 20 steps → no spikes
     * → σ_population ≈ 1. */
    for (int t = 0; t < 20; ++t)
        for (int i = 0; i < 8; ++i)
            cos_sigma_spike_step(&pop[i], 0.1f);
    float s_silent = cos_sigma_spike_population_sigma(pop, 8, 20, 5);
    if (!(s_silent > 0.95f)) return 50;

    /* Reset + pour in so every neuron fires every step. */
    for (int i = 0; i < 8; ++i)
        cos_sigma_spike_init(&pop[i], 1.0f, 0.9f, 0.0f);
    for (int t = 0; t < 20; ++t)
        for (int i = 0; i < 8; ++i)
            cos_sigma_spike_step(&pop[i], 1.5f);      /* always spikes */
    /* Every step fires → rate = 1/step; with rate_max = 1 we get
     * σ ≈ 0.  */
    float s_saturated = cos_sigma_spike_population_sigma(pop, 8, 20, 1);
    if (!(s_saturated < 0.05f)) return 51;
    return 0;
}

int cos_sigma_spike_self_test(void) {
    int rc;
    if ((rc = check_guards()))                    return rc;
    if ((rc = check_fire()))                      return rc;
    if ((rc = check_sigma_mapping()))             return rc;
    if ((rc = check_equivalence_with_digital()))  return rc;
    if ((rc = check_population_sigma()))          return rc;
    return 0;
}
