/*
 * Proof receipt implementation — SHA-256 via license kernel (spektre_sha256).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "proof_receipt.h"
#include "license_attest.h"
#include "reinforce.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static int mkdir_p(const char *dir);

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}

static void le64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        p[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}

static void float_bits(uint8_t *p, float f)
{
    union {
        float    f;
        uint32_t u;
    } u;
    u.f = f;
    le32(p, u.u);
}

static size_t marshal_preimage(const struct cos_proof_receipt *r, uint8_t *buf,
                               size_t cap)
{
    size_t off = 0;
    if (cap < 308)
        return 0;
    memcpy(buf + off, r->codex_hash, 32);
    off += 32;
    memcpy(buf + off, r->license_hash, 32);
    off += 32;
    memcpy(buf + off, r->output_hash, 32);
    off += 32;
    memcpy(buf + off, r->prev_receipt_hash, 32);
    off += 32;

    float_bits(buf + off, r->sigma_combined);
    off += 4;
    for (int i = 0; i < 4; ++i) {
        float_bits(buf + off, r->sigma_channels[i]);
        off += 4;
    }
    for (int i = 0; i < 4; ++i) {
        float_bits(buf + off, r->meta_channels[i]);
        off += 4;
    }

    le32(buf + off, (uint32_t)r->gate_decision);
    off += 4;
    le32(buf + off, (uint32_t)r->attribution);
    off += 4;

    le32(buf + off, (uint32_t)r->injection_detected);
    off += 4;
    le32(buf + off, (uint32_t)r->risk_level);
    off += 4;
    le32(buf + off, (uint32_t)r->sovereign_brake);
    off += 4;
    le32(buf + off, (uint32_t)r->within_compute_budget);
    off += 4;

    le64(buf + off, r->kernel_pass_bitmap);
    off += 8;
    le32(buf + off, (uint32_t)r->kernels_run);
    off += 4;
    le32(buf + off, (uint32_t)r->kernels_passed);
    off += 4;

    memset(buf + off, 0, 64);
    memcpy(buf + off, r->model_id,
           strnlen(r->model_id, sizeof r->model_id));
    off += 64;

    memset(buf + off, 0, 32);
    memcpy(buf + off, r->codex_version,
           strnlen(r->codex_version, sizeof r->codex_version));
    off += 32;

    le64(buf + off, (uint64_t)r->timestamp_ms);
    off += 8;
    return off;
}

static void compute_receipt_hash(struct cos_proof_receipt *r)
{
    uint8_t buf[384];
    size_t  n = marshal_preimage(r, buf, sizeof buf);
    spektre_sha256(buf, n, r->receipt_hash);
}

static int receipts_dir(char *out, size_t cap)
{
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
        return -1;
    snprintf(out, cap, "%s/.cos/receipts", home);
    return 0;
}

static int chain_prev_path(char *out, size_t cap)
{
    if (receipts_dir(out, cap) != 0)
        return -1;
    size_t n = strlen(out);
    if (n + 16 >= cap)
        return -1;
    snprintf(out + n, cap - n, "/.chain_prev");
    return 0;
}

static void read_chain_prev(uint8_t prev[32])
{
    char path[512];
    FILE *fp;
    memset(prev, 0, 32);
    if (chain_prev_path(path, sizeof path) != 0)
        return;
    fp = fopen(path, "rb");
    if (fp == NULL)
        return;
    if (fread(prev, 1, 32, fp) != 32)
        memset(prev, 0, 32);
    fclose(fp);
}

static int write_chain_prev(const uint8_t h[32])
{
    char dir[512];
    char path[512];
    FILE *fp;

    if (receipts_dir(dir, sizeof dir) != 0)
        return -1;
    if (mkdir_p(dir) != 0 && errno != EEXIST)
        return -4;

    if (chain_prev_path(path, sizeof path) != 0)
        return -1;
    fp = fopen(path, "wb");
    if (fp == NULL)
        return -2;
    if (fwrite(h, 1, 32, fp) != 32) {
        fclose(fp);
        return -3;
    }
    fclose(fp);
    return 0;
}

static void hash_codex_fnv(uint64_t fnv, uint8_t out[32])
{
    spektre_sha256(&fnv, sizeof fnv, out);
}

int cos_proof_receipt_generate_with(
    const char *output_text,
    float sigma_combined,
    const float sigma_channels[4],
    const float meta_channels[4],
    int gate_decision,
    const struct cos_error_attribution *attr,
    uint64_t kernel_bitmap,
    const struct cos_proof_receipt_options *opts,
    struct cos_proof_receipt *receipt)
{
    struct timespec ts;
    const char *mt = "";
    const char *cv = "";
    int64_t     tms = 0;

    if (receipt == NULL)
        return -1;

    memset(receipt, 0, sizeof(*receipt));

    if (output_text == NULL)
        output_text = "";

    if (opts != NULL) {
        hash_codex_fnv(opts->codex_fnv64, receipt->codex_hash);
        receipt->injection_detected   = opts->injection_detected;
        receipt->risk_level           = opts->risk_level;
        receipt->sovereign_brake      = opts->sovereign_brake;
        receipt->within_compute_budget = opts->within_compute_budget;
        receipt->kernels_run          = opts->kernels_run;
        mt                            = opts->model_id ? opts->model_id : "";
        cv                            = opts->codex_version ? opts->codex_version
                                                            : "";
        tms                           = opts->timestamp_ms;
        if (opts->constitutional_valid) {
            receipt->constitutional_valid           = 1;
            receipt->constitutional_compliant       = opts->constitutional_compliant;
            receipt->constitutional_checks          = opts->constitutional_checks;
            receipt->constitutional_violations      = opts->constitutional_violations;
            receipt->constitutional_mandatory_halts = opts->constitutional_mandatory_halts;
        }
    } else {
        receipt->kernels_run = 40;
        receipt->within_compute_budget = 1;
    }

    memcpy(receipt->license_hash, spektre_license_sha256_bin, 32);
    spektre_sha256(output_text, strlen(output_text), receipt->output_hash);

    read_chain_prev(receipt->prev_receipt_hash);

    receipt->sigma_combined = sigma_combined;
    if (sigma_channels != NULL) {
        memcpy(receipt->sigma_channels, sigma_channels,
               sizeof receipt->sigma_channels);
    } else {
        receipt->sigma_channels[0] = sigma_combined;
        receipt->sigma_channels[1] = sigma_combined;
        receipt->sigma_channels[2] = sigma_combined;
        receipt->sigma_channels[3] = 0.f;
    }

    if (meta_channels != NULL)
        memcpy(receipt->meta_channels, meta_channels,
               sizeof receipt->meta_channels);

    receipt->gate_decision = gate_decision;
    receipt->attribution =
        attr ? attr->source : COS_ERR_NONE;

    if (opts == NULL || opts->kernels_run <= 0)
        receipt->kernels_run = 40;

    receipt->kernel_pass_bitmap = kernel_bitmap & 0xFFFFFFFFFFULL;

    {
        uint64_t m = receipt->kernel_pass_bitmap;
        int      c = 0;
        while (m) {
            c += (int)(m & 1);
            m >>= 1;
        }
        receipt->kernels_passed = c;
    }

    snprintf(receipt->model_id, sizeof receipt->model_id, "%s", mt);
    snprintf(receipt->codex_version, sizeof receipt->codex_version, "%s",
             cv);

    if (tms > 0)
        receipt->timestamp_ms = tms;
    else if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        receipt->timestamp_ms =
            (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
    else
        receipt->timestamp_ms = (int64_t)time(NULL) * 1000LL;

    compute_receipt_hash(receipt);
    return 0;
}

int cos_proof_receipt_generate(
    const char *output_text,
    float sigma_combined,
    const float sigma_channels[4],
    const float meta_channels[4],
    int gate_decision,
    const struct cos_error_attribution *attr,
    uint64_t kernel_bitmap,
    struct cos_proof_receipt *receipt)
{
    return cos_proof_receipt_generate_with(output_text, sigma_combined,
                                           sigma_channels, meta_channels,
                                           gate_decision, attr,
                                           kernel_bitmap, NULL, receipt);
}

int cos_proof_receipt_verify(const struct cos_proof_receipt *receipt)
{
    uint8_t expect[32];
    uint8_t buf[384];
    size_t  n;

    if (receipt == NULL)
        return -1;
    n = marshal_preimage(receipt, buf, sizeof buf);
    if (n != 308)
        return -2;
    spektre_sha256(buf, n, expect);
    return spektre_ct_memcmp(expect, receipt->receipt_hash, 32);
}

char *cos_proof_receipt_to_json(const struct cos_proof_receipt *r)
{
    char *out;
    char  hx[65];
    char  prev[65];
    char  rh[65];
    char  ox[65];
    char  cx[65];
    char  lx[65];
    int   n;

    if (r == NULL)
        return NULL;
    out = malloc(2560);
    if (out == NULL)
        return NULL;

    spektre_hex_lower(r->receipt_hash, rh);
    spektre_hex_lower(r->prev_receipt_hash, prev);
    spektre_hex_lower(r->output_hash, ox);
    spektre_hex_lower(r->codex_hash, cx);
    spektre_hex_lower(r->license_hash, lx);

    n = snprintf(out, 2560,
                 "{\"proof_receipt\":{"
                 "\"receipt_hash\":\"%s\","
                 "\"prev_receipt_hash\":\"%s\","
                 "\"codex_hash\":\"%s\","
                 "\"license_hash\":\"%s\","
                 "\"output_hash\":\"%s\","
                 "\"sigma_combined\":%.6g,"
                 "\"sigma_channels\":[%.6g,%.6g,%.6g,%.6g],"
                 "\"meta_channels\":[%.6g,%.6g,%.6g,%.6g],"
                 "\"gate_decision\":%d,"
                 "\"attribution\":%d,"
                 "\"injection_detected\":%d,"
                 "\"risk_level\":%d,"
                 "\"sovereign_brake\":%d,"
                 "\"within_compute_budget\":%d,"
                 "\"kernel_pass_bitmap\":\"%016llx\","
                 "\"kernels_run\":%d,"
                 "\"kernels_passed\":%d,"
                 "\"model_id\":\"%s\","
                 "\"codex_version\":\"%s\","
                 "\"timestamp_ms\":%lld,"
                 "\"constitutional_valid\":%d,"
                 "\"constitutional_compliant\":%d,"
                 "\"constitutional_checks\":%d,"
                 "\"constitutional_violations\":%d,"
                 "\"constitutional_mandatory_halts\":%d}}",
                 rh, prev, cx, lx, ox, (double)r->sigma_combined,
                 (double)r->sigma_channels[0], (double)r->sigma_channels[1],
                 (double)r->sigma_channels[2], (double)r->sigma_channels[3],
                 (double)r->meta_channels[0], (double)r->meta_channels[1],
                 (double)r->meta_channels[2], (double)r->meta_channels[3],
                 r->gate_decision, (int)r->attribution,
                 r->injection_detected, r->risk_level, r->sovereign_brake,
                 r->within_compute_budget,
                 (unsigned long long)r->kernel_pass_bitmap, r->kernels_run,
                 r->kernels_passed, r->model_id, r->codex_version,
                 (long long)r->timestamp_ms, r->constitutional_valid,
                 r->constitutional_compliant, r->constitutional_checks,
                 r->constitutional_violations, r->constitutional_mandatory_halts);
    if (n < 0 || (size_t)n >= 2560) {
        free(out);
        return NULL;
    }
    (void)hx;
    return out;
}

int cos_proof_receipt_persist(const struct cos_proof_receipt *receipt,
                              const char *path)
{
    char    *js;
    FILE    *fp;
    int      rc;

    if (path == NULL || path[0] == '\0')
        return -1;
    js = cos_proof_receipt_to_json(receipt);
    if (js == NULL)
        return -2;
    fp = fopen(path, "w");
    if (fp == NULL) {
        free(js);
        return -3;
    }
    fputs(js, fp);
    fputc('\n', fp);
    fclose(fp);
    free(js);
    rc = write_chain_prev(receipt->receipt_hash);
    return rc != 0 ? rc : 0;
}

static int mkdir_p(const char *dir)
{
    char tmp[512];
    snprintf(tmp, sizeof tmp, "%s", dir);
    for (char *p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (tmp[0] != '\0')
                (void)mkdir(tmp, 0700);
            *p = '/';
        }
    }
    return mkdir(tmp, 0700);
}

int cos_proof_receipt_persist_chain(struct cos_proof_receipt *receipt)
{
    char         dir[512];
    char         path[640];
    char         hex9[9];
    int64_t      ts = receipt->timestamp_ms;

    if (receipts_dir(dir, sizeof dir) != 0)
        return -1;
    if (mkdir_p(dir) != 0 && errno != EEXIST)
        return -2;

    snprintf(hex9, sizeof hex9, "%08x",
             (unsigned)(receipt->receipt_hash[0] << 24
                        | receipt->receipt_hash[1] << 16
                        | receipt->receipt_hash[2] << 8
                        | receipt->receipt_hash[3]));

    snprintf(path, sizeof path, "%s/rec_%lld_%s.json", dir,
             (long long)ts, hex9);
    return cos_proof_receipt_persist(receipt, path);
}

static int hex_byte(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F')
        return 10 + (c - 'A');
    return -1;
}

static int parse_hex32(const char *json, const char *key, uint8_t out[32])
{
    char        pat[80];
    const char *s;
    int    i;

    snprintf(pat, sizeof pat, "\"%s\":\"", key);
    s = strstr(json, pat);
    if (s == NULL)
        return -1;
    s += strlen(pat);
    for (i = 0; i < 64; i += 2) {
        int hi = hex_byte(s[i]);
        int lo = hex_byte(s[i + 1]);
        if (hi < 0 || lo < 0)
            return -2;
        out[i / 2] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

static int parse_json_receipt(const char *json, struct cos_proof_receipt *r)
{
    const char *p;
    memset(r, 0, sizeof(*r));
    if (parse_hex32(json, "receipt_hash", r->receipt_hash) != 0)
        return -1;
    if (parse_hex32(json, "prev_receipt_hash", r->prev_receipt_hash) != 0)
        return -2;
    if (parse_hex32(json, "codex_hash", r->codex_hash) != 0)
        return -3;
    if (parse_hex32(json, "license_hash", r->license_hash) != 0)
        return -4;
    if (parse_hex32(json, "output_hash", r->output_hash) != 0)
        return -5;

    p = strstr(json, "\"sigma_combined\":");
    if (p)
        r->sigma_combined = (float)strtod(p + 17, NULL);

    p = strstr(json, "\"gate_decision\":");
    if (p)
        r->gate_decision = atoi(p + 16);

    p = strstr(json, "\"attribution\":");
    if (p)
        r->attribution = (cos_error_source_t)atoi(p + 14);

    p = strstr(json, "\"injection_detected\":");
    if (p)
        r->injection_detected = atoi(p + 23);

    p = strstr(json, "\"risk_level\":");
    if (p)
        r->risk_level = atoi(p + 13);

    p = strstr(json, "\"sovereign_brake\":");
    if (p)
        r->sovereign_brake = atoi(p + 18);

    p = strstr(json, "\"within_compute_budget\":");
    if (p)
        r->within_compute_budget = atoi(p + 25);

    p = strstr(json, "\"kernels_run\":");
    if (p)
        r->kernels_run = atoi(p + 14);

    p = strstr(json, "\"kernels_passed\":");
    if (p)
        r->kernels_passed = atoi(p + 17);

    p = strstr(json, "\"kernel_pass_bitmap\":\"");
    if (p)
        r->kernel_pass_bitmap =
            strtoull(p + 24, NULL, 16);

    p = strstr(json, "\"timestamp_ms\":");
    if (p)
        r->timestamp_ms = (int64_t)strtoll(p + 17, NULL, 10);

    p = strstr(json, "\"constitutional_valid\":");
    if (p)
        r->constitutional_valid = atoi(p + 25);
    p = strstr(json, "\"constitutional_compliant\":");
    if (p)
        r->constitutional_compliant = atoi(p + 30);
    p = strstr(json, "\"constitutional_checks\":");
    if (p)
        r->constitutional_checks = atoi(p + 26);
    p = strstr(json, "\"constitutional_violations\":");
    if (p)
        r->constitutional_violations = atoi(p + 30);
    p = strstr(json, "\"constitutional_mandatory_halts\":");
    if (p)
        r->constitutional_mandatory_halts = atoi(p + 35);

    /* Arrays/model strings: skipped for verify hash — must match writer;
     * full verify uses marshal fields filled above;
     * sigma_channels parsed lazily from JSON requires bracket parse.
     * For chain verify we re-hash from file by full re-parse of arrays:
     * simple sscanf on fixed pattern from our writer. */
    p = strstr(json, "\"sigma_channels\":[");
    if (p) {
        float a, b, c, d;
        if (sscanf(p + 18, "%f,%f,%f,%f", &a, &b, &c, &d) == 4) {
            r->sigma_channels[0] = a;
            r->sigma_channels[1] = b;
            r->sigma_channels[2] = c;
            r->sigma_channels[3] = d;
        }
    }
    p = strstr(json, "\"meta_channels\":[");
    if (p) {
        float a, b, c, d;
        if (sscanf(p + 17, "%f,%f,%f,%f", &a, &b, &c, &d) == 4) {
            r->meta_channels[0] = a;
            r->meta_channels[1] = b;
            r->meta_channels[2] = c;
            r->meta_channels[3] = d;
        }
    }

    {
        char pat[96];
        char buf[96];
        snprintf(pat, sizeof pat, "\"model_id\":\"");
        p = strstr(json, pat);
        if (p) {
            p += strlen(pat);
            if (sscanf(p, "%[^\"]", buf) == 1)
                snprintf(r->model_id, sizeof r->model_id, "%s", buf);
        }
        snprintf(pat, sizeof pat, "\"codex_version\":\"");
        p = strstr(json, pat);
        if (p) {
            p += strlen(pat);
            if (sscanf(p, "%[^\"]", buf) == 1)
                snprintf(r->codex_version, sizeof r->codex_version, "%s",
                         buf);
        }
    }

    return 0;
}

