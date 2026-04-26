/* Optional in-process BitNet path (scaffold).
 *
 * Microsoft BitNet (third_party/bitnet) builds as C++/CMake.  There is
 * no stable C ABI named like the pseudocode in older notes — linking it
 * requires a thin adapter in this tree or upstream.  Until that adapter
 * exists, `cos_bitnet_native_try_complete` always falls through so HTTP
 * llama-server / Ollama paths keep working unchanged.
 *
 * Build with:  make cos-chat BITNET_NATIVE=1
 * Runtime opt: COS_BITNET_NATIVE=1  (prints one hint, then falls back)
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_BITNET_NATIVE_H
#define COS_BITNET_NATIVE_H

#include "bitnet_server.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Map top-1 / top-2 integer logits to a bounded [0,1] margin proxy (Q16.16). */
int cos_bitnet_native_top2_gap_fraction(int32_t top1, int32_t top2,
                                        float *out_sigma01);

/**
 * Try in-process BitNet completion before HTTP.
 *
 * @return 0 on success (out filled),
 *         -1 if native path inactive or unavailable (caller continues),
 *         >0 on hard failure (caller may still fall back depending on policy).
 */
int cos_bitnet_native_try_complete(const char *prompt,
                                   const char *system_prompt, int round,
                                   cos_bitnet_server_result_t *out);

int cos_bitnet_native_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_BITNET_NATIVE_H */
