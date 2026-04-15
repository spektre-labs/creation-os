/*
 * CREATION OS — Ephemeral JIT (MAP_JIT + pthread_jit_write_protect_np).
 * Entitlement: com.apple.security.cs.allow-jit (sign the binary with this plist).
 */
#include "jit_engine.h"
#include "jit_templates.h"

#include <errno.h>
#include <libkern/OSCacheControl.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#ifndef MAP_JIT
#define MAP_JIT 0
#endif

void jit_emit_movz_x(uint8_t **wp, unsigned rd, uint16_t imm16, unsigned shift_hw) {
    uint32_t hw = (shift_hw / 16u) & 3u;
    uint32_t insn = 0xd2800000u | (((uint32_t)imm16 & 0xffffu) << 5) | (hw << 21) | (rd & 31u);
    jit_store_u32(*wp, insn);
    *wp += 4;
}

void jit_emit_movk_x(uint8_t **wp, unsigned rd, uint16_t imm16, unsigned shift_hw) {
    uint32_t hw = (shift_hw / 16u) & 3u;
    uint32_t insn = 0xf2800000u | (((uint32_t)imm16 & 0xffffu) << 5) | (hw << 21) | (rd & 31u);
    jit_store_u32(*wp, insn);
    *wp += 4;
}

void jit_emit_ret(uint8_t **wp) {
    /* RET: 0xd65f03c0 */
    jit_store_u32(*wp, 0xd65f03c0u);
    *wp += 4;
}

int jit_ephemeral_exec(const uint8_t *code, size_t len, uint64_t *out_ret) {
#if !defined(__aarch64__)
    (void)code;
    (void)len;
    (void)out_ret;
    return -1;
#else
    if (!code || len < 4u || !out_ret || len > 65536u)
        return -1;

    long pgsz = sysconf(_SC_PAGESIZE);
    if (pgsz < 4096)
        pgsz = 4096;
    size_t mapsz = (len + (size_t)pgsz - 1u) & ~(size_t)((size_t)pgsz - 1u);

    void *p = mmap(NULL, mapsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED)
        return -1;

    memcpy(p, code, len);
    sys_icache_invalidate(p, mapsz);

    typedef uint64_t (*jit_fn)(void);
    uint64_t r = 0;

    if (mprotect(p, mapsz, PROT_READ | PROT_EXEC) == 0) {
        r = ((jit_fn)p)();
        munmap(p, mapsz);
        *out_ret = r;
        return 0;
    }

    munmap(p, mapsz);

#if defined(__APPLE__) && defined(MAP_JIT) && MAP_JIT
    void *pj = mmap(NULL, mapsz, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_JIT, -1, 0);
    if (pj == MAP_FAILED)
        return -1;
    pthread_jit_write_protect_np(0);
    memcpy(pj, code, len);
    sys_icache_invalidate(pj, mapsz);
    pthread_jit_write_protect_np(1);
    r = ((jit_fn)pj)();
    pthread_jit_write_protect_np(0);
    munmap(pj, mapsz);
    *out_ret = r;
    return 0;
#else
    return -1;
#endif
#endif
}

int jit_emit_return_imm64(uint8_t *buf, size_t cap, uint64_t imm) {
    if (cap < 32u)
        return -1;
    uint8_t *w = buf;
    /* movz x0, imm[15:0] */
    jit_emit_movz_x(&w, 0, (uint16_t)(imm & 0xffffu), 0);
    if ((imm >> 16) & 0xffffu)
        jit_emit_movk_x(&w, 0, (uint16_t)((imm >> 16) & 0xffffu), 16);
    if ((imm >> 32) & 0xffffu)
        jit_emit_movk_x(&w, 0, (uint16_t)((imm >> 32) & 0xffffu), 32);
    if ((imm >> 48) & 0xffffu)
        jit_emit_movk_x(&w, 0, (uint16_t)((imm >> 48) & 0xffffu), 48);
    jit_emit_ret(&w);
    return (int)(w - buf);
}
