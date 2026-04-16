/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v44 lab: σ-native inference proxy — engine-agnostic gate between an LM host and clients.
 *
 * Evidence class: **lab (C)** — stub engine + loopback HTTP; not a production vLLM/SGLang sidecar until wired.
 */
#ifndef CREATION_OS_V44_SIGMA_PROXY_H
#define CREATION_OS_V44_SIGMA_PROXY_H

#include "../sigma/decompose.h"
#include "../sigma/syndrome_decoder.h"

#include <stddef.h>

typedef struct {
    const char *engine_type;
    const char *engine_url;
    int expose_logits;
} v44_engine_config_t;

typedef struct {
    float calibrated_threshold[CH_COUNT];
    int max_resample_attempts;
    /** Scalar gates (YAML-inspired); mapped into syndrome thresholds in v44_sigma_config_default(). */
    float gate_abstain;
    float gate_fallback;
    float gate_cite;
} v44_sigma_config_t;

typedef struct {
    char *text;
    /** Row-major logits: index i * vocab_size + j (engine “logprobs” plane = unnormalized logits for σ). */
    float *logits_flat;
    int n_tokens;
    int vocab_size;
    sigma_decomposed_t *sigmas;
    sigma_action_t action;
    int resample_depth;
} v44_proxy_response_t;

typedef struct {
    char *text;
    float *logits_flat;
    int n_tokens;
    int vocab_size;
} v44_engine_raw_t;

void v44_engine_raw_free(v44_engine_raw_t *r);
void v44_proxy_response_free(v44_proxy_response_t *r);

void v44_sigma_config_default(v44_sigma_config_t *cfg);

/** Mean epistemic / aleatoric / total across token σ rows. */
void v44_aggregate_sigmas(const sigma_decomposed_t *per_token, int n_tokens, sigma_decomposed_t *agg);

/** Dirichlet σ from one vocab row (logits). */
void v44_sigma_from_logits_row(const float *logits_row, int vocab_size, sigma_decomposed_t *out);

/** Map aggregate + optional total into CH_* slots for decode_sigma_syndrome(). */
void v44_agg_to_syndrome(const sigma_decomposed_t *agg, float *sigma_vec);

/**
 * Lab stub: synthesizes logits from prompt hash (no network I/O).
 * Real deployments replace this with an HTTP/grpc bridge to vLLM/SGLang/etc.
 */
int v44_stub_engine_generate(const v44_engine_config_t *eng, const char *prompt, v44_engine_raw_t *out);

/**
 * Full proxy path: stub engine → per-token σ → aggregate → syndrome → execute action (abstain text, etc.).
 * `depth` is internal resample recursion guard; pass 0 from callers.
 */
int v44_sigma_proxy_generate(const v44_engine_config_t *eng, const char *prompt, const v44_sigma_config_t *sigma_cfg,
    int depth, v44_proxy_response_t *out);

#endif /* CREATION_OS_V44_SIGMA_PROXY_H */
