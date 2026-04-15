#ifndef CREATION_OS_SHADOW_LEDGER_H
#define CREATION_OS_SHADOW_LEDGER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void shadow_ledger_init(void);
void shadow_ledger_mark(uint32_t token_id, uint32_t flags);
uint32_t shadow_ledger_get(uint32_t token_id);

#ifdef __cplusplus
}
#endif

#endif
