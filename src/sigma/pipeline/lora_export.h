/*
 * σ-export — shareable, integrity-checked LoRA adapters.
 *
 * The `.cos` container is a fixed, byte-stable layout so the
 * verification happens deterministically on every host:
 *
 *     struct {
 *         uint8_t  magic[8];          = "COSLORA1"
 *         uint32_t version;           = 1
 *         uint32_t flags;             = 0
 *         char     name[32];
 *         char     description[128];
 *         char     trained_by[64];
 *         int32_t  in_dim, out_dim, rank;
 *         float    alpha, benchmark_sigma;
 *         uint64_t seed;
 *         uint64_t created_unix;
 *         float    A[rank * in_dim];  // row-major
 *         float    B[out_dim * rank]; // row-major
 *         uint8_t  mac[32];           // FNV-1a64 rolled into 32 bytes
 *     }
 *
 * The MAC covers every byte of the header + A + B.  This is not
 * cryptographically strong on its own — the v1.3 kernel pairs it
 * with the existing Ed25519 signing path in src/ed25519.c for the
 * eventual marketplace flow, but the standalone integrity check
 * here is enough to reject casual tampering and works without
 * any secrets.
 *
 * Multi-byte integers & floats are written little-endian; this is
 * the dominant target for Creation OS and avoids dragging in an
 * endian conversion dependency for the embedded ports.  On
 * big-endian hosts the functions swap bytes internally.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_LORA_EXPORT_H
#define COS_SIGMA_LORA_EXPORT_H

#include <stdint.h>
#include <stddef.h>

#include "lora.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COS_LORA_EXPORT_MAGIC      "COSLORA1"
#define COS_LORA_EXPORT_VERSION     1u
#define COS_LORA_EXPORT_DESC_MAX   128
#define COS_LORA_EXPORT_AUTHOR_MAX  64
#define COS_LORA_EXPORT_MAC_BYTES   32

enum cos_lora_export_status {
    COS_LORA_EXPORT_OK           =  0,
    COS_LORA_EXPORT_ERR_ARG      = -1,
    COS_LORA_EXPORT_ERR_IO       = -2,
    COS_LORA_EXPORT_ERR_FORMAT   = -3,
    COS_LORA_EXPORT_ERR_MAGIC    = -4,
    COS_LORA_EXPORT_ERR_VERSION  = -5,
    COS_LORA_EXPORT_ERR_MAC      = -6,   /* tamper detected */
};

typedef struct {
    char     description[COS_LORA_EXPORT_DESC_MAX];
    char     trained_by  [COS_LORA_EXPORT_AUTHOR_MAX];
    float    benchmark_sigma;
    uint64_t created_unix;   /* caller may pass 0 → kernel uses 0 for
                               * deterministic tests; real code passes
                               * time(NULL). */
} cos_lora_export_meta_t;

typedef struct {
    char     name[COS_LORA_MAX_NAME];
    char     description[COS_LORA_EXPORT_DESC_MAX];
    char     trained_by [COS_LORA_EXPORT_AUTHOR_MAX];
    int      in_dim, out_dim, rank;
    float    alpha;
    float    benchmark_sigma;
    uint64_t seed;
    uint64_t created_unix;
    size_t   bytes_weights;
    uint8_t  mac[COS_LORA_EXPORT_MAC_BYTES];
} cos_lora_export_info_t;

/* ==================================================================
 * Write / read
 * ================================================================== */

/* Serialise `a` + `meta` into the fixed-layout file.  `meta` may be
 * NULL → blank strings, sigma=0, ts=0. */
int cos_lora_export_write(const cos_lora_adapter_t  *a,
                          const cos_lora_export_meta_t *meta,
                          const char *path);

/* Load a `.cos` file back into `out`.  Performs magic / version /
 * size / MAC checks; returns COS_LORA_EXPORT_ERR_MAC when the MAC
 * doesn't match, so the caller must not promote the adapter. */
int cos_lora_export_read(const char *path,
                         cos_lora_adapter_t *out,
                         cos_lora_export_info_t *info);

/* Peek at the header without allocating an adapter. */
int cos_lora_export_peek(const char *path,
                         cos_lora_export_info_t *info);

/* ==================================================================
 * Self-test
 * ================================================================== */
int cos_lora_export_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_LORA_EXPORT_H */
