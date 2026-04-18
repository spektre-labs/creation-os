/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v113 σ-Sandbox — CLI wrapper.  Runs the pure-C self-test or a
 * parsed JSON request from a file (for merge-gate smoke tests).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sandbox.h"

static int usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s --self-test\n"
            "  %s --execute <path-to-request-body.json>\n"
            "  %s --echo python|bash|sh '<code>'\n",
            argv0, argv0, argv0);
    return 2;
}

static char *slurp(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[got] = '\0';
    if (out_len) *out_len = got;
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) return usage(argv[0]);
    if (strcmp(argv[1], "--self-test") == 0) {
        return cos_v113_self_test();
    }
    if (strcmp(argv[1], "--execute") == 0 && argc >= 3) {
        size_t len = 0;
        char *body = slurp(argv[2], &len);
        if (!body) { perror(argv[2]); return 1; }
        cos_v113_request_t req;
        if (cos_v113_parse_request(body, &req) != 0) {
            fprintf(stderr, "parse failed\n"); free(body); return 1;
        }
        cos_v113_result_t res;
        int rc = cos_v113_sandbox_run(&req, &res);
        if (rc != 0) { free(body); return 2; }
        static char js[96 * 1024];
        int jl = cos_v113_result_to_json(&res, js, sizeof js);
        if (jl > 0) puts(js);
        free(body);
        return res.executed && res.exit_code == 0 ? 0 : 1;
    }
    if (strcmp(argv[1], "--echo") == 0 && argc >= 4) {
        cos_v113_request_t req;
        cos_v113_request_defaults(&req);
        req.language = cos_v113_lang_from_string(argv[2]);
        if (req.language == COS_V113_LANG_UNKNOWN) req.language = COS_V113_LANG_SH;
        req.code     = argv[3];
        req.code_len = strlen(argv[3]);
        req.timeout_sec = 5;
        req.sigma_product_in = -1.f;
        cos_v113_result_t res;
        if (cos_v113_sandbox_run(&req, &res) != 0) return 2;
        static char js[96 * 1024];
        cos_v113_result_to_json(&res, js, sizeof js);
        puts(js);
        return res.executed && res.exit_code == 0 ? 0 : 1;
    }
    return usage(argv[0]);
}
