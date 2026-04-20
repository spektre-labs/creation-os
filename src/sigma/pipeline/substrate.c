/* σ-Substrate — vtable unifying digital / bitnet / spike / photonic. */

#include "substrate.h"

#include "spike.h"
#include "photonic.h"

#include <math.h>
#include <string.h>

/* ---- default gate ---- */

int cos_sigma_substrate_default_gate(float sigma, float tau_accept) {
    return (sigma < tau_accept) ? 1 : 0;
}

/* ---- digital backend: 1 − max / Σ over activations ---- */

static float measure_digital(const float *a, int n, void *ctx) {
    (void)ctx;
    if (!a || n <= 0) return 1.0f;
    float total = 0.0f, max_v = 0.0f;
    for (int i = 0; i < n; ++i) {
        float v = a[i];
        if (v != v || v < 0.0f) return 1.0f;
        total += v;
        if (v > max_v) max_v = v;
    }
    if (total <= 0.0f) return 1.0f;
    float s = 1.0f - max_v / total;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    return s;
}

/* ---- BitNet backend: quantise to {−1, 0, +1} via sign+threshold,
 *      then compute σ over the quantised magnitudes.  Since the
 *      activations are non-negative we collapse to {0, 1}: any
 *      value ≥ BITNET_Q_THRESH becomes 1, else 0.  σ = 1 − max/Σ
 *      on the 0/1 vector, i.e. 1 − 1/k where k is the count of
 *      above-threshold channels. ---- */

#define BITNET_Q_THRESH 0.25f

static float measure_bitnet(const float *a, int n, void *ctx) {
    (void)ctx;
    if (!a || n <= 0) return 1.0f;
    int k = 0;
    for (int i = 0; i < n; ++i) {
        if (a[i] >= BITNET_Q_THRESH) k++;
    }
    if (k == 0) return 1.0f;
    float s = 1.0f - 1.0f / (float)k;
    return s;
}

/* ---- Spike backend: drive one LIF neuron per channel for
 *      SPIKE_WINDOW steps using activation as a constant input,
 *      then read population σ. ---- */

#define SPIKE_WINDOW      20
#define SPIKE_MAX_CHAN   256

static float measure_spike(const float *a, int n, void *ctx) {
    (void)ctx;
    if (!a || n <= 0) return 1.0f;
    if (n > SPIKE_MAX_CHAN) n = SPIKE_MAX_CHAN;
    cos_spike_neuron_t pop[SPIKE_MAX_CHAN];
    for (int i = 0; i < n; ++i) {
        cos_sigma_spike_init(&pop[i], 1.0f, 0.95f, 0.0f);
    }
    for (int t = 0; t < SPIKE_WINDOW; ++t) {
        for (int i = 0; i < n; ++i) {
            float v = a[i];
            if (v != v || v < 0.0f) v = 0.0f;
            cos_sigma_spike_step(&pop[i], v);
        }
    }
    /* Per-neuron firing rate → σ in the same shape as the other
     * backends: σ = 1 − max_rate / Σ rates.  This keeps the
     * substrate-equivalence guarantee tight: digital /
     * photonic / spike all collapse onto the same 1 − max/Σ
     * formula, just evaluated on a different raw signal
     * (activation, intensity, or spike count). */
    int max_spikes = 0, total_spikes = 0;
    for (int i = 0; i < n; ++i) {
        total_spikes += pop[i].n_spikes;
        if (pop[i].n_spikes > max_spikes) max_spikes = pop[i].n_spikes;
    }
    if (total_spikes <= 0) return 1.0f;
    float s = 1.0f - (float)max_spikes / (float)total_spikes;
    if (s < 0.0f) s = 0.0f;
    if (s > 1.0f) s = 1.0f;
    return s;
}

/* ---- Photonic backend: feed intensities through the existing
 *      intensity-ratio σ-gate. ---- */

static float measure_photonic(const float *a, int n, void *ctx) {
    (void)ctx;
    if (!a || n <= 0) return 1.0f;
    float s = 1.0f;
    cos_sigma_photonic_sigma_from_intensities(a, n, &s, NULL, NULL, NULL);
    return s;
}

/* ---- vtables ---- */

cos_sigma_substrate_t COS_SUBSTRATE_DIGITAL = {
    .name      = "digital",
    .kind      = COS_SUB_DIGITAL,
    .measure   = measure_digital,
    .gate      = cos_sigma_substrate_default_gate,
    .self_test = NULL,
    .ctx       = NULL,
};
cos_sigma_substrate_t COS_SUBSTRATE_BITNET = {
    .name      = "bitnet",
    .kind      = COS_SUB_BITNET,
    .measure   = measure_bitnet,
    .gate      = cos_sigma_substrate_default_gate,
    .self_test = NULL,
    .ctx       = NULL,
};
cos_sigma_substrate_t COS_SUBSTRATE_SPIKE = {
    .name      = "spike",
    .kind      = COS_SUB_SPIKE,
    .measure   = measure_spike,
    .gate      = cos_sigma_substrate_default_gate,
    .self_test = cos_sigma_spike_self_test,
    .ctx       = NULL,
};
cos_sigma_substrate_t COS_SUBSTRATE_PHOTONIC = {
    .name      = "photonic",
    .kind      = COS_SUB_PHOTONIC,
    .measure   = measure_photonic,
    .gate      = cos_sigma_substrate_default_gate,
    .self_test = cos_sigma_photonic_self_test,
    .ctx       = NULL,
};

