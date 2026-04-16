/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "cos_gguf.h"

#include <string.h>

static int rd_u8(FILE *f, uint8_t *o)
{
    return fread(o, 1, 1, f) == 1 ? 0 : -1;
}

static int rd_u32(FILE *f, uint32_t *o)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4)
        return -1;
    *o = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return 0;
}

static int rd_u64(FILE *f, uint64_t *o)
{
    uint8_t b[8];
    if (fread(b, 1, 8, f) != 8)
        return -1;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++)
        v |= (uint64_t)b[i] << (8 * i);
    *o = v;
    return 0;
}

static int rd_string_into(FILE *f, char *buf, size_t cap, uint64_t *out_len)
{
    uint64_t len = 0;
    if (rd_u64(f, &len) != 0)
        return -1;
    if (out_len)
        *out_len = len;
    if (len + 1u > cap)
        return -1;
    if (len && fread(buf, 1, (size_t)len, f) != (size_t)len)
        return -1;
    buf[len] = '\0';
    return 0;
}

static int skip_string(FILE *f)
{
    uint64_t len = 0;
    if (rd_u64(f, &len) != 0)
        return -1;
    if (len) {
        if (fseek(f, (long)len, SEEK_CUR) != 0)
            return -1;
    }
    return 0;
}

static int skip_value(FILE *f, uint32_t type, int depth);

static int skip_array(FILE *f, int depth)
{
    uint32_t et = 0;
    uint64_t n = 0;
    if (rd_u32(f, &et) != 0)
        return -1;
    if (rd_u64(f, &n) != 0)
        return -1;
    for (uint64_t i = 0; i < n; i++) {
        if (skip_value(f, et, depth + 1) != 0)
            return -1;
    }
    return 0;
}

static int skip_value(FILE *f, uint32_t type, int depth)
{
    if (depth > 24)
        return -1;
    switch (type) {
    case COS_GGUF_VAL_UINT8: {
        uint8_t x;
        return rd_u8(f, &x);
    }
    case COS_GGUF_VAL_INT8: {
        int8_t x;
        return fread(&x, 1, 1, f) == 1 ? 0 : -1;
    }
    case COS_GGUF_VAL_UINT16: {
        uint8_t b[2];
        return fread(b, 1, 2, f) == 2 ? 0 : -1;
    }
    case COS_GGUF_VAL_INT16: {
        uint8_t b[2];
        return fread(b, 1, 2, f) == 2 ? 0 : -1;
    }
    case COS_GGUF_VAL_UINT32:
        return fseek(f, 4, SEEK_CUR) == 0 ? 0 : -1;
    case COS_GGUF_VAL_INT32:
        return fseek(f, 4, SEEK_CUR) == 0 ? 0 : -1;
    case COS_GGUF_VAL_FLOAT32:
        return fseek(f, 4, SEEK_CUR) == 0 ? 0 : -1;
    case COS_GGUF_VAL_BOOL: {
        uint8_t x;
        return rd_u8(f, &x);
    }
    case COS_GGUF_VAL_STRING:
        return skip_string(f);
    case COS_GGUF_VAL_ARRAY:
        return skip_array(f, depth);
    case COS_GGUF_VAL_UINT64:
        return fseek(f, 8, SEEK_CUR) == 0 ? 0 : -1;
    case COS_GGUF_VAL_INT64:
        return fseek(f, 8, SEEK_CUR) == 0 ? 0 : -1;
    case COS_GGUF_VAL_FLOAT64:
        return fseek(f, 8, SEEK_CUR) == 0 ? 0 : -1;
    default:
        return -1;
    }
}

static uint64_t align_u64(uint64_t x, uint32_t align)
{
    if (align == 0u)
        align = 32u;
    const uint64_t m = (uint64_t)align - 1u;
    return (x + m) & ~m;
}

