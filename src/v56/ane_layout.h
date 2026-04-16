/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v56 ANE layout — matmul → 1×1 convolution layout helper for
 * Apple Neural Engine dispatch.
 *
 * Framing (2026 reverse-engineering of Apple's Neural Engine,
 * "80× More Efficient Than A100?" analysis and m0at/ANEtransformers
 * + jmanhype/ane-lora-training projects): the ANE MIL `matmul`
 * operation **silently does not execute on the ANE** despite
 * compiling successfully.  Every matmul must be rewritten as a
 * 1×1 convolution:
 *
 *     A[M, K] @ B[K, N]
 *   ⟶ conv(input=[1, K, 1, N], weight=[M, K, 1, 1])
 *
 *   ⇒ ~3× higher throughput on M-series devices.
 *
 * Additional ANE constraint: **spatial dims must be ≥ 16 AND a
 * multiple of 16**.  Violations compile but execute on CPU or
 * silently fail.
 *
 * Creation OS treats this as a **layout-validation** concern:
 * decide at scheduling time whether a given GEMV/GEMM is ANE-ready,
 * report the exact shapes and required padding, and return a
 * fallback reason string the caller can log.  No Core ML, no
 * tensors, no dispatch — pure C11 integer math.
 *
 * This is the M4-first discipline applied honestly: we do not
 * claim ANE integration; we ship the layout math that any future
 * Core ML / private-API binding will need first.  Scaffold tier C.
 */
#ifndef CREATION_OS_V56_ANE_LAYOUT_H
#define CREATION_OS_V56_ANE_LAYOUT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define V56_ANE_SPATIAL_MULT 16

/* Pure description of a GEMV/GEMM: Y[M, N] = A[M, K] @ X[K, N]. */
typedef struct {
    int M;
    int K;
    int N;
} v56_gemm_t;

typedef struct {
    /* NCHW Core ML convention after matmul-to-conv rewrite.
     *   input_shape  = [N_batch=1, C_in=K, H=1, W=N]
     *   weight_shape = [C_out=M, C_in=K, H=1, W=1]
     * We keep both shapes explicit so callers can copy them into
     * Core ML / MIL ops without re-deriving the rewrite. */
    int input_shape[4];
    int weight_shape[4];

    /* ANE compatibility.  spatial_h/spatial_w are the H, W of the
     * INPUT tensor; these are the dims that must be ≥ 16 and a
     * multiple of 16. */
    int spatial_h;
    int spatial_w;
    int ane_compatible;   /* 1 iff both spatial dims ≥ 16 and % 16 == 0 */

    /* Padding plan (if not compatible): how many units to add so
     * each dim becomes the next multiple of 16 (and ≥ 16). */
    int pad_h;
    int pad_w;
    int pad_bytes_fp32;   /* (pad_h * new_w + pad_w * new_h - pad_h * pad_w) * K * 4 */

    /* Short, caller-loggable reason string.  Static storage. */
    const char *reason;
} v56_ane_layout_t;

/* Compute the conv-as-matmul layout.  Deterministic, O(1), no alloc. */
void v56_ane_layout_from_gemm(const v56_gemm_t *in, v56_ane_layout_t *out);

/* Return 1 iff (spatial_h, spatial_w) ∈ ≥16 ∧ multiples of 16. */
int v56_ane_layout_validate(const v56_ane_layout_t *l);

/* Suggest the next-larger dim that satisfies the ANE constraint. */
int v56_ane_round_up_spatial(int d);

#ifdef __cplusplus
}
#endif

#endif /* CREATION_OS_V56_ANE_LAYOUT_H */
