/*
 * σ-export — implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "lora_export.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* ------------------------ endian helpers ------------------------ */

static int host_is_little(void) {
    const uint16_t v = 1;
    return *(const uint8_t *)&v == 1;
}

static void put_u32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v);       p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static void put_i32(uint8_t *p, int32_t v) { put_u32(p, (uint32_t)v); }
static void put_u64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}
static uint32_t get_u32(const uint8_t *p) {
    return  (uint32_t)p[0]        | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static int32_t  get_i32(const uint8_t *p) { return (int32_t)get_u32(p); }
static uint64_t get_u64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)p[i] << (8 * i);
    return v;
}

static void put_f32(uint8_t *p, float f) {
    uint32_t u; memcpy(&u, &f, 4);
    put_u32(p, u);
}
static float get_f32(const uint8_t *p) {
    uint32_t u = get_u32(p); float f; memcpy(&f, &u, 4); return f;
}

/* ------------------------ layout ------------------------ */

#define HDR_MAGIC_OFF       0
#define HDR_VERSION_OFF     8
#define HDR_FLAGS_OFF      12
#define HDR_NAME_OFF       16                             /* 32 bytes */
#define HDR_DESC_OFF      (HDR_NAME_OFF + 32)             /* 128 bytes */
#define HDR_AUTHOR_OFF    (HDR_DESC_OFF + COS_LORA_EXPORT_DESC_MAX)  /* 64 */
#define HDR_IN_OFF        (HDR_AUTHOR_OFF + COS_LORA_EXPORT_AUTHOR_MAX)
#define HDR_OUT_OFF       (HDR_IN_OFF + 4)
#define HDR_RANK_OFF      (HDR_OUT_OFF + 4)
#define HDR_ALPHA_OFF     (HDR_RANK_OFF + 4)
#define HDR_BENCH_OFF     (HDR_ALPHA_OFF + 4)
#define HDR_SEED_OFF      (HDR_BENCH_OFF + 4)
#define HDR_CREATED_OFF   (HDR_SEED_OFF + 8)
#define HDR_BYTES         (HDR_CREATED_OFF + 8)

/* ------------------------ MAC ------------------------
 *
 * We want a 32-byte tag with a single pure-C primitive.  Two
 * interleaved FNV-1a64 streams seeded with different primes give us
 * 16 bytes of header+payload-dependent output; we repeat the pair
 * with a second seed family to fill 32 bytes.  Collision-resistance
 * is not the goal here — integrity against accidental corruption &
 * casual edits is.  Ed25519 signing (D4) layers on top for the
 * marketplace flow.
 */
static void lora_mac(const uint8_t *buf, size_t len, uint8_t out[32]) {
    static const uint64_t seeds[4] = {
        0xCBF29CE484222325ULL, 0x9E3779B97F4A7C15ULL,
        0xD1B54A32D192ED03ULL, 0xA3B1C2D3E4F5061FULL,
    };
    static const uint64_t primes[4] = {
        0x100000001B3ULL, 0x100000001D1ULL,
        0x100000001E5ULL, 0x1000000020FULL,
    };
    uint64_t h[4] = { seeds[0], seeds[1], seeds[2], seeds[3] };
    for (size_t i = 0; i < len; i++) {
        for (int k = 0; k < 4; k++) {
            h[k] ^= buf[i];
            h[k] *= primes[k];
        }
    }
    for (int k = 0; k < 4; k++) {
        put_u64(out + k * 8, h[k]);
    }
}

/* ------------------------ write ------------------------ */

static int write_all(FILE *fp, const void *buf, size_t n) {
    return fwrite(buf, 1, n, fp) == n ? 0 : COS_LORA_EXPORT_ERR_IO;
}

