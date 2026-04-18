/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v118 σ-Vision placeholder implementation.
 */

#include "vision.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Base64 decode (RFC 4648, permissive on whitespace)                  */
/* ------------------------------------------------------------------ */

static int b64_value(unsigned char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+')             return 62;
    if (c == '/')             return 63;
    return -1;
}

static int b64_decode(const char *in, size_t in_len,
                      unsigned char *out, size_t out_cap,
                      size_t *out_len) {
    size_t o = 0;
    int bits = 0, buf = 0;
    for (size_t i = 0; i < in_len; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '=' || c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int v = b64_value(c);
        if (v < 0) return -1;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= out_cap) return -1;
            out[o++] = (unsigned char)((buf >> bits) & 0xFF);
        }
    }
    if (out_len) *out_len = o;
    return 0;
}

/* ------------------------------------------------------------------ */
/* JSON parsing (minimal, just enough for image_url messages)          */
/* ------------------------------------------------------------------ */

static const char *find_substr(const char *hay, size_t n, const char *needle) {
    size_t nl = strlen(needle);
    if (nl == 0 || nl > n) return NULL;
    for (size_t i = 0; i + nl <= n; ++i) {
        if (memcmp(hay + i, needle, nl) == 0) return hay + i;
    }
    return NULL;
}

