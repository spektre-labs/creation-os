/* CREATION OS — Living weights (σ-guided token reputation). CC BY 4.0 */
#ifndef LIVING_WEIGHTS_H
#define LIVING_WEIGHTS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Matches llama_token_data layout for { id, logit } (third field p ignored). */
typedef struct {
    int32_t id;
    float   logit;
} lw_token_row_t;

void living_weights_init(int32_t n_vocab, const char * save_path);
void living_weights_teardown(void);

/** Adjust candidate logits using 8-bit shift-register reputation (O(n_rows)). */
void living_weights_apply_rows(lw_token_row_t * rows, size_t n_rows);

/**
 * After a token is committed: σ dropped or flat → coherent bit 1; σ rose → 0.
 * Also triggers periodic save / decay per integration guide.
 */
void living_weights_on_token_committed(int32_t token_id, int sigma_before, int sigma_after);

#ifdef __cplusplus
}
#endif

#endif /* LIVING_WEIGHTS_H */
