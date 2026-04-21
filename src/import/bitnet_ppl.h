/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Optional second-pass σ from llama-perplexity (NLL / perplexity on the
 * prompt + completion text).  Enabled when CREATION_OS_BITNET_USE_PPL=1
 * and CREATION_OS_BITNET_MODEL is set.
 *
 * Returns a value in [0,1] on success, or a negative value on skip/fail
 * (caller should fall back to the text heuristic).
 */
#ifndef BITNET_PPL_H
#define BITNET_PPL_H

float cos_bitnet_sigma_from_perplexity(const char *prompt, const char *generated_text);

#endif /* BITNET_PPL_H */
