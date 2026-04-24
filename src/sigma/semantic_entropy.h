/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Semantic-entropy σ: sample the same prompt several times at different
 * temperatures, cluster answers by pairwise similarity, and map the
 * number of meaning clusters to uncertainty (no per-token logprobs).
 *
 * See also: COS_SEMANTIC_ENTROPY_METRIC (jaccard vs bsc),
 *           COS_SEMANTIC_ENTROPY_SIM (merge threshold, default 0.6).
 */
#ifndef COS_SEMANTIC_ENTROPY_H
#define COS_SEMANTIC_ENTROPY_H

#ifdef __cplusplus
extern "C" {
#endif

/** Jaccard similarity of whitespace-separated word sets (ASCII case fold). */
float cos_text_similarity_jaccard(const char *a, const char *b);

/** σ_semantic = 1 − 1 / n_clusters (connected components when sim > τ). */
float cos_semantic_entropy(const char *prompt, const char *system_prompt,
                           int port, const char *model, int n_samples);

/** Same as cos_semantic_entropy; when out_n_clusters non-NULL, writes cluster count. */
float cos_semantic_entropy_ex(const char *prompt, const char *system_prompt,
                              int port, const char *model, int n_samples,
                              int *out_n_clusters);

#ifdef __cplusplus
}
#endif

#endif /* COS_SEMANTIC_ENTROPY_H */
