/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Minimal GGUF reader/writer (GGML spec v3 subset) for Creation OS v28.
 * See: https://github.com/ggerganov/ggml/blob/master/docs/gguf.md
 */
#ifndef COS_GGUF_H
#define COS_GGUF_H

#include <stdint.h>
#include <stdio.h>

#define COS_GGUF_MAGIC_LE 0x46554747u /* "GGUF" little-endian uint32 */

typedef enum {
    COS_GGUF_VAL_UINT8 = 0,
    COS_GGUF_VAL_INT8 = 1,
    COS_GGUF_VAL_UINT16 = 2,
    COS_GGUF_VAL_INT16 = 3,
    COS_GGUF_VAL_UINT32 = 4,
    COS_GGUF_VAL_INT32 = 5,
    COS_GGUF_VAL_FLOAT32 = 6,
    COS_GGUF_VAL_BOOL = 7,
    COS_GGUF_VAL_STRING = 8,
    COS_GGUF_VAL_ARRAY = 9,
    COS_GGUF_VAL_UINT64 = 10,
    COS_GGUF_VAL_INT64 = 11,
    COS_GGUF_VAL_FLOAT64 = 12,
} CosGgufValueType;

typedef enum {
    COS_GGML_TYPE_F32 = 0,
} CosGgmlType;

typedef struct {
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;
    uint32_t alignment; /* default 32 if absent */
} CosGgufHeaderInfo;

typedef struct {
    char name[128];
    uint32_t n_dim;
    uint64_t dims[4];
    uint32_t type;
    uint64_t offset; /* relative to tensor_data blob */
} CosGgufTensorInfo;

/** Read header + scan KV for general.architecture + general.alignment. Returns 0 on success. */
int cos_gguf_read_info(FILE *f, CosGgufHeaderInfo *out);

/** Read tensor infos (names/dims/type/offset). Caller provides array sized >= header.tensor_count. */
int cos_gguf_read_tensor_infos(FILE *f, const CosGgufHeaderInfo *hdr, CosGgufTensorInfo *tensors);

/** Write a tiny valid GGUF v3 fixture (1 KV + 1 F32 tensor) for self-tests. */
int cos_gguf_write_minimal_fixture(const char *path);

/** Byte offset of tensor data blob (aligned), after tensor info table. `f` rewound internally. */
int cos_gguf_tensor_data_base_offset(FILE *f, const CosGgufHeaderInfo *hdr, uint64_t *out_off);

/** POSIX `mmap` read of [off, off+len); Windows stub returns -1. */
int cos_gguf_mmap_read_at(const char *path, uint64_t off, void *dst, size_t len);

#endif /* COS_GGUF_H */
