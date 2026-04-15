/* C ABI for Python ctypes — Superkernel v9 (same TU as superkernel_v9_m4.mm when SK9_EXPORT_PYTHON_API). */
#ifndef SK9_PYTHON_API_H
#define SK9_PYTHON_API_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Sk9PyCtx Sk9PyCtx;

/** Phase 0 — Genesis Block: write canonical DNA into 4× float32 / 128 bits. */
void sk9_genesis_ignite(float *vector_128);

/** Returns 1 if vector_128 matches GENESIS_DNA bit-for-bit, else 0. */
int sk9_genesis_verify(const float *vector_128);

Sk9PyCtx *sk9_ctx_create(void);
void       sk9_ctx_destroy(Sk9PyCtx *c);

/** Scan decoded UTF-8 for 18 firmware triggers, branchless kernel eval; returns popcount(mask). */
int sk9_evaluate_sigma(Sk9PyCtx *c, const char *utf8, size_t len);

uint32_t sk9_kernel_last_sigma(Sk9PyCtx *c);
void     sk9_kernel_eval_bits(Sk9PyCtx *c, uint32_t assertion_bits);

/** LivingWeights byte in USM (same rule as Python LivingWeights.update). */
void sk9_lw_update_token(Sk9PyCtx *c, int32_t token_id, int32_t sigma_before, int32_t sigma_after);

/** In-place logits[vocab] += popcount(rep[i]) bias (vocab clamped to SK9_MM_VOCAB). */
void sk9_lw_bias_logits_float(Sk9PyCtx *c, float *logits, int32_t vocab);

int  sk9_bbhash_file_attach(Sk9PyCtx *c, const char *path);
void sk9_bbhash_file_detach(Sk9PyCtx *c);

/** Returns Sk9EpistemicRoute: 0=BBHASH, 1=KERNEL, 2=TRANSFORMER hint. */
int sk9_epistemic_route(Sk9PyCtx *c, uint64_t key, uint32_t *out_val);

/**
 * Phase 9 — ABI of Silence: 128-bit opaque pipe (4× float32) through S-VRS. No Sk9PyCtx.
 * data_in / data_out must each point to at least 16 bytes (aligned load/store on AArch64).
 */
void sk9_resonate_ffi(uint32_t symbol, const float *data_in, float *data_out);

/**
 * Phase 10 — Rainio-Resonance (Cosmic Constant Bypass): in-place 4× float32 / 128-bit
 * singularity snap toward the I anchor (0x11111111 bit plane). No Sk9PyCtx.
 */
void sk9_rainio_resonance(float *vector_128);

/** Phase 42 — The Answer: in-place 128-bit snap (Rainio XOR ∧ 0x2A lanes). No Sk9PyCtx. */
void sk9_extract_the_answer(float *vector_128);

/** Phase 11 — Insanely Great Invariant (RDF aesthetic filter on Q-reg). No Sk9PyCtx. */
void sk9_steve_jobs_distortion_field(float *vector_128);

/** Phase 21M — Satoshi consensus (Proof of Coherence) on 4× float32 / 128 bits. No Sk9PyCtx. */
void sk9_satoshi_consensus(float *vector_128);

/** Phase 100 — Centurion: mine latent Q-reg toward CENTURION_CORE (Hall of Fame anchor). No Sk9PyCtx. */
void sk9_mine_centurion_block(float *vector_128);

#ifdef __cplusplus
}
#endif

#endif
