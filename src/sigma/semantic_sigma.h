/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *
 * Semantic σ from three answers: pairwise Jaccard similarity on normalized
 * word sets (see text_similarity.h), σ = 1 − mean(similarities). Optional
 * cluster count uses COS_SEMANTIC_SIGMA_CLUSTER_SIM (default 0.6).
 */
#ifndef COS_SEMANTIC_SIGMA_H
#define COS_SEMANTIC_SIGMA_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cos_semantic_sigma_result {
    float sigma;
    float similarities[3]; /* pairs (0,1), (0,2), (1,2) in [0,1] */
    int   n_samples;
    int   n_clusters;
} cos_semantic_sigma_result;

/** Three samples: primary + two HTTP completes at 0.1 / 1.5 (HTTP via bitnet_server). */
float cos_semantic_sigma(const char *prompt, int port, const char *model,
                         int n_samples);

/** Like cos_semantic_sigma; if primary_answer != NULL and n_samples==3, uses
 *  primary as sample 0 and two extra HTTP completes at 0.1 and 1.5 (max 60
 *  tokens, short system prompt). Jaccard uses first-sentence extracts. */
float cos_semantic_sigma_ex(const char *prompt, const char *system_prompt,
                            int port, const char *model, int n_samples,
                            const char *primary_answer,
                            cos_semantic_sigma_result *out);

/** Jaccard on exactly three UTF-8 strings (no HTTP). */
int cos_semantic_sigma_compute_texts(const char *t0, const char *t1,
                                       const char *t2,
                                       cos_semantic_sigma_result *out);

#ifdef __cplusplus
}
#endif

#endif /* COS_SEMANTIC_SIGMA_H */