int cos_proof_receipt_chain_verify_dir(const char *dir)
{
    DIR           *d;
    struct dirent *e;
    char           path[1024];
    int            n_files = 0;
    int            ok       = 0;

    d = opendir(dir);
    if (d == NULL)
        return -1;

    while ((e = readdir(d)) != NULL) {
        size_t L;
        if (strncmp(e->d_name, "rec_", 4) != 0)
            continue;
        L = strlen(e->d_name);
        if (L < 8 || strcmp(e->d_name + L - 5, ".json") != 0)
            continue;
        n_files++;
    }
    closedir(d);

    d = opendir(dir);
    if (d == NULL)
        return -1;

    while ((e = readdir(d)) != NULL) {
        FILE *fp;
        char  buf[4096];
        size_t n;
        struct cos_proof_receipt r;

        if (strncmp(e->d_name, "rec_", 4) != 0)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        fp = fopen(path, "r");
        if (fp == NULL)
            continue;
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        buf[n] = '\0';

        if (parse_json_receipt(buf, &r) != 0)
            continue;
        if (cos_proof_receipt_verify(&r) != 0) {
            closedir(d);
            return -2;
        }
        ok++;
    }
    closedir(d);

    if (n_files == 0)
        return 1;
    return ok == n_files ? 0 : -3;
}

