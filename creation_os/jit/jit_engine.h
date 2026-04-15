#ifndef CREATION_OS_JIT_ENGINE_H
#define CREATION_OS_JIT_ENGINE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** MAP_JIT path: write → execute → munmap. Returns 0 on success. */
int jit_ephemeral_exec(const uint8_t *code, size_t len, uint64_t *out_ret);

/** Build leaf that returns imm in x0 then ret; returns byte length or -1. */
int jit_emit_return_imm64(uint8_t *buf, size_t cap, uint64_t imm);

#ifdef __cplusplus
}
#endif

#endif
