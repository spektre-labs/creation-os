/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Read bytes from a file via mmap (POSIX). Merge-gate safe; Windows stub.
 */
#include "cos_gguf.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#if defined(_WIN32)
int cos_gguf_mmap_read_at(const char *path, uint64_t off, void *dst, size_t len)
{
    (void)path;
    (void)off;
    (void)dst;
    (void)len;
    return -1;
}
#else
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int cos_gguf_mmap_read_at(const char *path, uint64_t off, void *dst, size_t len)
{
    if (!path || !dst || len == 0u)
        return -1;
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;
    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -1;
    }
    if (!S_ISREG(st.st_mode)) {
        close(fd);
        return -1;
    }
    uint64_t sz = (uint64_t)st.st_size;
    if (off > sz || len > (size_t)(sz - off)) {
        close(fd);
        return -1;
    }
    void *p = mmap(NULL, (size_t)sz, PROT_READ, MAP_PRIVATE, fd, 0);
    if (p == MAP_FAILED) {
        close(fd);
        return -1;
    }
    memcpy(dst, (const uint8_t *)p + off, len);
    munmap(p, (size_t)sz);
    close(fd);
    return 0;
}
#endif