void cos_proof_receipt_print_filenames(FILE *fp, const char *dir)
{
    DIR           *d;
    struct dirent *e;

    d = opendir(dir);
    if (d == NULL) {
        fprintf(fp, "(cannot open %s)\n", dir);
        return;
    }
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, "rec_", 4) == 0)
            fprintf(fp, "  %s/%s\n", dir, e->d_name);
    }
    closedir(d);
}

char *cos_proof_receipt_chain_export_jsonl(const char *dir)
{
    DIR           *d;
    struct dirent *e;
    size_t         cap = 8192;
    size_t         len = 0;
    char          *acc = malloc(cap);
    char           path[1024];

    if (acc == NULL)
        return NULL;
    acc[0] = '\0';

    d = opendir(dir);
    if (d == NULL) {
        free(acc);
        return NULL;
    }
    while ((e = readdir(d)) != NULL) {
        FILE *fp;
        char  buf[4096];
        size_t n;

        if (strncmp(e->d_name, "rec_", 4) != 0)
            continue;
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        fp = fopen(path, "r");
        if (fp == NULL)
            continue;
        n = fread(buf, 1, sizeof(buf) - 1, fp);
        fclose(fp);
        buf[n] = '\0';
        while (len + n + 2 > cap) {
            cap *= 2;
            acc = realloc(acc, cap);
            if (acc == NULL)
                return NULL;
        }
        memcpy(acc + len, buf, n);
        len += n;
        acc[len++] = '\n';
        acc[len]   = '\0';
    }
    closedir(d);
    return acc;
}

