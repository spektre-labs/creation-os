/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Local σ estimate for BitNet / llama-cli captured text.
 *
 * When per-token logprobs are not yet parsed from the engine, this
 * returns a bounded heuristic, optionally overridden by:
 *   CREATION_OS_BITNET_SIGMA=<float in [0,1]>
 */
#ifndef BITNET_SIGMA_H
#define BITNET_SIGMA_H

float cos_bitnet_sigma_for_local_output(const char *text);

#endif /* BITNET_SIGMA_H */