int cos_v118_parse_image_url(const char *body, size_t body_len,
                             cos_v118_image_t *img,
                             unsigned char *buf, size_t buf_cap,
                             size_t *n_decoded) {
    if (!body || !img) return -1;
    memset(img, 0, sizeof *img);
    if (n_decoded) *n_decoded = 0;

    /* Find `"image_url"` field.  OpenAI nests it, but also accepts the
     * flat `"url": "…"` shape inside `image_url`. */
    const char *hit = find_substr(body, body_len, "\"image_url\"");
    if (!hit) hit = find_substr(body, body_len, "image_url");
    if (!hit) return -1;

    const char *url_key = find_substr(hit, body_len - (size_t)(hit - body),
                                       "\"url\"");
    if (!url_key) return -1;
    const char *colon = strchr(url_key, ':');
    if (!colon) return -1;
    const char *q = strchr(colon, '"');
    if (!q) return -1;
    const char *p = q + 1;
    const char *end = body + body_len;
    size_t o = 0;
    while (p < end && *p != '"') {
        if (*p == '\\' && p + 1 < end) {
            /* Just copy the escape char verbatim for URLs; not semantic. */
            if (o + 2 < sizeof img->url) {
                img->url[o++] = *p;
                img->url[o++] = *(p + 1);
            }
            p += 2;
        } else {
            if (o + 1 < sizeof img->url) img->url[o++] = *p;
            ++p;
        }
    }
    img->url[o < sizeof img->url ? o : sizeof img->url - 1] = '\0';

    /* Determine source + decode base64 if needed. */
    if (!strncmp(img->url, "data:", 5)) {
        img->source = COS_V118_SRC_DATA_URL;
        const char *comma = strchr(img->url, ',');
        if (!comma) return -1;
        const char *payload = comma + 1;
        size_t pl = strlen(payload);
        if (buf && buf_cap > 0) {
            size_t n = 0;
            if (b64_decode(payload, pl, buf, buf_cap, &n) != 0) return -1;
            img->bytes   = buf;
            img->n_bytes = n;
            if (n_decoded) *n_decoded = n;
        }
        return 0;
    }
    if (!strncmp(img->url, "http://",  7) ||
        !strncmp(img->url, "https://", 8)) {
        img->source = COS_V118_SRC_HTTP_URL;
        /* v118 does not fetch; caller populates bytes/n_bytes if desired. */
        return 0;
    }
    img->source = COS_V118_SRC_UNKNOWN;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Placeholder encoder + projection + σ                                */
/* ------------------------------------------------------------------ */

static uint32_t fnv32(const unsigned char *p, size_t n) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* Mix counter with seed for a sub-stream of deterministic pseudo-
 * random values in [-1, 1). */
static float prng_unit(uint32_t seed, uint32_t i) {
    uint32_t x = seed ^ (i * 0x9e3779b9u);
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    /* Map to [-1, 1). */
    return ((float)(int32_t)x) / 2147483648.0f;
}

int cos_v118_project(const cos_v118_image_t *img, float tau_vision,
                     cos_v118_projection_result_t *out,
                     float *embedding_out) {
    if (!img || !out) return -1;
    memset(out, 0, sizeof *out);
    out->embedding_dim = COS_V118_EMBED_DIM;
    out->tau_vision    = tau_vision > 0.f ? tau_vision : 0.55f;

    if (!img->bytes || img->n_bytes == 0) {
        /* No decoded bytes available.  Signal high σ + abstain so
         * callers don't silently pretend we processed an image. */
        out->sigma_product = 0.95f;
        out->abstained     = 1;
        snprintf(out->projection_channel, sizeof out->projection_channel,
                 "no_bytes");
        return 0;
    }

    uint32_t seed = fnv32(img->bytes, img->n_bytes);
    out->content_hash = seed;

    /* Build a 2048-d pseudo-embedding: phase-locked sinusoid
     * modulated by the content hash, plus bucketed byte histogram
     * (frequency contribution), L2-normalised. */
    float *emb = embedding_out;
    float  local[COS_V118_EMBED_DIM];
    if (!emb) emb = local;

    /* Byte histogram (256 buckets). */
    uint32_t hist[256] = {0};
    for (size_t i = 0; i < img->n_bytes; ++i) hist[img->bytes[i]]++;

    for (int k = 0; k < COS_V118_EMBED_DIM; ++k) {
        float base = prng_unit(seed, (uint32_t)k);
        float freq = 0.f;
        if (img->n_bytes > 0) {
            int bucket = k & 0xFF;
            freq = (float)hist[bucket] / (float)img->n_bytes;  /* [0,1] */
        }
        /* Combine: projection = base + 2*(freq-0.5) scaled. */
        emb[k] = base + (freq - (1.0f/256.0f)) * 4.0f;
    }
    /* L2-normalise. */
    double ss = 0.0;
    for (int k = 0; k < COS_V118_EMBED_DIM; ++k) ss += (double)emb[k] * emb[k];
    float inv = (ss > 0.0) ? (float)(1.0 / sqrt(ss)) : 0.f;
    for (int k = 0; k < COS_V118_EMBED_DIM; ++k) emb[k] *= inv;

    /* σ on the projection: inverse coefficient-of-variation of the
     * histogram — a flat histogram (random bytes) → high σ, a peaky
     * histogram (real image with structure) → lower σ.
     *
     * Compute entropy H of the histogram; normalise by log(256).
     * H_norm ∈ [0,1]; 1 = uniform; σ_product ≈ H_norm is then a
     * sensible out-of-distribution detector for a placeholder
     * encoder.  Bounded to [0.05, 0.98] for numerical sanity. */
    double H = 0.0;
    double n = (double)img->n_bytes;
    for (int b = 0; b < 256; ++b) {
        if (hist[b] == 0) continue;
        double p = (double)hist[b] / n;
        H -= p * log(p);
    }
    double H_norm = H / log(256.0);            /* 0..1 */
    if (H_norm < 0.0) H_norm = 0.0;
    if (H_norm > 1.0) H_norm = 1.0;

    float sigma = (float)H_norm;
    if (sigma < 0.05f) sigma = 0.05f;
    if (sigma > 0.98f) sigma = 0.98f;
    out->sigma_product = sigma;
    out->abstained     = sigma > out->tau_vision;
    if (out->abstained) {
        snprintf(out->projection_channel, sizeof out->projection_channel,
                 "embedding_entropy");
    } else {
        snprintf(out->projection_channel, sizeof out->projection_channel,
                 "embedding_entropy_ok");
    }

    for (int i = 0; i < 8; ++i) out->preview[i] = emb[i];
    return 0;
}

int cos_v118_result_to_json(const cos_v118_projection_result_t *r,
                            char *out, size_t cap) {
    if (!r || !out || cap == 0) return -1;
    size_t o = 0;
    int n = snprintf(out, cap,
        "{\"sigma_product\":%.6f,\"tau_vision\":%.6f,"
        "\"abstained\":%s,\"embedding_dim\":%d,"
        "\"projection_channel\":\"%s\","
        "\"content_hash\":\"%08x\","
        "\"preview\":[",
        (double)r->sigma_product, (double)r->tau_vision,
        r->abstained ? "true" : "false",
        r->embedding_dim,
        r->projection_channel, r->content_hash);
    if (n < 0 || (size_t)n >= cap) return -1;
    o = (size_t)n;
    for (int i = 0; i < 8; ++i) {
        int w = snprintf(out + o, cap - o, "%s%.5f", i ? "," : "",
                         (double)r->preview[i]);
        if (w < 0 || (size_t)w >= cap - o) return -1;
        o += (size_t)w;
    }
    if (o + 3 >= cap) return -1;
    out[o++] = ']';
    out[o++] = '}';
    out[o]   = '\0';
    return (int)o;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v118 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v118_self_test(void) {
    /* 1) Data-URL parse round-trip.  Payload: 6 bytes "HELLO\n" →
     *    base64 "SEVMTE9K" -> wait: HELLO\n = 48,45,4c,4c,4f,0a
     *    base64: SEVMTE8K  (verify: S=48 E=4545 … let python compute
     *    — just use a known-good pair). */
    const char *body =
        "{\"type\":\"image_url\","
         "\"image_url\":{\"url\":\"data:image/png;base64,SEVMTE8K\"}}";
    cos_v118_image_t img;
    unsigned char buf[32];
    size_t n = 0;
    int rc = cos_v118_parse_image_url(body, strlen(body), &img,
                                       buf, sizeof buf, &n);
    _CHECK(rc == 0, "parse data url");
    _CHECK(img.source == COS_V118_SRC_DATA_URL, "source = data url");
    _CHECK(n == 6, "decoded 6 bytes from HELLO\\n");
    _CHECK(buf[0] == 'H' && buf[1] == 'E' && buf[2] == 'L' &&
           buf[3] == 'L' && buf[4] == 'O' && buf[5] == '\n',
           "decoded content matches HELLO\\n");

    /* 2) Projection on the 6-byte image: σ should be bounded and
     *    content_hash deterministic. */
    cos_v118_projection_result_t r;
    float emb[COS_V118_EMBED_DIM];
    rc = cos_v118_project(&img, 0.55f, &r, emb);
    _CHECK(rc == 0, "project ok");
    _CHECK(r.embedding_dim == COS_V118_EMBED_DIM, "embedding dim");
    _CHECK(r.sigma_product >= 0.05f && r.sigma_product <= 0.98f,
           "σ bounded");
    /* Finite check */
    for (int i = 0; i < 8; ++i) {
        _CHECK(r.preview[i] == r.preview[i], "preview finite");
    }

    /* 3) Uniform byte stream should produce HIGH σ → abstain. */
    unsigned char uniform[4096];
    for (size_t i = 0; i < sizeof uniform; ++i)
        uniform[i] = (unsigned char)(i & 0xFF);      /* perfect flat hist */
    cos_v118_image_t img2 = {0};
    img2.source  = COS_V118_SRC_RAW_BYTES;
    img2.bytes   = uniform;
    img2.n_bytes = sizeof uniform;
    cos_v118_projection_result_t r2;
    rc = cos_v118_project(&img2, 0.55f, &r2, NULL);
    _CHECK(rc == 0, "project uniform");
    _CHECK(r2.sigma_product >= 0.95f, "uniform → high σ");
    _CHECK(r2.abstained == 1, "uniform → abstain");
    _CHECK(strcmp(r2.projection_channel, "embedding_entropy") == 0,
           "abstain channel = embedding_entropy");

    /* 4) Peaky histogram (one byte dominates) should produce LOW σ. */
    unsigned char peaky[4096];
    for (size_t i = 0; i < sizeof peaky; ++i)
        peaky[i] = (i % 100 == 0) ? (unsigned char)(i & 0xFF) : 0;
    cos_v118_image_t img3 = {0};
    img3.source  = COS_V118_SRC_RAW_BYTES;
    img3.bytes   = peaky;
    img3.n_bytes = sizeof peaky;
    cos_v118_projection_result_t r3;
    rc = cos_v118_project(&img3, 0.55f, &r3, NULL);
    _CHECK(rc == 0, "project peaky");
    _CHECK(r3.sigma_product < 0.55f, "peaky → low σ (below τ)");
    _CHECK(r3.abstained == 0, "peaky does not abstain");

    /* 5) JSON shape carries all required fields. */
    char js[1024];
    int jn = cos_v118_result_to_json(&r2, js, sizeof js);
    _CHECK(jn > 0, "json serialize");
    _CHECK(strstr(js, "\"abstained\":true")       != NULL, "json abstained");
    _CHECK(strstr(js, "\"embedding_dim\":2048")   != NULL, "json embedding_dim");
    _CHECK(strstr(js, "\"projection_channel\"")   != NULL, "json channel");
    _CHECK(strstr(js, "\"content_hash\"")         != NULL, "json content_hash");
    _CHECK(strstr(js, "\"preview\":[")            != NULL, "json preview array");

    /* 6) http(s) URL detected but not fetched. */
    const char *body_http =
        "{\"image_url\":{\"url\":\"https://example.com/cat.png\"}}";
    rc = cos_v118_parse_image_url(body_http, strlen(body_http), &img,
                                   NULL, 0, NULL);
    _CHECK(rc == 0, "parse http url");
    _CHECK(img.source == COS_V118_SRC_HTTP_URL, "http source");
    _CHECK(img.n_bytes == 0, "http url: no bytes fetched by v118");

    return 0;
}
