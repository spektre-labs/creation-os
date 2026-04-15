/*
 * Creation OS v7 — ObjC++ AMX + Metal host. "Hard silicon" path for retro fingerprint.
 * AMX micro-ops: amx_aarch64.h (reverse-engineered encodings; see corsix/amx).
 */
#import <Foundation/Foundation.h>
#import <CommonCrypto/CommonDigest.h>
#if !defined(_SK9_NO_METAL)
#import <Metal/Metal.h>
#endif
#import <dlfcn.h>
#import <cctype>
#import <cmath>
#import <cstdlib>
#import <cstring>

#include "amx_aarch64.h"

extern "C" {

uint32_t sk9_amx_retro_fingerprint_u128(uint64_t q_lo, uint64_t q_hi);
int sk9_amx_retro_available(void);

/* --- ASCII trim + lower (matches Python path closely for Latin prompts) --- */
static size_t normalize_prompt_inplace(char *buf, size_t n) {
    size_t w = 0;
    size_t i = 0;
    while (i < n && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\n' || buf[i] == '\r'))
        i++;
    while (i < n) {
        char c = buf[i++];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c + ('a' - 'A'));
        buf[w++] = c;
    }
    while (w > 0 && (buf[w - 1] == ' ' || buf[w - 1] == '\t' || buf[w - 1] == '\n' || buf[w - 1] == '\r'))
        w--;
    buf[w] = '\0';
    return w;
}

/*
 * Multi-row vector FMA32: exercise 4 Z rows (256 B observable) — stronger AMX coupling than single row.
 */
#if defined(__APPLE__) && defined(__arm64__)

static uint32_t fold_u32(const uint8_t *p, size_t n) {
    uint32_t h = 0x811C9DC5u;
    for (size_t i = 0; i < n; i++) {
        h ^= (uint32_t)p[i];
        h *= 0x01000193u;
    }
    return h;
}

static uint64_t fpcr_dn_on(void) {
    uint64_t old_fpcr, new_fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(old_fpcr));
    new_fpcr = old_fpcr | (1ull << 25);
    __asm__ volatile("msr fpcr, %0" : : "r"(new_fpcr));
    return old_fpcr;
}

static void fpcr_restore(uint64_t fpcr) { __asm__ volatile("msr fpcr, %0" : : "r"(fpcr)); }

static void pack_channels(const uint8_t *lo8, const uint8_t *hi8, float *x16, float *y16) {
    for (int i = 0; i < 8; i++) {
        x16[i] = (float)lo8[i] * (1.0f / 255.0f);
        x16[8 + i] = (float)hi8[i] * (1.0f / 255.0f);
        y16[i] = 1.0f;
        y16[8 + i] = 1.0f;
    }
}

static uint32_t sk9_amx_retro_fingerprint_u128_multiz(uint64_t q_lo, uint64_t q_hi) {
    alignas(256) uint8_t xy_blob[512];
    alignas(256) uint8_t z_acc[256];
    memset(xy_blob, 0, sizeof(xy_blob));
    memset(z_acc, 0, sizeof(z_acc));
    uint8_t lo8[8], hi8[8];
    memcpy(lo8, &q_lo, 8);
    memcpy(hi8, &q_hi, 8);
    float *x16 = (float *)xy_blob;
    float *y16 = (float *)(xy_blob + 256);
    pack_channels(lo8, hi8, x16, y16);

    uint64_t old_fpcr = fpcr_dn_on();
    AMX_SET();
    AMX_LDX(AMX_PTR_ROW_FLAGS(xy_blob, 0, 1));
    AMX_LDY(AMX_PTR_ROW_FLAGS(xy_blob + 256, 0, 1));
    for (int row = 0; row < 4; row++) {
        uint64_t op = FMA_VECTOR_PRODUCT | ((uint64_t)row << 20);
        AMX_FMA32(op);
        AMX_STZ(AMX_PTR_ROW_FLAGS(z_acc + row * 64, row, 0));
    }
    AMX_CLR();
    fpcr_restore(old_fpcr);
    return fold_u32(z_acc, sizeof(z_acc));
}

#endif /* __APPLE__ && __arm64__ */

/*
 * NULL / hardware failure → 0 (bit-level NULL signature contract).
 */
uint32_t amx_retro_fingerprint(const char *utf8) {
    if (!utf8)
        return 0;
#if !defined(__APPLE__) || !defined(__arm64__)
    (void)utf8;
    return 0;
#else
    size_t len = strlen(utf8);
    if (len == 0)
        return 0;
    char *tmp = (char *)malloc(len + 1);
    if (!tmp)
        return 0;
    memcpy(tmp, utf8, len + 1);
    normalize_prompt_inplace(tmp, len);
    if (tmp[0] == '\0') {
        free(tmp);
        return 0;
    }
    uint8_t digest[CC_SHA256_DIGEST_LENGTH];
    CC_SHA256(tmp, (CC_LONG)strlen(tmp), digest);
    free(tmp);
    uint64_t lo = 0, hi = 0;
    memcpy(&lo, digest, 8);
    memcpy(&hi, digest + 8, 8);
    return sk9_amx_retro_fingerprint_u128_multiz(lo, hi);
#endif
}

/*
 * Invariant: after AMX x·y+z with unit attractor, first Z lane should round to 1.0f within tol.
 */
