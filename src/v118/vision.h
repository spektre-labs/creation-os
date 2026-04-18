/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v118 σ-Vision — multimodal input layer.
 *
 * v118 adds image acceptance to Creation OS.  The full architecture
 * is:
 *
 *   1. SigLIP vision encoder (400M, GGUF) produces a 768-dim image
 *      embedding from an RGB tensor.
 *   2. A fixed projection matrix P ∈ R^{768×2048} maps that into the
 *      BitNet 2B token-embedding space.  P is learned offline from
 *      200 COCO caption pairs (representation surgery, v104-style;
 *      no online training).
 *   3. The projected vector is spliced in front of the user's text
 *      prompt as a single "image token" pseudo-embedding.
 *   4. σ is measured on the projection step itself: well-aligned
 *      images produce low σ, out-of-distribution images (line art,
 *      abstract drawings, extreme crops) produce high σ and we
 *      abstain with "Model uncertain about image interpretation".
 *
 * v118.0 (this kernel) ships the *interface surface* in pure C:
 *
 *   - request parsing: OpenAI-compatible `image_url` field
 *     accepting data: URLs (base64) or http(s): URLs (host-side
 *     fetch not performed here — caller passes bytes);
 *   - a deterministic placeholder encoder + projection that produces
 *     a 2048-dim pseudo-embedding from image bytes (SHA-adjacent
 *     hash + sinusoidal fill);
 *   - a σ estimator on the projection: entropy of the projection
 *     bucket histogram (higher σ = flatter histogram = less
 *     structured image);
 *   - the response contract: `abstained`, `sigma_product`,
 *     `embedding_dim`, `projection_channel` used elsewhere.
 *
 * The real SigLIP + learned P matrix drop in at v118.1 behind the
 * same API — merge-gate stays green on every platform because no
 * weights are required.
 */
#ifndef COS_V118_VISION_H
#define COS_V118_VISION_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V118_EMBED_DIM      2048       /* BitNet 2B hidden size   */
#define COS_V118_URL_MAX        4096

typedef enum {
    COS_V118_SRC_UNKNOWN = 0,
    COS_V118_SRC_DATA_URL,
    COS_V118_SRC_HTTP_URL,
    COS_V118_SRC_RAW_BYTES,
} cos_v118_source_t;

typedef struct cos_v118_image {
    cos_v118_source_t source;
    const unsigned char *bytes;    /* decoded / raw image bytes */
    size_t               n_bytes;
    char                 url[COS_V118_URL_MAX];  /* original URL or "" */
} cos_v118_image_t;

typedef struct cos_v118_projection_result {
    float   sigma_product;         /* σ on the projection step */
    int     abstained;             /* 1 if σ > tau_vision */
    float   tau_vision;
    int     embedding_dim;         /* COS_V118_EMBED_DIM */
    /* First 8 embedding values (for debug/JSON; full buffer is
     * consumed by the v101 bridge and not copied here). */
    float   preview[8];
    char    projection_channel[32]; /* which σ channel triggered abstain */
    /* Deterministic fingerprint of the image bytes so downstream
     * components can correlate across requests. */
    uint32_t content_hash;
} cos_v118_projection_result_t;

/* Parse an image descriptor out of an OpenAI chat-completions message:
 *   {"type":"image_url","image_url":{"url":"data:image/png;base64,…"}}
 *
 * Extracts the URL; if it is a `data:` URL, base64-decodes the
 * payload into a caller-owned buffer.  `buf`/`buf_cap` are used for
 * the decoded bytes; on http(s) URLs we do NOT fetch — `bytes`/`n`
 * are set from `buf` only if the caller later populates them (so the
 * v106 server can do the HTTP fetch itself).
 *
 * Returns 0 on success, -1 on malformed input. */
int cos_v118_parse_image_url(const char *body, size_t body_len,
                             cos_v118_image_t *img,
                             unsigned char *buf, size_t buf_cap,
                             size_t *n_decoded);

/* Run the placeholder encoder + projection + σ estimator.
 *
 * `embedding_out` is an optional pre-allocated buffer of
 * COS_V118_EMBED_DIM floats; may be NULL if the caller only wants
 * the result metadata.
 *
 * Returns 0 on success. */
int cos_v118_project(const cos_v118_image_t *img,
                     float tau_vision,
                     cos_v118_projection_result_t *out,
                     float *embedding_out);

/* Serialize the result into JSON suitable for embedding into an
 * OpenAI-compatible chat-completions response's creation_os.vision
 * block.  Returns bytes written or -1 on overflow. */
int cos_v118_result_to_json(const cos_v118_projection_result_t *r,
                            char *out, size_t cap);

/* Pure-C self-test.  Verifies:
 *   - data: URL round-trip on a tiny known image
 *   - projection returns finite values + sane σ
 *   - out-of-distribution input (uniform bytes) triggers abstention
 *   - JSON shape carries all required fields
 * Returns 0 on pass. */
int cos_v118_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V118_VISION_H */
