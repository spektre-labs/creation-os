/*
 * σ-arxiv — implementation.  Includes a self-contained SHA-256 so
 * the kernel builds with only libc + libm.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "arxiv.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ===============================================================
 * SHA-256 (NIST FIPS 180-4) — compact public-domain-style
 * implementation.  Purely for anchor-file fingerprinting; NOT a
 * cryptographic trust boundary in Creation OS (Ed25519 is the
 * trust layer).
 * =============================================================== */

static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2,
};

#define ROTR(x,n) (((x) >> (n)) | ((x) << (32 - (n))))

typedef struct {
    uint32_t h[8];
    uint64_t len;      /* bits */
    uint8_t  buf[64];
    size_t   nbuf;
} sha256_ctx;

static void sha256_init(sha256_ctx *c) {
    static const uint32_t iv[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    memcpy(c->h, iv, sizeof iv);
    c->len = 0;
    c->nbuf = 0;
}

static void sha256_block(sha256_ctx *c, const uint8_t *p) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)p[4*i] << 24) | ((uint32_t)p[4*i+1] << 16) |
               ((uint32_t)p[4*i+2] << 8) | (uint32_t)p[4*i+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROTR(w[i-15],7) ^ ROTR(w[i-15],18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROTR(w[i-2],17) ^ ROTR(w[i-2],19)  ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=c->h[0],b=c->h[1],cc=c->h[2],d=c->h[3];
    uint32_t e=c->h[4],f=c->h[5],g=c->h[6],h=c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25);
        uint32_t ch = (e & f) ^ ((~e) & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22);
        uint32_t mj = (a & b) ^ (a & cc) ^ (b & cc);
        uint32_t t2 = S0 + mj;
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0]+=a; c->h[1]+=b; c->h[2]+=cc; c->h[3]+=d;
    c->h[4]+=e; c->h[5]+=f; c->h[6]+=g;  c->h[7]+=h;
}

static void sha256_update(sha256_ctx *c, const void *data, size_t n) {
    const uint8_t *p = (const uint8_t *)data;
    c->len += (uint64_t)n * 8;
    while (n > 0) {
        size_t room = 64 - c->nbuf;
        size_t take = n < room ? n : room;
        memcpy(c->buf + c->nbuf, p, take);
        c->nbuf += take; p += take; n -= take;
        if (c->nbuf == 64) { sha256_block(c, c->buf); c->nbuf = 0; }
    }
}

static void sha256_final(sha256_ctx *c, uint8_t out[32]) {
    uint64_t total_bits = c->len;
    c->buf[c->nbuf++] = 0x80;
    if (c->nbuf > 56) {
        while (c->nbuf < 64) c->buf[c->nbuf++] = 0;
        sha256_block(c, c->buf); c->nbuf = 0;
    }
    while (c->nbuf < 56) c->buf[c->nbuf++] = 0;
    for (int i = 7; i >= 0; i--) c->buf[c->nbuf++] = (uint8_t)(total_bits >> (i*8));
    sha256_block(c, c->buf);
    for (int i = 0; i < 8; i++) {
        out[4*i]   = (uint8_t)(c->h[i] >> 24);
        out[4*i+1] = (uint8_t)(c->h[i] >> 16);
        out[4*i+2] = (uint8_t)(c->h[i] >> 8);
        out[4*i+3] = (uint8_t) c->h[i];
    }
}

static int sha256_file(const char *path, char hex[COS_ARXIV_HEX_LEN],
                       long *bytes_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return -1;
    sha256_ctx ctx;
    sha256_init(&ctx);
    uint8_t buf[8192];
    long total = 0;
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, fp)) > 0) {
        sha256_update(&ctx, buf, n);
        total += (long)n;
    }
    fclose(fp);
    uint8_t digest[32];
    sha256_final(&ctx, digest);
    static const char *tbl = "0123456789abcdef";
    for (int i = 0; i < 32; i++) {
        hex[2*i]   = tbl[(digest[i] >> 4) & 0xF];
        hex[2*i+1] = tbl[digest[i] & 0xF];
    }
    hex[64] = '\0';
    if (bytes_out) *bytes_out = total;
    return 0;
}

/* ===============================================================
 * Manifest
 * =============================================================== */

static const char *ANCHOR_PATHS[] = {
    "docs/papers/creation_os_v1.md",
    "docs/papers/creation_os_v1.tex",
    "CHANGELOG.md",
    "include/cos_version.h",
    "LICENSE",
};

