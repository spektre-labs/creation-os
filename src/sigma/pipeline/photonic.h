/*
 * σ-Photonic — optical rendering of the σ-gate.
 *
 * Claim: the σ-gate depends only on the ratio noise / (signal +
 * noise), which is substrate-free.  σ-Photonic realises that
 * formula on intensities measured from a multi-channel optical
 * mesh (Mach-Zehnder interferometer array, photodetector bank,
 * in-memory photonic compute).  No transistors, no resistance,
 * no thermal noise dominated by CMOS — just intensity ratios.
 *
 * Gate formula at the photonic level (channel index k):
 *
 *     σ_photo := 1 − I_max / Σ I_k          (entropy-style)
 *     σ_photo ∈ [0, 1]
 *
 * σ ≈ 0  ⇔ one channel dominates     ⇔ ACCEPT
 * σ ≈ 1  ⇔ intensity spread          ⇔ ABSTAIN
 *
 * Mach-Zehnder interference:
 *     I_out = I_in · (1 + V cos Δφ) / 2
 *
 *     where V is the visibility (fringe contrast) and Δφ the
 *     phase difference between arms.  Constructive interference
 *     (Δφ = 0) collapses intensity onto one arm, yielding σ ≈ 0.
 *     Destructive interference (Δφ = π) spreads intensity
 *     uniformly → σ ≈ ln(N)/ln(N) = 1.
 *
 * This module is not a full FDTD simulator.  It is the σ-layer
 * that consumes either:
 *   (a) measured photodetector intensities, or
 *   (b) an idealised Mach-Zehnder mesh (for self-test and paper
 *       figures).
 *
 * Contracts (v0):
 *   1. σ_from_intensities rejects n ≤ 0, negative or NaN
 *      intensities, and the trivial all-zero input (σ ← 1).
 *   2. σ_mach_zehnder accepts visibility ∈ [0, 1], phases in
 *      radians, and builds an N-channel intensity vector whose
 *      k-th entry peaks when Δφ_k = 0.
 *   3. All σ outputs are clamped to [0, 1].
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_PHOTONIC_H
#define COS_SIGMA_PHOTONIC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float wavelength_nm;
    float intensity;       /* arbitrary units, ≥ 0          */
    float phase_rad;
} cos_photon_channel_t;

int   cos_sigma_photonic_sigma_from_intensities(
          const float *intensities, int n,
          float *out_sigma,
          float *out_total_intensity,
          float *out_max_intensity,
          int   *out_argmax);

/* Generate a Mach-Zehnder-style intensity vector for N channels
 * given a common input intensity, a visibility V and an array of
 * phase offsets (radians) — then compute σ.  When every Δφ = 0
 * one channel dominates and σ → 0; when phases are evenly
 * distributed the intensities spread out and σ → 1.            */
int   cos_sigma_photonic_mach_zehnder_sigma(
          float input_intensity,
          float visibility,
          const float *phases_rad, int n,
          cos_photon_channel_t *out_channels,
          float *out_sigma);

int   cos_sigma_photonic_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_PHOTONIC_H */
