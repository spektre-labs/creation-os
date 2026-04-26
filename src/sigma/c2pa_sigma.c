/*
 * C2PA-oriented σ-credential JSON sidecar (informative; not a C2PA signer).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "c2pa_sigma.h"
#include "license_attest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#define CREATION_OS_ENABLE_SELF_TESTS 0
#endif

static void json_esc_cstr(const char *in, char *out, size_t cap)
{
    size_t j = 0;
    if (in == NULL)
        in = "";
    for (size_t i = 0; in[i] != '\0' && j + 2 < cap; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '"' || c == '\\') {
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c < 0x20) {
            if (j + 7 >= cap)
                break;
            j += (size_t)snprintf(out + j, cap - j, "\\u%04x", (unsigned)c);
        } else
            out[j++] = (char)c;
    }
    out[j] = '\0';
}

static void iso8601_utc(int64_t tms, char *buf, size_t cap)
{
    time_t    sec = (time_t)(tms / 1000LL);
    struct tm tmv;
    if (gmtime_r(&sec, &tmv) == NULL) {
        snprintf(buf, cap, "1970-01-01T00:00:00Z");
        return;
    }
    strftime(buf, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

int cos_c2pa_sigma_assertion_to_json(const cos_c2pa_sigma_assertion_t *a,
                                     char *out, size_t cap)
{
    char esc_action[48], esc_method[140], esc_model[220];
    char hx_content[65], hx_rec[65], hx_prev[65], hx_codex[65];
    char ts[40];

    if (a == NULL || out == NULL || cap < 512)
        return -1;

    json_esc_cstr(a->action, esc_action, sizeof esc_action);
    json_esc_cstr(a->method, esc_method, sizeof esc_method);
    json_esc_cstr(a->model, esc_model, sizeof esc_model);
    spektre_hex_lower(a->content_sha256, hx_content);
    spektre_hex_lower(a->receipt_hash, hx_rec);
    spektre_hex_lower(a->prev_receipt_hash, hx_prev);
    spektre_hex_lower(a->codex_sha256, hx_codex);
    iso8601_utc(a->timestamp_ms, ts, sizeof ts);

    return snprintf(
        out, cap,
        "{"
        "\"c2pa_assertion\":\"%s\","
        "\"version\":\"%s\","
        "\"sigma\":{"
        "\"combined\":%.6g,\"semantic\":%.6g,\"logprob\":%.6g,"
        "\"action\":\"%s\",\"tau_accept\":%.6g,\"tau_rethink\":%.6g"
        "},"
        "\"verification\":{"
        "\"method\":\"%s\",\"model\":\"%s\","
        "\"n_samples\":%d,"
        "\"temperatures\":[%.6g,%.6g,%.6g]"
        "},"
        "\"proofs\":{"
        "\"repository\":{"
        "\"lean\":\"check-lean-t3-discharged\","
        "\"frama_c\":\"check-v138\""
        "},"
        "\"note\":\"Harness names only — see docs/CLAIM_DISCIPLINE.md for "
        "evidence classes; not a substitute for certification.\""
        "},"
        "\"content_sha256\":\"%s\","
        "\"receipt\":{"
        "\"hash\":\"%s\",\"chain_prev\":\"%s\",\"codex_sha256\":\"%s\""
        "},"
        "\"timestamp\":\"%s\""
        "}",
        COS_C2PA_SIGMA_LABEL, a->version, (double)a->sigma_combined,
        (double)a->sigma_semantic, (double)a->sigma_logprob, esc_action,
        (double)a->tau_accept, (double)a->tau_rethink, esc_method,
        esc_model, a->n_samples, (double)a->temps[0], (double)a->temps[1],
        (double)a->temps[2], hx_content, hx_rec, hx_prev, hx_codex, ts);
}

int cos_c2pa_serialize(const cos_c2pa_sigma_assertion_t *a, uint8_t *out,
                       int max_len)
{
    (void)a;
    (void)out;
    (void)max_len;
    return -1;
}

int cos_c2pa_sigma_write_sidecar(const char *sidecar_path,
                                 const cos_c2pa_sigma_assertion_t *a)
{
    char  buf[16384];
    int   n;
    FILE *fp;

    if (sidecar_path == NULL || sidecar_path[0] == '\0' || a == NULL)
        return -1;
    n = cos_c2pa_sigma_assertion_to_json(a, buf, sizeof buf);
    if (n < 0 || (size_t)n >= sizeof buf)
        return -2;
    fp = fopen(sidecar_path, "w");
    if (fp == NULL)
        return -3;
    if (fputs(buf, fp) == EOF) {
        fclose(fp);
        return -4;
    }
    fclose(fp);
    return 0;
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

static float parse_float_after(const char *json, const char *needle)
{
    const char *p = strstr(json, needle);
    if (p == NULL)
        return -1.f;
    return (float)strtod(p + strlen(needle), NULL);
}

int cos_c2pa_sigma_validate_file(const char *sidecar_path, FILE *report)
{
    char  buf[65536];
    FILE *fp;
    size_t n;
    float  sg;

    if (sidecar_path == NULL || report == NULL)
        return -1;
    fp = fopen(sidecar_path, "r");
    if (fp == NULL)
        return -2;
    n = fread(buf, 1, sizeof buf - 1, fp);
    fclose(fp);
    buf[n] = '\0';

    if (strstr(buf, COS_C2PA_SIGMA_LABEL) == NULL) {
        fprintf(report, "σ-credential INVALID (missing assertion label)\n");
        return 1;
    }
    sg = parse_float_after(buf, "\"combined\":");
    if (sg < 0.f || sg > 1.f) {
        fprintf(report, "σ-credential INVALID (sigma combined out of range)\n");
        return 1;
    }
    {
        const char *rp = strstr(buf, "\"receipt\":{");
        const char *hp;
        uint8_t     tmp[32];
        if (rp != NULL && (hp = strstr(rp, "\"hash\":\"")) != NULL) {
            hp += strlen("\"hash\":\"");
            {
                int ok = 1;
                int i;
                for (i = 0; i < 64; i += 2) {
                    int hi = hex_byte(hp[i]);
                    int lo = hex_byte(hp[i + 1]);
                    if (hi < 0 || lo < 0) {
                        ok = 0;
                        break;
                    }
                    tmp[i / 2] = (uint8_t)((hi << 4) | lo);
                }
                if (!ok) {
                    fprintf(report, "σ-credential INVALID (receipt hash hex)\n");
                    return 1;
                }
            }
        }
    }
    fprintf(report,
            "σ-credential VALID\n"
            "  σ = %.3f (combined)\n"
            "  See file: %s\n",
            (double)sg, sidecar_path);
    return 0;
}

int cos_c2pa_sigma_self_test(void)
{
#if CREATION_OS_ENABLE_SELF_TESTS
    cos_c2pa_sigma_assertion_t a;
    char                       js[8192];
    char                       tmpl[] = "/tmp/cos_c2pa_ckXXXXXX";
    int                        fd;

    memset(&a, 0, sizeof a);
    snprintf(a.version, sizeof a.version, "%s", "1.0");
    a.sigma_combined  = 0.25f;
    a.sigma_semantic  = 0.26f;
    a.sigma_logprob   = 0.24f;
    snprintf(a.action, sizeof a.action, "%s", "ACCEPT");
    a.tau_accept  = 0.4f;
    a.tau_rethink = 0.6f;
    snprintf(a.method, sizeof a.method, "%s", "unit_test");
    snprintf(a.model, sizeof a.model, "%s", "stub");
    a.n_samples = 3;
    a.temps[0]  = 0.1f;
    a.temps[1]  = 0.7f;
    a.temps[2]  = 1.5f;
    a.timestamp_ms = 1700000000000LL;
    spektre_sha256("hello", 5, a.content_sha256);
    memcpy(a.receipt_hash, a.content_sha256, 32);
    if (cos_c2pa_sigma_assertion_to_json(&a, js, sizeof js) < 10)
        return 1;
    fd = mkstemp(tmpl);
    if (fd < 0)
        return 1;
    close(fd);
    if (cos_c2pa_sigma_write_sidecar(tmpl, &a) != 0)
        return 1;
    if (cos_c2pa_sigma_validate_file(tmpl, stdout) != 0)
        return 1;
    remove(tmpl);
#endif
    return 0;
}
