/* σ-Photonic — intensity-ratio σ-gate on optical channels. */

#include "photonic.h"

#include <math.h>
#include <string.h>

static float clamp01f(float x) {
    if (x != x)  return 1.0f;
    if (x < 0.0f)return 0.0f;
    if (x > 1.0f)return 1.0f;
    return x;
}

int cos_sigma_photonic_sigma_from_intensities(const float *I, int n,
                                              float *out_sigma,
                                              float *out_total,
                                              float *out_max,
                                              int   *out_argmax)
{
    if (!I || n <= 0)           return -1;
    float total = 0.0f;
    float max_i = 0.0f;
    int   argmax = 0;
    for (int k = 0; k < n; ++k) {
        float v = I[k];
        if (v != v || v < 0.0f) return -2;
        total += v;
        if (v > max_i) { max_i = v; argmax = k; }
    }
    float sigma;
    if (total <= 0.0f) {
        sigma = 1.0f;            /* no signal → maximally abstain */
    } else {
        sigma = 1.0f - (max_i / total);
        sigma = clamp01f(sigma);
    }
    if (out_sigma)          *out_sigma          = sigma;
    if (out_total)          *out_total          = total;
    if (out_max)            *out_max            = max_i;
    if (out_argmax)         *out_argmax         = argmax;
    return 0;
}

int cos_sigma_photonic_mach_zehnder_sigma(float I_in, float V,
                                          const float *phases, int n,
                                          cos_photon_channel_t *out_ch,
                                          float *out_sigma)
{
    if (!phases || !out_ch || !out_sigma) return -1;
    if (n <= 0)                            return -2;
    if (I_in < 0.0f)                       return -3;
    if (!(V >= 0.0f && V <= 1.0f))         return -4;

    float intensities[256];
    if (n > 256) n = 256;
    for (int k = 0; k < n; ++k) {
        float phi = phases[k];
        /* Single-arm MZI output:
         *     I_k = I_in · (1 + V cos Δφ_k) / 2
         * Any phase where cos≈1 peaks at I_in; cos≈−1 bottoms
         * out at I_in·(1−V)/2. */
        float I_k = I_in * (1.0f + V * cosf(phi)) * 0.5f;
        if (I_k < 0.0f) I_k = 0.0f;
        intensities[k]          = I_k;
        out_ch[k].wavelength_nm = 1550.0f;    /* telecom default   */
        out_ch[k].intensity     = I_k;
        out_ch[k].phase_rad     = phi;
    }
    return cos_sigma_photonic_sigma_from_intensities(
        intensities, n, out_sigma, NULL, NULL, NULL);
}

/* ---------- self-test ---------- */

static float fabsf8(float x) { return x < 0.0f ? -x : x; }

static int check_guards(void) {
    float out;
    if (cos_sigma_photonic_sigma_from_intensities(NULL, 2, &out, 0, 0, 0) == 0)
        return 10;
    float bad[2] = { 1.0f, -0.5f };
    if (cos_sigma_photonic_sigma_from_intensities(bad, 2, &out, 0, 0, 0) == 0)
        return 11;
    float nan_buf[2] = { 1.0f, (float)(0.0/0.0) };
    if (cos_sigma_photonic_sigma_from_intensities(nan_buf, 2, &out, 0, 0, 0) == 0)
        return 12;
    float phases[2] = { 0.0f, 0.0f };
    cos_photon_channel_t ch[2];
    if (cos_sigma_photonic_mach_zehnder_sigma(1.0f, 1.5f, phases, 2, ch, &out) == 0)
        return 13;
    return 0;
}

static int check_dominant(void) {
    /* Single dominant channel → σ ≈ 0. */
    float I[4] = { 0.0f, 0.0f, 0.9f, 0.0f };
    float s, total, max_i;
    int argmax;
    cos_sigma_photonic_sigma_from_intensities(I, 4, &s, &total, &max_i, &argmax);
    if (fabsf8(s - 0.0f) > 1e-5f) return 20;
    if (argmax != 2)              return 21;
    if (fabsf8(total - 0.9f) > 1e-5f) return 22;
    if (fabsf8(max_i - 0.9f) > 1e-5f) return 23;
    return 0;
}

static int check_uniform(void) {
    /* Perfectly uniform N-channel spread → σ = 1 − 1/N. */
    float I[4] = { 0.25f, 0.25f, 0.25f, 0.25f };
    float s;
    cos_sigma_photonic_sigma_from_intensities(I, 4, &s, 0, 0, 0);
    if (fabsf8(s - 0.75f) > 1e-5f) return 30;
    return 0;
}

static int check_all_zero(void) {
    float I[3] = { 0.0f, 0.0f, 0.0f };
    float s;
    cos_sigma_photonic_sigma_from_intensities(I, 3, &s, 0, 0, 0);
    if (fabsf8(s - 1.0f) > 1e-5f) return 40;
    return 0;
}

static int check_mach_zehnder(void) {
    /* 4-channel MZI with one arm tuned to Δφ=0 and the rest to
     * Δφ=π.  Visibility = 1.  The single constructive arm should
     * carry almost all the intensity → σ near 0. */
    float phases[4] = { 0.0f, 3.14159265f, 3.14159265f, 3.14159265f };
    cos_photon_channel_t ch[4];
    float s;
    cos_sigma_photonic_mach_zehnder_sigma(1.0f, 1.0f, phases, 4, ch, &s);
    if (!(s < 0.05f))             return 50;
    if (!(ch[0].intensity > 0.9f))return 51;

    /* Now spread equally with zero visibility (incoherent light):
     * every arm gets I_in/2 regardless of phase → σ = 1 − 1/4. */
    float flat[4] = { 0.5f, 1.2f, 2.4f, 3.6f };
    cos_sigma_photonic_mach_zehnder_sigma(1.0f, 0.0f, flat, 4, ch, &s);
    if (fabsf8(s - 0.75f) > 1e-5f) return 52;

    /* Phase sweep 0, π/2, π, 3π/2 with V=1:
     *   I = I_in·(1+cos)/2 = 1.0, 0.5, 0.0, 0.5 (sum=2.0, max=1.0)
     *   σ = 1 − 1.0/2.0 = 0.5 */
    float sweep[4] = { 0.0f, 1.5707963f, 3.1415927f, 4.7123890f };
    cos_sigma_photonic_mach_zehnder_sigma(1.0f, 1.0f, sweep, 4, ch, &s);
    if (fabsf8(s - 0.5f) > 1e-3f)  return 53;
    return 0;
}

int cos_sigma_photonic_self_test(void) {
    int rc;
    if ((rc = check_guards()))       return rc;
    if ((rc = check_dominant()))     return rc;
    if ((rc = check_uniform()))      return rc;
    if ((rc = check_all_zero()))     return rc;
    if ((rc = check_mach_zehnder())) return rc;
    return 0;
}