#if CREATION_OS_ENABLE_SELF_TESTS
#include <stdio.h>
static int pr_fail(const char *m)
{
    fprintf(stderr, "proof_receipt self-test: %s\n", m);
    return 1;
}
#endif

int cos_proof_receipt_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    struct cos_proof_receipt r;
    float                    sch[4] = {0.1f, 0.2f, 0.3f, 0.4f};
    float                    mch[4] = {0.5f, 0.6f, 0.7f, 0.8f};
    struct cos_error_attribution attr;
    struct cos_proof_receipt_options opt;
    char *js;

    memset(&attr, 0, sizeof attr);
    attr.source = COS_ERR_NONE;

    memset(&opt, 0, sizeof opt);
    opt.codex_fnv64           = 0xdeadbeefULL;
    opt.kernels_run           = 10;
    opt.within_compute_budget = 1;
    opt.model_id              = "unit-test";
    opt.codex_version         = "seed";
    opt.timestamp_ms          = 123456789LL;

    if (cos_proof_receipt_generate_with("hello world", 0.33f, sch, mch,
                                        COS_SIGMA_ACTION_ACCEPT, &attr,
                                        0x3ffULL, &opt, &r)
        != 0)
        return pr_fail("generate");

    if (cos_proof_receipt_verify(&r) != 0)
        return pr_fail("verify");

    js = cos_proof_receipt_to_json(&r);
    if (js == NULL)
        return pr_fail("json");
    free(js);

    {
        struct cos_proof_receipt r2;
        memset(&r2, 0, sizeof r2);
        memcpy(r2.codex_hash, r.codex_hash, 32);
        memcpy(r2.license_hash, r.license_hash, 32);
        memcpy(r2.output_hash, r.output_hash, 32);
        memcpy(r2.prev_receipt_hash, r.prev_receipt_hash, 32);
        r2.sigma_combined = r.sigma_combined + 1.f;
        memcpy(r2.sigma_channels, r.sigma_channels, sizeof r2.sigma_channels);
        memcpy(r2.meta_channels, r.meta_channels, sizeof r2.meta_channels);
        r2.gate_decision            = r.gate_decision;
        r2.attribution              = r.attribution;
        r2.injection_detected       = r.injection_detected;
        r2.risk_level               = r.risk_level;
        r2.sovereign_brake          = r.sovereign_brake;
        r2.within_compute_budget    = r.within_compute_budget;
        r2.kernel_pass_bitmap       = r.kernel_pass_bitmap;
        r2.kernels_run              = r.kernels_run;
        r2.kernels_passed         = r.kernels_passed;
        memcpy(r2.model_id, r.model_id, sizeof r2.model_id);
        memcpy(r2.codex_version, r.codex_version, sizeof r2.codex_version);
        r2.timestamp_ms = r.timestamp_ms;
        r2.constitutional_valid           = r.constitutional_valid;
        r2.constitutional_compliant       = r.constitutional_compliant;
        r2.constitutional_checks          = r.constitutional_checks;
        r2.constitutional_violations      = r.constitutional_violations;
        r2.constitutional_mandatory_halts = r.constitutional_mandatory_halts;
        memcpy(r2.receipt_hash, r.receipt_hash, 32);
        if (cos_proof_receipt_verify(&r2) == 0)
            return pr_fail("tamper detect");
    }

    fprintf(stderr, "proof_receipt self-test: OK\n");
#endif
    return 0;
}
