/* SPDX-License-Identifier: AGPL-3.0-or-later */
#ifndef CREATION_OS_V41_BEST_OF_N_H
#define CREATION_OS_V41_BEST_OF_N_H

/** σ-adaptive sample count from quick epistemic read (lab thresholds; tunable by host). */
int v41_compute_n_samples(float epistemic);

/** Index of minimum float in a[0..n-1]; returns 0 if n<=0. */
int v41_argmin_float(const float *a, int n);

#endif /* CREATION_OS_V41_BEST_OF_N_H */
