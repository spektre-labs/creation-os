/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Fixed caps for libc-only inference (no heap in the hot path).
 *
 * Full GGUF models may exceed these; loader returns an error unless
 * dimensions fit.
 */
#ifndef COS_INFERENCE_LIMITS_H
#define COS_INFERENCE_LIMITS_H

#define COS_INF_MAX_VOCAB     512
#define COS_INF_MAX_HIDDEN    64
#define COS_INF_MAX_HEAD_DIM  64
#define COS_INF_MAX_SEQ       128
#define COS_INF_MAX_LAYERS    4
#define COS_INF_MAX_SCORES    COS_INF_MAX_SEQ

#endif /* COS_INFERENCE_LIMITS_H */
