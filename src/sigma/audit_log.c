/* Append-only ~/.cos/audit/YYYY-MM-DD.jsonl with chain_prev = SHA-256 of prior raw line + '\n'.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "audit_log.h"
#include "../license_kernel/license_attest.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int audit_mkdir_p(const char *path)
{
#if defined(__unix__) || defined(__APPLE__)
    return mkdir(path, 0700);
#else
    (void)path;
    return -1;
#endif
}

static void audit_dir_today_file(char *buf, size_t cap, const struct tm *tmv)
{
    const char *home = getenv("HOME");
    if (!home || !home[0])
        home = "/tmp";
    char dir[512];
    snprintf(dir, sizeof dir, "%s/.cos/audit", home);
    if (audit_mkdir_p(dir) != 0 && errno != EEXIST)
        home = "/tmp";
    char date[32];
    strftime(date, sizeof date, "%Y-%m-%d", tmv);
    snprintf(buf, cap, "%s/.cos/audit/%s.jsonl", home, date);
}

static void iso8601_utc(char *out, size_t cap, time_t t)
{
    struct tm tmv;
    if (gmtime_r(&t, &tmv) == NULL) {
        if (cap)
            out[0] = '\0';
        return;
    }
    strftime(out, cap, "%Y-%m-%dT%H:%M:%SZ", &tmv);
}

static void sha256_bytes_hex(const void *data, size_t len, char out65[65])
{
    uint8_t h[32];
    spektre_sha256(data, len, h);
    static const char *hx = "0123456789abcdef";
    for (int i = 0; i < 32; ++i) {
        out65[i * 2]     = hx[(h[i] >> 4) & 15];
        out65[i * 2 + 1] = hx[h[i] & 15];
    }
    out65[64] = '\0';
}

static void chain_prev_from_file(const char *path, char *out_chain, size_t chaincap)
{
    out_chain[0] = '\0';
    if (chaincap < 72)
        return;
    FILE *f = fopen(path, "rb");
    if (!f)
        return;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return;
    }
    long sz = ftell(f);
    if (sz <= 0) {
        fclose(f);
        return;
    }
    long chunk = sz > 65536 ? 65536 : sz;
    if (fseek(f, sz - chunk, SEEK_SET) != 0) {
        fclose(f);
        return;
    }
    char *buf = (char *)malloc((size_t)chunk + 1u);
    if (!buf) {
        fclose(f);
        return;
    }
    size_t n = fread(buf, 1, (size_t)chunk, f);
    fclose(f);
    buf[n] = '\0';
    char *lastnl = NULL;
    for (char *p = buf; *p; ++p) {
        if (*p == '\n')
            lastnl = p;
    }
    if (!lastnl) {
        free(buf);
        return;
    }
    char *prev_start = buf;
    for (char *p = buf; p < lastnl; ++p) {
        if (*p == '\n')
            prev_start = p + 1;
    }
    size_t line_with_nl = (size_t)(lastnl - prev_start + 1);
    char hx[65];
    sha256_bytes_hex(prev_start, line_with_nl, hx);
    snprintf(out_chain, chaincap, "sha256:%s", hx);
    free(buf);
}

static void fprint_json_str(FILE *fp, const char *s, size_t maxrun)
{
    fputc('"', fp);
    if (s) {
        for (size_t i = 0; i < maxrun && s[i]; ++i) {
            unsigned char c = (unsigned char)s[i];
            if (c == '"' || c == '\\') {
                fputc('\\', fp);
                fputc((int)c, fp);
            } else if (c < 0x20u)
                fprintf(fp, "\\u%04x", c);
            else
                fputc((int)c, fp);
        }
    }
    fputc('"', fp);
}

int cos_audit_append_serve_row(const char *audit_id, const char *prompt_hash,
                               const char *input_preview, double sigma,
                               double sigma_calibrated, const char *action,
                               const char *model, const char *response_hash,
                               double latency_ms, float temperature,
                               float tau_accept, float tau_rethink,
                               int constitutional_halt, const char *codex_hash)
{
    if (!audit_id || !prompt_hash || !action || !model || !response_hash)
        return -1;
    time_t now = time(NULL);
    struct tm tmv;
    if (gmtime_r(&now, &tmv) == NULL)
        return -1;
    char path[640];
    audit_dir_today_file(path, sizeof path, &tmv);

    char chain_prev[96];
    chain_prev_from_file(path, chain_prev, sizeof chain_prev);

    char ts[48];
    iso8601_utc(ts, sizeof ts, now);

    const char *cx = codex_hash && codex_hash[0] ? codex_hash
                                                 : "sha256:0000000000000000000000000000000000000000000000000000000000000000";

    FILE *fp = fopen(path, "a");
    if (!fp)
        return -1;

    fprintf(fp,
            "{\"timestamp\":\"%s\",\"audit_id\":\"%s\",\"prompt_hash\":\"%s\","
            "\"input_preview\":",
            ts, audit_id, prompt_hash);
    fprint_json_str(fp, input_preview ? input_preview : "", 120);
    fprintf(fp,
            ",\"sigma\":%.6f,\"sigma_calibrated\":%.6f,\"action\":\"%s\","
            "\"model\":",
            sigma, sigma_calibrated, action);
    fprint_json_str(fp, model, 200);
    fprintf(fp, ",\"response_hash\":\"%s\",\"latency_ms\":%.1f,"
                "\"temperature\":%.3f,\"tau_accept\":%.6f,\"tau_rethink\":%.6f,"
                "\"constitutional_halt\":%s,\"codex_hash\":\"%s\","
                "\"chain_prev\":\"%s\"}\n",
            response_hash, latency_ms, (double)temperature, (double)tau_accept,
            (double)tau_rethink, constitutional_halt ? "true" : "false", cx,
            chain_prev[0] ? chain_prev : "");
    if (fflush(fp) != 0) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

void cos_audit_write(const char *audit_id, const char *prompt_hash, float sigma,
                     float sigma_calibrated, const char *action, const char *model,
                     const char *response_hash, int latency_ms, float tau_accept,
                     float tau_rethink, int constitutional_halt)
{
    (void)cos_audit_append_serve_row(audit_id, prompt_hash, NULL, (double)sigma,
                                     (double)sigma_calibrated, action, model, response_hash,
                                     (double)latency_ms, 0.0f, tau_accept, tau_rethink,
                                     constitutional_halt, NULL);
}