int cos_lora_export_write(const cos_lora_adapter_t *a,
                          const cos_lora_export_meta_t *meta,
                          const char *path) {
    if (!a || !path) return COS_LORA_EXPORT_ERR_ARG;
    if (!a->A || !a->B) return COS_LORA_EXPORT_ERR_ARG;

    size_t nA = (size_t)a->rank    * (size_t)a->in_dim;
    size_t nB = (size_t)a->out_dim * (size_t)a->rank;
    size_t wbytes = (nA + nB) * sizeof(float);
    size_t total = HDR_BYTES + wbytes;

    uint8_t *buf = (uint8_t *)calloc(1, total);
    if (!buf) return COS_LORA_EXPORT_ERR_IO;

    memcpy(buf + HDR_MAGIC_OFF, COS_LORA_EXPORT_MAGIC, 8);
    put_u32(buf + HDR_VERSION_OFF, COS_LORA_EXPORT_VERSION);
    put_u32(buf + HDR_FLAGS_OFF,   0);
    strncpy((char *)(buf + HDR_NAME_OFF), a->name, 32);
    if (meta) {
        strncpy((char *)(buf + HDR_DESC_OFF),   meta->description,
                COS_LORA_EXPORT_DESC_MAX);
        strncpy((char *)(buf + HDR_AUTHOR_OFF), meta->trained_by,
                COS_LORA_EXPORT_AUTHOR_MAX);
    }
    put_i32(buf + HDR_IN_OFF,   a->in_dim);
    put_i32(buf + HDR_OUT_OFF,  a->out_dim);
    put_i32(buf + HDR_RANK_OFF, a->rank);
    put_f32(buf + HDR_ALPHA_OFF, a->alpha);
    put_f32(buf + HDR_BENCH_OFF, meta ? meta->benchmark_sigma : 0.0f);
    put_u64(buf + HDR_SEED_OFF, a->seed);
    put_u64(buf + HDR_CREATED_OFF, meta ? meta->created_unix : 0ULL);

    /* Serialise weights little-endian. */
    uint8_t *wp = buf + HDR_BYTES;
    if (host_is_little()) {
        memcpy(wp,           a->A, nA * sizeof(float));
        memcpy(wp + nA*sizeof(float), a->B, nB * sizeof(float));
    } else {
        for (size_t i = 0; i < nA; i++) put_f32(wp + i*4, a->A[i]);
        wp += nA*4;
        for (size_t i = 0; i < nB; i++) put_f32(wp + i*4, a->B[i]);
    }

    /* MAC covers everything in `buf` so far. */
    uint8_t mac[COS_LORA_EXPORT_MAC_BYTES];
    lora_mac(buf, total, mac);

    FILE *fp = fopen(path, "wb");
    if (!fp) { free(buf); return COS_LORA_EXPORT_ERR_IO; }
    int rc = write_all(fp, buf, total);
    if (rc == 0) rc = write_all(fp, mac, sizeof mac);
    fclose(fp);
    free(buf);
    return rc;
}

/* ------------------------ read ------------------------ */

static int read_all(FILE *fp, void *buf, size_t n) {
    return fread(buf, 1, n, fp) == n ? 0 : COS_LORA_EXPORT_ERR_IO;
}

static int parse_header(const uint8_t *buf, cos_lora_export_info_t *info) {
    if (memcmp(buf + HDR_MAGIC_OFF, COS_LORA_EXPORT_MAGIC, 8) != 0)
        return COS_LORA_EXPORT_ERR_MAGIC;
    uint32_t ver = get_u32(buf + HDR_VERSION_OFF);
    if (ver != COS_LORA_EXPORT_VERSION) return COS_LORA_EXPORT_ERR_VERSION;

    memset(info, 0, sizeof *info);
    memcpy(info->name,        buf + HDR_NAME_OFF,   COS_LORA_MAX_NAME - 1);
    memcpy(info->description, buf + HDR_DESC_OFF,   COS_LORA_EXPORT_DESC_MAX - 1);
    memcpy(info->trained_by,  buf + HDR_AUTHOR_OFF, COS_LORA_EXPORT_AUTHOR_MAX - 1);
    info->in_dim  = get_i32(buf + HDR_IN_OFF);
    info->out_dim = get_i32(buf + HDR_OUT_OFF);
    info->rank    = get_i32(buf + HDR_RANK_OFF);
    info->alpha           = get_f32(buf + HDR_ALPHA_OFF);
    info->benchmark_sigma = get_f32(buf + HDR_BENCH_OFF);
    info->seed            = get_u64(buf + HDR_SEED_OFF);
    info->created_unix    = get_u64(buf + HDR_CREATED_OFF);
    if (info->in_dim  <= 0 || info->in_dim  > COS_LORA_MAX_IN_DIM)  return COS_LORA_EXPORT_ERR_FORMAT;
    if (info->out_dim <= 0 || info->out_dim > COS_LORA_MAX_OUT_DIM) return COS_LORA_EXPORT_ERR_FORMAT;
    if (info->rank    <= 0 || info->rank    > COS_LORA_MAX_RANK)    return COS_LORA_EXPORT_ERR_FORMAT;
    if (!(info->alpha > 0.0f)) return COS_LORA_EXPORT_ERR_FORMAT;
    return 0;
}

