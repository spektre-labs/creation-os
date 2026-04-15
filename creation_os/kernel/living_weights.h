#ifndef CREATION_OS_KERNEL_LIVING_WEIGHTS_H
#define CREATION_OS_KERNEL_LIVING_WEIGHTS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void lw_soc_init(int vocab_cap);
uint8_t lw_soc_reputation(uint32_t token_id);
void lw_soc_touch(uint32_t token_id, int coherent);

#ifdef __cplusplus
}
#endif

#endif
