/* CREATION OS — Superkernel v9 bare-metal — C ABI (opaque USM / kernel) */
#ifndef SUPERKERNEL_V9_M4_MM_H
#define SUPERKERNEL_V9_M4_MM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SK9_MM_NA         18
#define SK9_MM_VOCAB      32000
#define SK9_MM_MAX_SEQ    512
#define SK9_MM_HIDDEN_DIM 3072
#define SK9_MM_N_LAYERS_T 28
#define SK9_MM_MAX_ORB    32

typedef struct Sk9MMUSM Sk9MMUSM;
typedef struct Sk9MMKern Sk9MMKern;

Sk9MMUSM *sk9_mm_usm_create(void);
void       sk9_mm_usm_destroy(Sk9MMUSM *u);

Sk9MMKern *sk9_mm_kern_create(void);
void       sk9_mm_kern_destroy(Sk9MMKern *k);

void sk9_mm_kern_eval(Sk9MMKern *k, uint32_t assertion_bits);

/** Per-layer repulsion: Accelerate cblas on Darwin P-core queue (NPU/GPU off). */
void sk9_mm_repulsion_neon(Sk9MMUSM *u, int32_t n_tokens, float lambda);

/** mmap(MAP_PRIVATE) whole file read-only. Caller munmap(*out_ptr, *out_size). */
int sk9_mm_mmap_file_ro(const char *path, void **out_ptr, size_t *out_size);

typedef enum {
    SK9_ROUTE_BBHASH      = 0,
    SK9_ROUTE_KERNEL      = 1,
    SK9_ROUTE_TRANSFORMER = 2,
} Sk9EpistemicRoute;

/**
 * Epistemic order: (0) BBHash mmap table if loaded and key hits, else (1) kernel path hint,
 * else (2) Core ML. On BBHash hit, *out_val is set (if non-NULL).
 */
Sk9EpistemicRoute sk9_mm_epistemic_route(Sk9MMUSM *u, uint64_t query_key, uint32_t *out_val);

/** Attach BBHash table: mmap file. Layout: u32 capacity LE; then capacity slots of { u64 key, u32 val, u32 pad } key 0 = empty. Returns 0 on ok. */
int sk9_mm_bbhash_attach(Sk9MMUSM *u, const char *path);
void                 sk9_mm_bbhash_detach(Sk9MMUSM *u);

int sk9_mm_living_weights_metal(Sk9MMUSM *u);

int sk9_mm_coreml_load(Sk9MMUSM *u, const char *mlmodelc_path);
int sk9_mm_coreml_forward(Sk9MMUSM *u);

void sk9_mm_daemon_start(Sk9MMUSM *u, const char *heartbeat_path);
void sk9_mm_daemon_stop(Sk9MMUSM *u);

#ifdef __cplusplus
}
#endif

#endif
