/*
 * cos-stamp — write a C2PA-oriented σ-credential JSON sidecar for a file.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _POSIX_C_SOURCE 200809L

#include "c2pa_sigma.h"
#include "license_attest.h"
#include "semantic_sigma.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int usage(void)
{
    fprintf(stderr,
            "usage: cos-stamp --file PATH [options]\n"
            "  --prompt TEXT     prompt for semantic σ (needs llama-server)\n"
            "  --sigma F         combined σ in [0,1] (when not using --prompt)\n"
            "  --action LABEL    ACCEPT|RETHINK|ABSTAIN (default ACCEPT)\n"
            "  --model NAME      model id (default COS_STAMP_MODEL or stub)\n"
            "  --tau-accept F    default 0.40\n"
            "  --tau-rethink F   default 0.60\n"
            "Writes PATH.cos.json next to the input file.\n");
    return 2;
}

static int read_all(const char *path, char *buf, size_t cap, size_t *out_len)
{
    FILE *fp = fopen(path, "rb");
    long  sz;
    if (fp == NULL)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -2;
    }
    sz = ftell(fp);
    if (sz < 0 || (size_t)sz >= cap) {
        fclose(fp);
        return -3;
    }
    rewind(fp);
    if (fread(buf, 1, (size_t)sz, fp) != (size_t)sz) {
        fclose(fp);
        return -4;
    }
    fclose(fp);
    buf[sz] = '\0';
    *out_len = (size_t)sz;
    return 0;
}

int main(int argc, char **argv)
{
    const char *path = NULL;
    const char *prompt = NULL;
    const char *model  = getenv("COS_STAMP_MODEL");
    const char *action = "ACCEPT";
    float       sigma  = -1.f;
    float       tau_a  = 0.40f;
    float       tau_r  = 0.60f;
    char        outpath[1024];
    char        body[262144];
    size_t      blen = 0;
    cos_c2pa_sigma_assertion_t a;
    struct timespec ts;
    int           i;
    int           port = 0;
    const char   *pe;

    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--file") == 0 && i + 1 < argc)
            path = argv[++i];
        else if (strcmp(argv[i], "--prompt") == 0 && i + 1 < argc)
            prompt = argv[++i];
        else if (strcmp(argv[i], "--sigma") == 0 && i + 1 < argc)
            sigma = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--action") == 0 && i + 1 < argc)
            action = argv[++i];
        else if (strcmp(argv[i], "--model") == 0 && i + 1 < argc)
            model = argv[++i];
        else if (strcmp(argv[i], "--tau-accept") == 0 && i + 1 < argc)
            tau_a = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "--tau-rethink") == 0 && i + 1 < argc)
            tau_r = (float)atof(argv[++i]);
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
            return usage();
    }
    if (path == NULL)
        return usage();

    if (read_all(path, body, sizeof body - 1, &blen) != 0) {
        fprintf(stderr, "cos-stamp: cannot read %s\n", path);
        return 3;
    }

    memset(&a, 0, sizeof a);
    snprintf(a.version, sizeof a.version, "%s", "1.0");
    a.tau_accept  = tau_a;
    a.tau_rethink = tau_r;
    snprintf(a.action, sizeof a.action, "%s", action);
    if (model == NULL || model[0] == '\0')
        model = "stub";
    snprintf(a.model, sizeof a.model, "%s", model);
    a.n_samples = 3;
    a.temps[0]  = 0.1f;
    a.temps[1]  = 0.7f;
    a.temps[2]  = 1.5f;

    spektre_sha256(body, blen, a.content_sha256);

    pe = getenv("COS_BITNET_SERVER_PORT");
    if (pe != NULL && pe[0] != '\0')
        port = atoi(pe);

    if (prompt != NULL && prompt[0] != '\0' && port > 0) {
        cos_semantic_sigma_result sr;
        float                     s = cos_semantic_sigma_ex(
            prompt, NULL, port, model, 3, body, &sr);
        a.sigma_combined  = s;
        a.sigma_semantic  = sr.sigma;
        a.sigma_logprob   = sr.similarities[0];
        snprintf(a.method, sizeof a.method, "%s",
                 "semantic_entropy_3_sample");
    } else if (sigma >= 0.f && sigma <= 1.f) {
        a.sigma_combined = sigma;
        a.sigma_semantic = sigma;
        a.sigma_logprob  = sigma;
        snprintf(a.method, sizeof a.method, "%s", "cli_supplied_sigma");
    } else {
        fprintf(stderr,
                "cos-stamp: supply --sigma F in [0,1] or --prompt with "
                "COS_BITNET_SERVER_PORT and llama-server for semantic σ\n");
        return 4;
    }

    memcpy(a.receipt_hash, a.content_sha256, 32);
    memset(a.prev_receipt_hash, 0, 32);
    memset(a.codex_sha256, 0, 32);

    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        a.timestamp_ms =
            (int64_t)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
    else
        a.timestamp_ms = (int64_t)time(NULL) * 1000LL;

    if (snprintf(outpath, sizeof outpath, "%s.cos.json", path)
        >= (int)sizeof outpath) {
        fprintf(stderr, "cos-stamp: output path too long\n");
        return 5;
    }
    if (cos_c2pa_sigma_write_sidecar(outpath, &a) != 0) {
        fprintf(stderr, "cos-stamp: cannot write %s\n", outpath);
        return 6;
    }
    printf("cos-stamp: wrote %s\n", outpath);
    return 0;
}
