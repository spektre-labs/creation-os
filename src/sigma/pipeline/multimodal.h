/*
 * σ-Multimodal — modality-aware σ measurement.
 *
 * σ-gating needs a per-modality σ formula; "text confidence" is not
 * a valid proxy for "this image matches the prompt" or "this JSON
 * validates the schema".  This primitive provides canonical σ
 * measurements for the five modalities Creation OS currently emits:
 *
 *   MODAL_TEXT       — identity pass-through (σ comes from reinforce).
 *   MODAL_CODE       — cheap syntactic sanity: balance of (){}[], ""
 *                      and '' quotes.  Unbalanced → σ = 1.0
 *                      (run it, compile it, let the caller overlay
 *                      real build/test results via an external
 *                      observation hook).
 *   MODAL_STRUCTURED — JSON schema check: for each declared required
 *                      field, parse the blob and look for "<field>":
 *                      at top level (no full JSON parser — keep the
 *                      primitive small; see the Python mirror for a
 *                      full json.loads-based check).
 *   MODAL_IMAGE      — caller provides a similarity score in [0, 1]
 *                      (e.g. CLIP-score vs prompt); σ = 1 − clamp(s).
 *   MODAL_AUDIO      — same as image: σ = 1 − clamp(similarity).
 *
 * The point is not to replace CLIP / compilers / JSON parsers — the
 * point is to normalise their outputs into a single σ ∈ [0, 1] that
 * feeds the same reinforce / speculative / TTT / engram machinery.
 *
 * Contracts (v0):
 *   1. measure_text returns the input σ unchanged (identity).
 *   2. measure_code returns 1.0 on any unbalanced pair; 0.10 on
 *      syntactically balanced input (a "minor" non-zero so downstream
 *      stages can still escalate on balanced-but-nonsense output).
 *   3. measure_schema returns 1.0 on missing required field OR
 *      parse-like failure sentinels; 0.10 on all fields present.
 *      Each missing field contributes (1.0 / n_required) in a linear
 *      fashion so partial matches degrade gracefully.
 *   4. measure_image / measure_audio return clamp(1 − similarity,
 *      0, 1).  NaN similarity → 1.0.
 *   5. measure_multimodal dispatches by modality.
 *   6. self-test: 7 canonical rows + 10^3 LCG mixed-modality stress
 *      with monotonicity invariants.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_MULTIMODAL_H
#define COS_SIGMA_MULTIMODAL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    COS_MODAL_TEXT       = 0,
    COS_MODAL_CODE       = 1,
    COS_MODAL_STRUCTURED = 2,   /* JSON / tabular */
    COS_MODAL_IMAGE      = 3,
    COS_MODAL_AUDIO      = 4,
} cos_modality_t;

const char *cos_sigma_multimodal_label(cos_modality_t m);

/* Text: identity pass-through, clamped to [0, 1]; NaN → 1.0. */
float cos_sigma_measure_text(float sigma_in);

/* Code: balance check over () [] {} plus " and ' quote parity.
 * ``src_len`` may be 0 (NUL-terminated) or the explicit byte count. */
float cos_sigma_measure_code(const char *src, size_t src_len);

/* Structured (JSON-ish): confirm every ``required[i]`` appears as
 * ``"<name>":`` in ``blob``.  Each missing field adds 1.0/n_required
 * to σ (linear degradation).  ``required`` may be NULL (returns 0.1).
 */
float cos_sigma_measure_schema(const char *blob, size_t blob_len,
                               const char *const *required,
                               int n_required);

/* Image / Audio: σ = clamp(1 − similarity, 0, 1).  NaN → 1.0. */
float cos_sigma_measure_image(float similarity);
float cos_sigma_measure_audio(float similarity);

/* Dispatch by modality.  ``extra`` carries the modality-specific
 * payload:
 *   TEXT       → (float*)sigma_in
 *   CODE       → (const char*)src   (NUL-terminated)
 *   STRUCTURED → (const char*)blob  (NUL-terminated) with required[]
 *                provided separately; pass NULL required to skip.
 *   IMAGE/AUDIO → (float*)similarity
 * For STRUCTURED you need the typed entry point; the dispatcher is
 * only for text / code / image / audio. */
float cos_sigma_measure_multimodal(cos_modality_t m,
                                   const void *extra, size_t len);

int cos_sigma_multimodal_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_MULTIMODAL_H */
