/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v29: mmap-backed GGUF view (metadata + tensor bytes as offsets into the map).
 * Does not memcpy multi-GB payloads; merge-gate safe.
 */
#ifndef GGUF_LOADER_H
#define GGUF_LOADER_H

#include <stdint.h>

typedef struct {
    char name[128];
    uint32_t type;
    uint64_t n_elements;
    const void *bytes; /* points into mmap; owned by model */
} gguf_tensor_t;

typedef struct gguf_model gguf_model_t;

gguf_model_t *gguf_load(const char *path);
void gguf_free(gguf_model_t *m);

uint64_t gguf_model_tensor_count(const gguf_model_t *m);
const gguf_tensor_t *gguf_model_tensor(const gguf_model_t *m, uint64_t i);

#endif /* GGUF_LOADER_H */