int cos_lora_export_peek(const char *path,
                         cos_lora_export_info_t *info) {
    if (!path || !info) return COS_LORA_EXPORT_ERR_ARG;
    FILE *fp = fopen(path, "rb");
    if (!fp) return COS_LORA_EXPORT_ERR_IO;
    uint8_t hdr[HDR_BYTES];
    int rc = read_all(fp, hdr, HDR_BYTES);
    fclose(fp);
    if (rc != 0) return rc;
    rc = parse_header(hdr, info);
    if (rc != 0) return rc;
    info->bytes_weights =
        ((size_t)info->rank    * (size_t)info->in_dim +
         (size_t)info->out_dim * (size_t)info->rank) * sizeof(float);
    memset(info->mac, 0, sizeof info->mac);
    return COS_LORA_EXPORT_OK;
}

int cos_lora_export_read(const char *path,
                         cos_lora_adapter_t *out,
                         cos_lora_export_info_t *info) {
    if (!path || !out) return COS_LORA_EXPORT_ERR_ARG;

    struct stat sb;
    if (stat(path, &sb) != 0) return COS_LORA_EXPORT_ERR_IO;
    if ((size_t)sb.st_size < HDR_BYTES + COS_LORA_EXPORT_MAC_BYTES)
        return COS_LORA_EXPORT_ERR_FORMAT;

    size_t total = (size_t)sb.st_size;
    size_t payload_len = total - COS_LORA_EXPORT_MAC_BYTES;

    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return COS_LORA_EXPORT_ERR_IO;
    FILE *fp = fopen(path, "rb");
    if (!fp) { free(buf); return COS_LORA_EXPORT_ERR_IO; }
    int rc = read_all(fp, buf, total);
    fclose(fp);
    if (rc != 0) { free(buf); return rc; }

    cos_lora_export_info_t lc;
    rc = parse_header(buf, &lc);
    if (rc != 0) { free(buf); return rc; }

    size_t wbytes = ((size_t)lc.rank * (size_t)lc.in_dim +
                     (size_t)lc.out_dim * (size_t)lc.rank) * sizeof(float);
    if (payload_len != HDR_BYTES + wbytes) { free(buf); return COS_LORA_EXPORT_ERR_FORMAT; }

    /* Verify MAC. */
    uint8_t expected[COS_LORA_EXPORT_MAC_BYTES];
    lora_mac(buf, payload_len, expected);
    if (memcmp(expected, buf + payload_len, COS_LORA_EXPORT_MAC_BYTES) != 0) {
        free(buf); return COS_LORA_EXPORT_ERR_MAC;
    }

    /* Build the adapter. */
    rc = cos_lora_init(out, lc.name, lc.in_dim, lc.out_dim, lc.rank,
                       lc.alpha, lc.seed);
    if (rc != COS_LORA_OK) { free(buf); return COS_LORA_EXPORT_ERR_FORMAT; }

    size_t nA = (size_t)lc.rank    * (size_t)lc.in_dim;
    size_t nB = (size_t)lc.out_dim * (size_t)lc.rank;
    const uint8_t *wp = buf + HDR_BYTES;
    if (host_is_little()) {
        memcpy(out->A, wp,                    nA * sizeof(float));
        memcpy(out->B, wp + nA * sizeof(float), nB * sizeof(float));
    } else {
        for (size_t i = 0; i < nA; i++) out->A[i] = get_f32(wp + i*4);
        wp += nA * 4;
        for (size_t i = 0; i < nB; i++) out->B[i] = get_f32(wp + i*4);
    }

    if (info) {
        *info = lc;
        info->bytes_weights = wbytes;
        memcpy(info->mac, buf + payload_len, COS_LORA_EXPORT_MAC_BYTES);
    }
    free(buf);
    return COS_LORA_EXPORT_OK;
}