int cos_arxiv_manifest_build(cos_arxiv_manifest_t *m) {
    if (!m) return -1;
    memset(m, 0, sizeof *m);
    m->title        = "Creation OS: σ-Gated Sovereign Inference "
                      "with Formal Guarantees and Substrate-Agnostic Design";
    m->author       = "Lauri Elias Rainio";
    m->orcid        = "0009-0006-0903-8541";
    m->affiliation  = "Spektre Labs, Helsinki";
    m->category     = "cs.LG";
    m->abstract     = "We present Creation OS, a σ-gated sovereign-inference "
                      "operating system whose kernel enforces "
                      "declared==realized as a measurable runtime invariant, "
                      "with a partial formal backing in Lean 4 + Frama-C/Wp "
                      "and a substrate-agnostic design spanning CPU, "
                      "neuromorphic, and photonic back-ends.";
    m->license      = "SCSL-1.0 OR AGPL-3.0-only";
    m->version_tag  = "v2.0.0-omega";
    m->doi_reserved = "10.5281/zenodo.RESERVED-ON-SUBMIT";

    int n = (int)(sizeof ANCHOR_PATHS / sizeof ANCHOR_PATHS[0]);
    if (n > COS_ARXIV_MAX_ANCHORS) n = COS_ARXIV_MAX_ANCHORS;
    m->n_anchors = n;

    int all_present = 1;
    for (int i = 0; i < n; i++) {
        cos_arxiv_anchor_t *a = &m->anchors[i];
        size_t pl = strlen(ANCHOR_PATHS[i]);
        if (pl >= sizeof a->path) pl = sizeof a->path - 1;
        memcpy(a->path, ANCHOR_PATHS[i], pl);
        a->path[pl] = '\0';
        if (sha256_file(ANCHOR_PATHS[i], a->sha256_hex, &a->bytes) == 0) {
            a->exists = 1;
        } else {
            a->exists = 0;
            memset(a->sha256_hex, 0, sizeof a->sha256_hex);
            strcpy(a->sha256_hex, "MISSING");
            a->bytes = 0;
            all_present = 0;
        }
    }
    m->anchors_present = all_present;
    return 0;
}

int cos_arxiv_manifest_write_json(const cos_arxiv_manifest_t *m,
                                  char *buf, size_t cap) {
    if (!m || !buf || cap == 0) return -1;
    size_t off = 0;
    int w;
    w = snprintf(buf + off, cap - off,
        "{\n"
        "  \"kernel\": \"sigma_arxiv\",\n"
        "  \"metadata\": {\n"
        "    \"title\": \"%s\",\n"
        "    \"author\": \"%s\",\n"
        "    \"orcid\": \"%s\",\n"
        "    \"affiliation\": \"%s\",\n"
        "    \"category\": \"%s\",\n"
        "    \"license\": \"%s\",\n"
        "    \"version_tag\": \"%s\",\n"
        "    \"doi_reserved\": \"%s\",\n"
        "    \"abstract\": \"%s\"\n"
        "  },\n"
        "  \"anchors_present\": %s,\n"
        "  \"anchors\": [\n",
        m->title, m->author, m->orcid, m->affiliation,
        m->category, m->license, m->version_tag, m->doi_reserved,
        m->abstract, m->anchors_present ? "true" : "false");
    if (w < 0 || (size_t)w >= cap - off) return -1;
    off += (size_t)w;

    for (int i = 0; i < m->n_anchors; i++) {
        const cos_arxiv_anchor_t *a = &m->anchors[i];
        w = snprintf(buf + off, cap - off,
            "    { \"path\": \"%s\", \"bytes\": %ld, \"exists\": %s,"
            " \"sha256\": \"%s\" }%s\n",
            a->path, a->bytes, a->exists ? "true" : "false",
            a->sha256_hex,
            i + 1 == m->n_anchors ? "" : ",");
        if (w < 0 || (size_t)w >= cap - off) return -1;
        off += (size_t)w;
    }
    w = snprintf(buf + off, cap - off, "  ]\n}\n");
    if (w < 0 || (size_t)w >= cap - off) return -1;
    off += (size_t)w;
    return (int)off;
}

/* ===============================================================
 * Self-test — verify metadata present + all required anchors
 * exist in the tree.
 * =============================================================== */
int cos_arxiv_self_test(void) {
    cos_arxiv_manifest_t m;
    cos_arxiv_manifest_build(&m);
    if (!m.title || strlen(m.title) < 30)           return -1;
    if (!m.orcid || strlen(m.orcid) < 10)            return -2;
    if (strcmp(m.category, "cs.LG") != 0)            return -3;
    if (m.n_anchors < 4)                              return -4;
    if (!m.anchors_present)                           return -5;
    for (int i = 0; i < m.n_anchors; i++) {
        if (!m.anchors[i].exists)                     return -6;
        if (strlen(m.anchors[i].sha256_hex) != 64)    return -7;
        if (m.anchors[i].bytes <= 0)                  return -8;
    }
    /* Quick SHA-256 known-answer sanity: empty-string digest. */
    sha256_ctx c; sha256_init(&c);
    uint8_t out[32];
    sha256_final(&c, out);
    static const uint8_t expected[32] = {
        0xe3,0xb0,0xc4,0x42,0x98,0xfc,0x1c,0x14,
        0x9a,0xfb,0xf4,0xc8,0x99,0x6f,0xb9,0x24,
        0x27,0xae,0x41,0xe4,0x64,0x9b,0x93,0x4c,
        0xa4,0x95,0x99,0x1b,0x78,0x52,0xb8,0x55
    };
    if (memcmp(out, expected, 32) != 0)               return -9;
    return 0;
}
