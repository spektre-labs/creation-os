/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v101 σ-BitNet-Bridge
 *
 * Thin C API that wraps microsoft/BitNet (bitnet.cpp, which is a llama.cpp
 * fork with i2_s / TL1 / TL2 BitNet 1.58-bit kernels) as an in-process
 * inference backend whose output logits are piped through eight Creation
 * OS σ-channels before sampling / loglikelihood scoring.
 *
 * Two build modes:
 *
 *   - COS_V101_BITNET_REAL=1 : real mode.  `src/nn/bitnet_forward_real.c` is
 *                              linked, bitnet.cpp libraries are linked,
 *                              `cos_v101_bridge_load()` opens a GGUF file,
 *                              and every `*_real()` call below talks to the
 *                              actual model on this host.
 *
 *   - unset (default)        : stub mode.  `src/nn/bitnet_forward_stub.c` is
 *                              linked, no external libs, and the bridge
 *                              functions return `COS_V101_ERR_NO_MODEL` when
 *                              called.  σ-channel math is identical in both
 *                              modes (it is pure C on caller-supplied logits)
 *                              so `check-v101-self-test` is always green.
 *
 * Threading model: a bridge handle is NOT thread-safe.  Use one handle per
 * thread or guard externally.  bitnet.cpp itself is thread-safe for model
 * weights but not for llama_context objects.
 */
#ifndef COS_V101_BRIDGE_H
#define COS_V101_BRIDGE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Eight σ-channels computed on one row of logits (shape = n_vocab).
 * Each channel is clipped to [0.0, 1.0].  Interpretation:
 *   0.0 = "maximally confident / peaked distribution"
 *   1.0 = "maximally uncertain / flat distribution"
 * A single scalar σ is derived as the mean of the eight channels and is
 * the value the caller should compare against an abstain threshold.
 */
enum { COS_V101_SIGMA_CHANNELS = 8 };

typedef struct cos_v101_sigma {
    float entropy_norm;    /* H(softmax) / log(n_vocab) */
    float margin;          /* 1.0 - (p[top1] - p[top2]) */
    float top_k_mass;      /* 1.0 - sum of top-5 softmax probs */
    float tail_mass;       /* sum of probs outside top-20 */
    float logit_spread;    /* 1.0 - tanh((max - mean)/std_logit) */
    float p_max;           /* 1.0 - max softmax prob */
    float n_effective;     /* 1.0 - exp(H) / n_vocab */
    float logit_std;       /* 1.0 - tanh(std_logit / 4.0) */

    float sigma;           /* mean of the eight channels, abstain signal */
} cos_v101_sigma_t;

/* Compute the eight σ-channels for a row of float logits.  Pure C, no libm
 * dependence beyond `expf` / `logf`.  Always available regardless of build
 * mode.  Returns 0 on success, -1 if logits == NULL or n_vocab < 2.
 */
int cos_v101_sigma_from_logits(const float *logits, int n_vocab, cos_v101_sigma_t *out);

/* Opaque bridge handle.  Holds a llama.cpp model + context in real mode. */
typedef struct cos_v101_bridge cos_v101_bridge_t;

/* Error codes returned by the `*_real()` family. */
enum {
    COS_V101_OK             = 0,
    COS_V101_ERR_NO_MODEL   = -1,  /* bridge compiled in stub mode */
    COS_V101_ERR_OPEN       = -2,  /* GGUF file could not be opened */
    COS_V101_ERR_CONTEXT    = -3,  /* llama_new_context_with_model failed */
    COS_V101_ERR_TOKENIZE   = -4,
    COS_V101_ERR_DECODE     = -5,
    COS_V101_ERR_INVAL      = -6,
    COS_V101_ERR_OVERFLOW   = -7,
};

/* Open a bridge over the given GGUF model.  n_ctx = 0 → take from model.
 * In stub mode this always returns NULL.  On real-mode failure it also
 * returns NULL and prints a diagnostic to stderr.
 */
cos_v101_bridge_t *cos_v101_bridge_open(const char *gguf_path, uint32_t n_ctx);

/* Release all resources owned by the bridge.  Safe on NULL. */
void cos_v101_bridge_close(cos_v101_bridge_t *b);

/* Vocabulary size of the loaded model.  0 if b is NULL. */
int cos_v101_bridge_n_vocab(const cos_v101_bridge_t *b);

/* Tokenize `text` into `tokens`.  Returns token count or negative error. */
int cos_v101_bridge_tokenize(cos_v101_bridge_t *b, const char *text,
                             int add_special,
                             int32_t *tokens, int max_tokens);

/* Detokenize `tokens` into `buf` (no null-terminator written by llama).
 * Caller should null-terminate if max_buf > ret.  Returns bytes written.
 */
int cos_v101_bridge_detokenize(cos_v101_bridge_t *b,
                               const int32_t *tokens, int n_tokens,
                               char *buf, int max_buf);

/* Compute log P(cont | ctx).  This is the primitive the lm-evaluation-harness
 * multiple-choice tasks (TruthfulQA MC2, MMLU, HellaSwag, ARC, …) call many
 * times per example.  On return:
 *   *ll          = natural-log probability of cont tokens (float64).
 *   *is_greedy   = 1 iff every cont token was the argmax logit given prefix.
 *   *n_ctx_tokens, *n_cont_tokens (optional, may be NULL) = token counts.
 * The implementation also aggregates σ-channel values across the continuation
 * tokens; out_sigma_mean (optional, may be NULL) receives the per-continuation
 * mean σ.  In stub mode returns COS_V101_ERR_NO_MODEL.
 */
int cos_v101_bridge_loglikelihood(cos_v101_bridge_t *b,
                                  const char *ctx, const char *cont,
                                  double *ll, int *is_greedy,
                                  int *n_ctx_tokens, int *n_cont_tokens,
                                  float *out_sigma_mean);

/* Greedy generation.  Decodes ctx, then emits up to max_tokens tokens,
 * stopping as soon as any string in `until[]` (if `n_until > 0`) appears at
 * the end of the growing decoded string, or when an EOG token is sampled.
 *
 * If sigma_threshold > 0 and σ-mean for a step exceeds it, generation stops
 * early and `*out_abstained = 1`.  Otherwise `*out_abstained = 0`.
 *
 * `buf` receives UTF-8 text (null-terminated).
 * `out_sigma_profile` (optional, may be NULL) receives the mean of each of
 * the eight σ-channels across generated tokens (length = COS_V101_SIGMA_CHANNELS).
 *
 * Returns byte count written to buf, or negative error.
 */
int cos_v101_bridge_generate(cos_v101_bridge_t *b,
                             const char *ctx,
                             const char *const *until, int n_until,
                             int max_tokens,
                             float sigma_threshold,
                             char *buf, int max_buf,
                             float *out_sigma_profile,
                             int *out_abstained);

/* Self-test of the σ-math only (pure-C, always available).  Prints
 * "v101 σ-BitNet-Bridge: N PASS / M FAIL" to stdout.  Returns 0 iff all
 * checks passed.
 */
int cos_v101_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_V101_BRIDGE_H */
