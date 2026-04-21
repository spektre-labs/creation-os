/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Local σ estimate for BitNet / llama-cli captured text.
 *
 * When per-token logprobs are not yet parsed from the engine, this
 * returns a bounded heuristic, optionally overridden by:
 *   CREATION_OS_BITNET_SIGMA=<float in [0,1]>
 *
 * When CREATION_OS_BITNET_USE_PPL=1 and CREATION_OS_BITNET_MODEL are set,
 * cos_bitnet_sigma_for_prompt_and_output() may run llama-perplexity on the
 * prompt + completion (slow second pass) and map perplexity to σ; otherwise
 * it falls back to the same heuristic as cos_bitnet_sigma_for_local_output().
 */
#ifndef BITNET_SIGMA_H
#define BITNET_SIGMA_H

float cos_bitnet_sigma_for_local_output(const char *text);

float cos_bitnet_sigma_for_prompt_and_output(const char *prompt, const char *text);

#endif /* BITNET_SIGMA_H */