cos_sigma_substrate_t *cos_sigma_substrate_lookup(cos_substrate_kind_t k) {
    switch (k) {
    case COS_SUB_DIGITAL:  return &COS_SUBSTRATE_DIGITAL;
    case COS_SUB_BITNET:   return &COS_SUBSTRATE_BITNET;
    case COS_SUB_SPIKE:    return &COS_SUBSTRATE_SPIKE;
    case COS_SUB_PHOTONIC: return &COS_SUBSTRATE_PHOTONIC;
    default:               return NULL;
    }
}

const char *cos_sigma_substrate_name(cos_substrate_kind_t k) {
    cos_sigma_substrate_t *s = cos_sigma_substrate_lookup(k);
    return s ? s->name : "unknown";
}

int cos_sigma_substrate_equivalence(const float *a, int n,
                                    float tau_accept,
                                    float out_sigma[COS_SUB__COUNT],
                                    int   out_gate [COS_SUB__COUNT])
{
    if (!a || n <= 0) return -1;
    int ref_gate = -1;
    for (int k = 0; k < COS_SUB__COUNT; ++k) {
        cos_sigma_substrate_t *s = cos_sigma_substrate_lookup((cos_substrate_kind_t)k);
        float sig = s->measure(a, n, s->ctx);
        int   g   = s->gate(sig, tau_accept);
        if (out_sigma) out_sigma[k] = sig;
        if (out_gate ) out_gate [k] = g;
        if (ref_gate < 0) ref_gate = g;
        else if (g != ref_gate) {
            /* Disagreement: return the disagreeing kind + 10 so
             * the caller can identify which backend diverged. */
            return 10 + k;
        }
    }
    return 0;
}

/* ---------- self-test ---------- */

static int check_dominant_equivalence(void) {
    /* Dominant activation: every substrate must say ACCEPT. */
    float a[4] = { 0.02f, 0.03f, 0.90f, 0.05f };
    float sig[COS_SUB__COUNT]; int  gate[COS_SUB__COUNT];
    int rc = cos_sigma_substrate_equivalence(a, 4, 0.30f, sig, gate);
    if (rc != 0) return 10;
    for (int k = 0; k < COS_SUB__COUNT; ++k) {
        if (gate[k] != 1) return 11 + k;
    }
    return 0;
}

static int check_uniform_equivalence(void) {
    /* Uniform activation: every substrate must say ABSTAIN at
     * tau_accept = 0.30 because σ ≈ 0.75. */
    float a[4] = { 0.25f, 0.25f, 0.25f, 0.25f };
    float sig[COS_SUB__COUNT]; int  gate[COS_SUB__COUNT];
    int rc = cos_sigma_substrate_equivalence(a, 4, 0.30f, sig, gate);
    if (rc != 0) return 20;
    for (int k = 0; k < COS_SUB__COUNT; ++k) {
        if (gate[k] != 0) return 21 + k;
    }
    return 0;
}

static int check_middle_equivalence(void) {
    /* Partial dominance: activations [0.1, 0.1, 0.7, 0.1]
     * σ_digital  = 1 − 0.7/1.0 = 0.30
     * σ_bitnet   = 1 − 1/1    = 0.00  (only one channel ≥ 0.25)
     * σ_spike    ≈ 0.00 under heavy drive (channel 2 fires all
     *               the time)
     * σ_photonic = 0.30
     * tau_accept = 0.50 → every backend says ACCEPT. */
    float a[4] = { 0.10f, 0.10f, 0.70f, 0.10f };
    float sig[COS_SUB__COUNT]; int  gate[COS_SUB__COUNT];
    int rc = cos_sigma_substrate_equivalence(a, 4, 0.50f, sig, gate);
    if (rc != 0) return 30;
    for (int k = 0; k < COS_SUB__COUNT; ++k) {
        if (gate[k] != 1) return 31 + k;
    }
    return 0;
}

static int check_lookup(void) {
    if (cos_sigma_substrate_lookup(COS_SUB_DIGITAL)->kind  != COS_SUB_DIGITAL)
        return 40;
    if (cos_sigma_substrate_lookup(COS_SUB__COUNT) != NULL) return 41;
    if (strcmp(cos_sigma_substrate_name(COS_SUB_SPIKE),    "spike") != 0)
        return 42;
    if (strcmp(cos_sigma_substrate_name(COS_SUB_PHOTONIC), "photonic") != 0)
        return 43;
    return 0;
}

int cos_sigma_substrate_self_test(void) {
    int rc;
    if ((rc = check_lookup()))                return rc;
    if ((rc = check_dominant_equivalence())) return rc;
    if ((rc = check_uniform_equivalence()))  return rc;
    if ((rc = check_middle_equivalence()))   return rc;
    return 0;
}