int cos_gguf_read_info(FILE *f, CosGgufHeaderInfo *out)
{
    if (!f || !out)
        return -1;
    rewind(f);
    uint32_t magic = 0;
    if (rd_u32(f, &magic) != 0)
        return -1;
    if (magic != COS_GGUF_MAGIC_LE)
        return -1;
    memset(out, 0, sizeof(*out));
    if (rd_u32(f, &out->version) != 0)
        return -1;
    if (rd_u64(f, &out->tensor_count) != 0)
        return -1;
    if (rd_u64(f, &out->kv_count) != 0)
        return -1;
    out->alignment = 32u;
    for (uint64_t i = 0; i < out->kv_count; i++) {
        char k[256];
        if (rd_string_into(f, k, sizeof(k), NULL) != 0)
            return -1;
        uint32_t vt = 0;
        if (rd_u32(f, &vt) != 0)
            return -1;
        if (!strcmp(k, "general.alignment") && vt == COS_GGUF_VAL_UINT32) {
            uint32_t al = 0;
            if (rd_u32(f, &al) != 0)
                return -1;
            if (al >= 8u && (al % 8u) == 0u)
                out->alignment = al;
        } else {
            if (skip_value(f, vt, 0) != 0)
                return -1;
        }
    }
    return 0;
}

int cos_gguf_read_tensor_infos(FILE *f, const CosGgufHeaderInfo *hdr, CosGgufTensorInfo *tensors)
{
    if (!f || !hdr || !tensors)
        return -1;
    (void)hdr;
    rewind(f);
    uint32_t magic = 0;
    if (rd_u32(f, &magic) != 0 || magic != COS_GGUF_MAGIC_LE)
        return -1;
    uint32_t ver = 0;
    uint64_t tc = 0, kc = 0;
    if (rd_u32(f, &ver) != 0)
        return -1;
    if (rd_u64(f, &tc) != 0)
        return -1;
    if (rd_u64(f, &kc) != 0)
        return -1;
    for (uint64_t i = 0; i < kc; i++) {
        if (skip_string(f) != 0)
            return -1;
        uint32_t vt = 0;
        if (rd_u32(f, &vt) != 0)
            return -1;
        if (skip_value(f, vt, 0) != 0)
            return -1;
    }
    for (uint64_t i = 0; i < tc; i++) {
        CosGgufTensorInfo *t = &tensors[i];
        memset(t, 0, sizeof(*t));
        uint64_t nlen = 0;
        if (rd_u64(f, &nlen) != 0)
            return -1;
        if (nlen >= sizeof(t->name))
            return -1;
        if (nlen && fread(t->name, 1, (size_t)nlen, f) != (size_t)nlen)
            return -1;
        t->name[nlen] = '\0';
        if (rd_u32(f, &t->n_dim) != 0)
            return -1;
        if (t->n_dim > 4u)
            return -1;
        for (uint32_t d = 0; d < t->n_dim; d++) {
            if (rd_u64(f, &t->dims[d]) != 0)
                return -1;
        }
        if (rd_u32(f, &t->type) != 0)
            return -1;
        if (rd_u64(f, &t->offset) != 0)
            return -1;
    }
    return 0;
}

int cos_gguf_tensor_data_base_offset(FILE *f, const CosGgufHeaderInfo *hdr, uint64_t *out_off)
{
    if (!f || !hdr || !out_off)
        return -1;
    rewind(f);
    uint32_t magic = 0;
    if (rd_u32(f, &magic) != 0 || magic != COS_GGUF_MAGIC_LE)
        return -1;
    uint32_t ver = 0;
    uint64_t tc = 0, kc = 0;
    if (rd_u32(f, &ver) != 0)
        return -1;
    if (rd_u64(f, &tc) != 0)
        return -1;
    if (rd_u64(f, &kc) != 0)
        return -1;
    for (uint64_t i = 0; i < kc; i++) {
        if (skip_string(f) != 0)
            return -1;
        uint32_t vt = 0;
        if (rd_u32(f, &vt) != 0)
            return -1;
        if (skip_value(f, vt, 0) != 0)
            return -1;
    }
    for (uint64_t i = 0; i < tc; i++) {
        if (skip_string(f) != 0)
            return -1;
        uint32_t nd = 0;
        if (rd_u32(f, &nd) != 0)
            return -1;
        if (nd > 4u)
            return -1;
        for (uint32_t d = 0; d < nd; d++) {
            uint64_t dim = 0;
            if (rd_u64(f, &dim) != 0)
                return -1;
        }
        uint32_t typ = 0;
        if (rd_u32(f, &typ) != 0)
            return -1;
        uint64_t off = 0;
        if (rd_u64(f, &off) != 0)
            return -1;
    }
    long pos = ftell(f);
    if (pos < 0)
        return -1;
    *out_off = align_u64((uint64_t)pos, hdr->alignment);
    return 0;
}

