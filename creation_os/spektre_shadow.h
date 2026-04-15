/* Shadow ledger: count hard scorches per token id (STEP 8 stub). */
#ifndef SPEKTRE_SHADOW_H
#define SPEKTRE_SHADOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void spektre_shadow_init(int32_t n_vocab);
void spektre_shadow_teardown(void);
void spektre_shadow_note_scorch(int32_t token_id);
/** Valid until teardown; may return NULL if shadow inactive. */
const uint32_t * spektre_shadow_get_counts(int32_t * out_n_vocab);

#ifdef __cplusplus
}
#endif

#endif
