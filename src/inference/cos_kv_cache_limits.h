/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * Caps for σ-KV helpers (caller supplies backing storage; these bound
 * self-tests and CLI demos only).
 */
#ifndef COS_KV_CACHE_LIMITS_H
#define COS_KV_CACHE_LIMITS_H

#define COS_KV_SELFTEST_MAX_LEN    256
#define COS_KV_SELFTEST_LAYERS     4
#define COS_KV_SELFTEST_HEAD_DIM   32
#define COS_SEM_CACHE_ENTRIES      128
#define COS_SEM_BSC_BITS           4096
#define COS_SEM_BSC_WORDS          (COS_SEM_BSC_BITS / 64)

#endif /* COS_KV_CACHE_LIMITS_H */