static int wr_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = {(uint8_t)(v & 0xffu), (uint8_t)((v >> 8) & 0xffu), (uint8_t)((v >> 16) & 0xffu), (uint8_t)((v >> 24) & 0xffu)};
    return fwrite(b, 1, 4, f) == 4 ? 0 : -1;
}

static int wr_u64(FILE *f, uint64_t v)
{
    uint8_t b[8];
    for (int i = 0; i < 8; i++)
        b[i] = (uint8_t)((v >> (8 * i)) & 0xffu);
    return fwrite(b, 1, 8, f) == 8 ? 0 : -1;
}

static int wr_string(FILE *f, const char *s)
{
    uint64_t len = (uint64_t)strlen(s);
    if (wr_u64(f, len) != 0)
        return -1;
    if (len && fwrite(s, 1, (size_t)len, f) != (size_t)len)
        return -1;
    return 0;
}

int cos_gguf_write_minimal_fixture(const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    const uint32_t magic = COS_GGUF_MAGIC_LE;
    if (wr_u32(f, magic) != 0)
        goto bad;
    const uint32_t version = 3u;
    if (wr_u32(f, version) != 0)
        goto bad;
    const uint64_t tensor_count = 1ull;
    const uint64_t kv_count = 2ull;
    if (wr_u64(f, tensor_count) != 0)
        goto bad;
    if (wr_u64(f, kv_count) != 0)
        goto bad;

    /* KV: general.architecture */
    if (wr_string(f, "general.architecture") != 0)
        goto bad;
    if (wr_u32(f, COS_GGUF_VAL_STRING) != 0)
        goto bad;
    if (wr_string(f, "llama") != 0)
        goto bad;

    /* KV: general.alignment */
    if (wr_string(f, "general.alignment") != 0)
        goto bad;
    if (wr_u32(f, COS_GGUF_VAL_UINT32) != 0)
        goto bad;
    if (wr_u32(f, 32u) != 0)
        goto bad;

    /* Tensor info */
    if (wr_string(f, "t.weight") != 0)
        goto bad;
    const uint32_t ndim = 1u;
    if (wr_u32(f, ndim) != 0)
        goto bad;
    const uint64_t dim0 = 2ull;
    if (wr_u64(f, dim0) != 0)
        goto bad;
    const uint32_t typ = (uint32_t)COS_GGML_TYPE_F32;
    if (wr_u32(f, typ) != 0)
        goto bad;
    const uint64_t toff = 0ull;
    if (wr_u64(f, toff) != 0)
        goto bad;

    const long pos = ftell(f);
    if (pos < 0)
        goto bad;
    const uint64_t apos = align_u64((uint64_t)pos, 32u);
    for (uint64_t p = (uint64_t)pos; p < apos; p++) {
        uint8_t z = 0;
        if (fwrite(&z, 1, 1, f) != 1u)
            goto bad;
    }

    float w[2] = {1.f, 2.f};
    if (fwrite(w, sizeof(float), 2, f) != 2u)
        goto bad;

    if (fclose(f) != 0)
        return -1;
    return 0;
bad:
    fclose(f);
    return -1;
}
