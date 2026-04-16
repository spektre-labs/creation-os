/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gguf_loader.h"

#include "cos_gguf.h"

#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

struct gguf_model {
#if !defined(_WIN32)
    void *map_base;
    size_t map_size;
#endif
    uint64_t tensor_base;
    uint64_t tensor_count;
    gguf_tensor_t *tensors;
};

static uint64_t tensor_nelem(const CosGgufTensorInfo *t)
{
    uint64_t n = 1ull;
    for (uint32_t i = 0; i < t->n_dim; i++)
        n *= t->dims[i];
    return n;
}

gguf_model_t *gguf_load(const char *path)
{
    if (!path || !path[0])
        return NULL;
#if defined(_WIN32)
    (void)path;
    return NULL;
#else
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    CosGgufHeaderInfo hdr;
    memset(&hdr, 0, sizeof(hdr));
    if (cos_gguf_read_info(f, &hdr) != 0) {
        fclose(f);
        return NULL;
    }
    CosGgufTensorInfo *ti = (CosGgufTensorInfo *)calloc((size_t)hdr.tensor_count, sizeof(CosGgufTensorInfo));
    if (!ti) {
        fclose(f);
        return NULL;
    }
    if (cos_gguf_read_tensor_infos(f, &hdr, ti) != 0) {
        free(ti);
        fclose(f);
        return NULL;
    }
    uint64_t base = 0;
    if (cos_gguf_tensor_data_base_offset(f, &hdr, &base) != 0) {
        free(ti);
        fclose(f);
        return NULL;
    }
    fclose(f);

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        free(ti);
        return NULL;
    }
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode)) {
        close(fd);
        free(ti);
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        free(ti);
        return NULL;
    }

    gguf_model_t *m = (gguf_model_t *)calloc(1, sizeof(gguf_model_t));
    if (!m) {
        munmap(p, sz);
        free(ti);
        return NULL;
    }
    m->map_base = p;
    m->map_size = sz;
    m->tensor_base = base;
    m->tensor_count = hdr.tensor_count;
    m->tensors = (gguf_tensor_t *)calloc((size_t)hdr.tensor_count, sizeof(gguf_tensor_t));
    if (!m->tensors) {
        munmap(p, sz);
        free(ti);
        free(m);
        return NULL;
    }
    for (uint64_t i = 0; i < hdr.tensor_count; i++) {
        memcpy(m->tensors[i].name, ti[i].name, sizeof(m->tensors[i].name));
        m->tensors[i].type = ti[i].type;
        m->tensors[i].n_elements = tensor_nelem(&ti[i]);
        const uint64_t off = base + ti[i].offset;
        if (off > (uint64_t)sz) {
            gguf_free(m);
            free(ti);
            return NULL;
        }
        m->tensors[i].bytes = (const uint8_t *)p + off;
    }
    free(ti);
    return m;
#endif
}

void gguf_free(gguf_model_t *m)
{
    if (!m)
        return;
#if !defined(_WIN32)
    if (m->map_base && m->map_size)
        munmap(m->map_base, m->map_size);
#endif
    free(m->tensors);
    free(m);
}

uint64_t gguf_model_tensor_count(const gguf_model_t *m)
{
    return m ? m->tensor_count : 0ull;
}

const gguf_tensor_t *gguf_model_tensor(const gguf_model_t *m, uint64_t i)
{
    if (!m || !m->tensors || i >= m->tensor_count)
        return NULL;
    return &m->tensors[i];
}