int sk9_amx_invariant_one_locked(float tol) {
#if !defined(__APPLE__) || !defined(__arm64__)
    (void)tol;
    return 0;
#else
    if (tol <= 0.f)
        tol = 1e-4f;
    alignas(256) uint8_t xy_blob[512];
    alignas(256) uint8_t z_scratch[256];
    memset(xy_blob, 0, sizeof(xy_blob));
    memset(z_scratch, 0, sizeof(z_scratch));
    float *x16 = (float *)xy_blob;
    float *y16 = (float *)(xy_blob + 256);
    for (int i = 0; i < 16; i++) {
        x16[i] = 1.0f;
        y16[i] = 1.0f;
    }
    uint64_t old_fpcr = fpcr_dn_on();
    AMX_SET();
    AMX_LDX(AMX_PTR_ROW_FLAGS(xy_blob, 0, 1));
    AMX_LDY(AMX_PTR_ROW_FLAGS(xy_blob + 256, 0, 1));
    AMX_FMA32(FMA_VECTOR_PRODUCT);
    AMX_STZ(AMX_PTR_ROW_FLAGS(z_scratch, 0, 0));
    AMX_CLR();
    fpcr_restore(old_fpcr);
    float z0;
    memcpy(&z0, z_scratch, sizeof(float));
    float r = roundf(z0);
    return (fabsf(r - 1.0f) <= tol) ? 1 : 0;
#endif
}

/*
 * Soft σ proxy from hardware signature (deterministic): popcount of 32-bit fingerprint, capped.
 */
int sk9_amx_evaluate_sigma_soft(const char *utf8) {
    uint32_t fp = amx_retro_fingerprint(utf8);
    if (fp == 0)
        return 8;
    int s = __builtin_popcount(fp);
    if (s > 8)
        s = 8;
    return s;
}

/*
 * GPU SNL parallel lane ops. Returns 0 OK, -1 if Metal unavailable / kernel missing.
 */
int sk9_metal_snl_parallel(
    const uint32_t *lane_a,
    const uint32_t *lane_b,
    size_t n,
    uint32_t *out_pop,
    uint32_t *out_xor,
    uint32_t *out_and) {
#if defined(_SK9_NO_METAL)
    (void)lane_a;
    (void)lane_b;
    (void)n;
    (void)out_pop;
    (void)out_xor;
    (void)out_and;
    return -1;
#else
    if (!lane_a || !lane_b || !out_pop || !out_xor || !out_and || n == 0 || n > 1u << 20)
        return -1;
    @autoreleasepool {
        id<MTLDevice> dev = MTLCreateSystemDefaultDevice();
        if (!dev)
            return -1;
        NSError *err = nil;
        Dl_info di{};
        NSString *ml = nil;
        if (dladdr((const void *)amx_retro_fingerprint, &di) && di.dli_fname) {
            NSString *p = [NSString stringWithUTF8String:di.dli_fname];
            NSString *base = [p stringByDeletingPathExtension];
            ml = [base stringByAppendingPathExtension:@"metallib"];
        }
        if (!ml || ![[NSFileManager defaultManager] fileExistsAtPath:ml])
            return -1;
        NSURL *url = [NSURL fileURLWithPath:ml];
        id<MTLLibrary> lib = [dev newLibraryWithURL:url error:&err];
        if (!lib)
            return -1;
        id<MTLFunction> fn = [lib newFunctionWithName:@"snl_symbol_parallel"];
        if (!fn)
            return -1;
        id<MTLComputePipelineState> pipe = [dev newComputePipelineStateWithFunction:fn error:&err];
        if (!pipe)
            return -1;
        id<MTLCommandQueue> q = [dev newCommandQueue];
        id<MTLBuffer> bA = [dev newBufferWithBytes:lane_a length:n * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bB = [dev newBufferWithBytes:lane_b length:n * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bP = [dev newBufferWithLength:n * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bX = [dev newBufferWithLength:n * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bN = [dev newBufferWithLength:n * sizeof(uint32_t) options:MTLResourceStorageModeShared];
        id<MTLCommandBuffer> cmd = [q commandBuffer];
        id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
        [enc setComputePipelineState:pipe];
        [enc setBuffer:bA offset:0 atIndex:0];
        [enc setBuffer:bB offset:0 atIndex:1];
        [enc setBuffer:bP offset:0 atIndex:2];
        [enc setBuffer:bX offset:0 atIndex:3];
        [enc setBuffer:bN offset:0 atIndex:4];
        uint32_t n32 = (uint32_t)n;
        [enc setBytes:&n32 length:sizeof(n32) atIndex:5];
        NSUInteger w = pipe.threadExecutionWidth;
        if (w < 1)
            w = 32;
        NSUInteger groups = (n + w - 1) / w;
        [enc dispatchThreadgroups:MTLSizeMake(groups, 1, 1) threadsPerThreadgroup:MTLSizeMake(w, 1, 1)];
        [enc endEncoding];
        [cmd commit];
        [cmd waitUntilCompleted];
        memcpy(out_pop, [bP contents], n * sizeof(uint32_t));
        memcpy(out_xor, [bX contents], n * sizeof(uint32_t));
        memcpy(out_and, [bN contents], n * sizeof(uint32_t));
    }
    return 0;
#endif
}

} /* extern "C" */