/* ------------------------ self-test ------------------------ */

int cos_lora_export_self_test(void) {
    cos_lora_adapter_t a, b;
    if (cos_lora_init(&a, "round-trip", 8, 4, 4, 16.0f,
                      0xC05C05C0C05C05C0ULL) != COS_LORA_OK)
        return -1;
    /* Run a few training steps so B has non-zero content. */
    float x[8]  = { 0.1f,-0.2f, 0.3f,-0.4f, 0.5f,-0.6f, 0.7f,-0.8f };
    float t[4]  = { 0.5f, 0.25f, 0.0f, -0.5f };
    float zero[4] = {0,0,0,0};
    for (int i = 0; i < 50; i++) cos_lora_train_step(&a, x, t, zero, 0.10f, NULL);

    char path[] = "/tmp/cos-lora-export-XXXXXX.cos";
    int fd = mkstemps(path, 4);
    if (fd < 0) return -2;
    close(fd);

    cos_lora_export_meta_t meta = {
        .description = "self-test adapter",
        .trained_by  = "self_test",
        .benchmark_sigma = 0.12f,
        .created_unix    = 1700000000ULL,
    };
    if (cos_lora_export_write(&a, &meta, path) != COS_LORA_EXPORT_OK) {
        cos_lora_free(&a); remove(path); return -3;
    }

    cos_lora_export_info_t info;
    if (cos_lora_export_peek(path, &info) != COS_LORA_EXPORT_OK) {
        cos_lora_free(&a); remove(path); return -4;
    }
    if (info.in_dim != a.in_dim || info.out_dim != a.out_dim ||
        info.rank   != a.rank) { cos_lora_free(&a); remove(path); return -5; }

    if (cos_lora_export_read(path, &b, &info) != COS_LORA_EXPORT_OK) {
        cos_lora_free(&a); remove(path); return -6;
    }

    /* Weights must round-trip byte-for-byte. */
    size_t nA = (size_t)a.rank    * (size_t)a.in_dim;
    size_t nB = (size_t)a.out_dim * (size_t)a.rank;
    for (size_t i = 0; i < nA; i++) if (a.A[i] != b.A[i]) { cos_lora_free(&a); cos_lora_free(&b); remove(path); return -7; }
    for (size_t i = 0; i < nB; i++) if (a.B[i] != b.B[i]) { cos_lora_free(&a); cos_lora_free(&b); remove(path); return -8; }

    /* Tamper detection: flip a byte past the header, expect MAC failure. */
    FILE *fp = fopen(path, "r+b");
    if (!fp) { cos_lora_free(&a); cos_lora_free(&b); remove(path); return -9; }
    fseek(fp, HDR_BYTES + 2, SEEK_SET);
    uint8_t v; fread(&v, 1, 1, fp); v ^= 0xFFu;
    fseek(fp, HDR_BYTES + 2, SEEK_SET); fwrite(&v, 1, 1, fp);
    fclose(fp);

    cos_lora_adapter_t c;
    int rc = cos_lora_export_read(path, &c, &info);
    if (rc != COS_LORA_EXPORT_ERR_MAC) {
        cos_lora_free(&a); cos_lora_free(&b); remove(path); return -10;
    }

    cos_lora_free(&a);
    cos_lora_free(&b);
    remove(path);
    return 0;
}
