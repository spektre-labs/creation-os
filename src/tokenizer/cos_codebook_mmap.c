/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "cos_codebook_mmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define COS_CB_ROW_SEED 0xC0DEB001D0C0DEB0ULL

struct CosCodebook {
    void *storage; /* mmap (POSIX) or malloc+fread (Windows) */
    size_t nbytes;
    const uint64_t *rows;
    uint32_t n_vocab;
    int is_heap; /* 1 => free(storage); 0 => munmap(storage, nbytes) */
};

static uint64_t splitmix64(uint64_t *x)
{
    uint64_t z = (*x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void cos_codebook_row_fill(uint32_t id, uint64_t *hv)
{
    uint64_t st = COS_CB_ROW_SEED ^ ((uint64_t)id * 0xD6E8FEB8669FD5D5ULL);
    for (int i = 0; i < COS_W; i++)
        hv[i] = splitmix64(&st);
}

int cos_codebook_write_file(const char *path, uint32_t n_vocab)
{
    if (!path || n_vocab == 0u)
        return -1;
    const uint32_t row_bytes = (uint32_t)(COS_W * (int)sizeof(uint64_t));
    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    CosCodebookHeader h;
    memset(&h, 0, sizeof(h));
    h.magic = COS_CB_MAGIC;
    h.version = COS_CB_VERSION;
    h.n_vocab = n_vocab;
    h.row_bytes = row_bytes;
    if (fwrite(&h, sizeof(h), 1u, f) != 1u) {
        fclose(f);
        return -1;
    }
    uint64_t row[COS_W];
    for (uint32_t id = 0; id < n_vocab; id++) {
        cos_codebook_row_fill(id, row);
        if (fwrite(row, sizeof(row), 1u, f) != 1u) {
            fclose(f);
            return -1;
        }
    }
    if (fclose(f) != 0)
        return -1;
    return 0;
}

static int validate_header_and_size(const CosCodebookHeader *hdr, size_t file_bytes)
{
    if (hdr->magic != COS_CB_MAGIC || hdr->version != COS_CB_VERSION || hdr->n_vocab == 0u)
        return -1;
    const uint32_t row_bytes = (uint32_t)(COS_W * (int)sizeof(uint64_t));
    if (hdr->row_bytes != row_bytes)
        return -1;
    const uint64_t need = (uint64_t)sizeof(CosCodebookHeader) + (uint64_t)hdr->n_vocab * (uint64_t)row_bytes;
    if ((uint64_t)file_bytes < need)
        return -1;
    return 0;
}

CosCodebook *cos_codebook_mmap_open(const char *path)
{
    if (!path)
        return NULL;

#if defined(_WIN32)
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long szl = ftell(f);
    if (szl < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    size_t sz = (size_t)szl;
    if (sz < sizeof(CosCodebookHeader)) {
        fclose(f);
        return NULL;
    }
    uint8_t *buf = (uint8_t *)malloc(sz);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    if (fread(buf, 1, sz, f) != sz) {
        free(buf);
        fclose(f);
        return NULL;
    }
    fclose(f);
    const CosCodebookHeader *hdr = (const CosCodebookHeader *)buf;
    if (validate_header_and_size(hdr, sz) != 0) {
        free(buf);
        return NULL;
    }
    CosCodebook *cb = (CosCodebook *)calloc(1, sizeof(CosCodebook));
    if (!cb) {
        free(buf);
        return NULL;
    }
    cb->storage = buf;
    cb->nbytes = sz;
    cb->rows = (const uint64_t *)(buf + sizeof(CosCodebookHeader));
    cb->n_vocab = hdr->n_vocab;
    cb->is_heap = 1;
    return cb;
#else
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return NULL;
    }
    size_t sz = (size_t)st.st_size;
    if (sz < sizeof(CosCodebookHeader)) {
        close(fd);
        return NULL;
    }
    void *p = mmap(NULL, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED)
        return NULL;
    const CosCodebookHeader *hdr = (const CosCodebookHeader *)p;
    if (validate_header_and_size(hdr, sz) != 0) {
        munmap(p, sz);
        return NULL;
    }
    CosCodebook *cb = (CosCodebook *)calloc(1, sizeof(CosCodebook));
    if (!cb) {
        munmap(p, sz);
        return NULL;
    }
    cb->storage = p;
    cb->nbytes = sz;
    cb->rows = (const uint64_t *)((const uint8_t *)p + sizeof(CosCodebookHeader));
    cb->n_vocab = hdr->n_vocab;
    cb->is_heap = 0;
    return cb;
#endif
}

void cos_codebook_mmap_close(CosCodebook *cb)
{
    if (!cb)
        return;
    if (cb->storage) {
#if defined(_WIN32)
        free(cb->storage);
#else
        if (cb->is_heap)
            free(cb->storage);
        else
            munmap(cb->storage, cb->nbytes);
#endif
        cb->storage = NULL;
        cb->rows = NULL;
    }
    free(cb);
}

uint32_t cos_codebook_n_vocab(const CosCodebook *cb)
{
    return cb ? cb->n_vocab : 0u;
}

int cos_codebook_lookup(const CosCodebook *cb, uint32_t id, uint64_t *hv)
{
    if (!cb || !hv || cb->n_vocab == 0u)
        return -1;
    const uint32_t idx = id % cb->n_vocab;
    const uint64_t *row = cb->rows + (size_t)idx * (size_t)COS_W;
    memcpy(hv, row, sizeof(uint64_t) * (size_t)COS_W);
    return 0;
}
