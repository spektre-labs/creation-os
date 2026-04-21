/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Local σ estimate for BitNet / llama-cli captured text.
 *
 * History: this used to host a second-pass `llama-perplexity` helper
 * (bitnet_ppl.c) that mapped NLL → σ.  DEV-1 made that obsolete by
 * pulling real per-token logprobs from llama-server's /v1/chat/
 * completions + /completion endpoints (src/import/bitnet_server.c).
 * The perplexity subprocess has been purged; what remains here is a
 * tiny text-shape heuristic retained for two narrow use sites:
 *
 *   1. stub_gen.c's legacy `CREATION_OS_BITNET_EXE` branch — a
 *      one-shot llama-cli capture with no logprob stream available.
 *   2. stub_gen.c's deterministic "2+2 → 4" fast path that keeps a
 *      handful of check-*.sh harnesses pinned without spinning up
 *      an inference server.
 *
 * All other callers should go through bitnet_server.h and get a real,
 * log-probability-derived σ.  The environment override
 *   CREATION_OS_BITNET_SIGMA=<float in [0,1]>
 * remains useful for CI pinning.
 */
#ifndef BITNET_SIGMA_H
#define BITNET_SIGMA_H

float cos_bitnet_sigma_for_local_output(const char *text);

float cos_bitnet_sigma_for_prompt_and_output(const char *prompt, const char *text);

#endif /* BITNET_SIGMA_H */
