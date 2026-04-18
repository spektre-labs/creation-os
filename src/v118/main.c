/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v118 CLI:
 *   --self-test           run pure-C self-test
 *   --project FILE [TAU]  run placeholder projection over FILE bytes,
 *                         print JSON result (TAU default 0.55)
 *   --parse-body FILE     parse image_url from a JSON body in FILE,
 *                         print decoded byte count
 */

#include "vision.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int read_all(const char *path, unsigned char **buf, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    if (len < 0) { fclose(f); return -1; }
    fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc((size_t)len);
    if (!b) { fclose(f); return -1; }
    size_t r = fread(b, 1, (size_t)len, f);
    fclose(f);
    if (r != (size_t)len) { free(b); return -1; }
    *buf = b;
    *n   = (size_t)len;
    return 0;
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "--self-test")) {
        int rc = cos_v118_self_test();
        printf(rc == 0 ? "v118 self-test: PASS\n" : "v118 self-test: FAIL\n");
        return rc ? 1 : 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "--parse-body")) {
        unsigned char *body = NULL;
        size_t n = 0;
        if (read_all(argv[2], &body, &n) < 0) {
            fprintf(stderr, "v118: cannot read %s\n", argv[2]);
            return 1;
        }
        cos_v118_image_t img;
        unsigned char decoded[1 << 20];
        size_t dn = 0;
        int rc = cos_v118_parse_image_url((const char *)body, n, &img,
                                           decoded, sizeof decoded, &dn);
        free(body);
        if (rc != 0) { fprintf(stderr, "v118: parse failed\n"); return 1; }
        printf("{\"source\":%d,\"url_prefix\":\"%.40s\",\"decoded\":%zu}\n",
               (int)img.source, img.url, dn);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "--project")) {
        unsigned char *b = NULL;
        size_t n = 0;
        if (read_all(argv[2], &b, &n) < 0) {
            fprintf(stderr, "v118: cannot read %s\n", argv[2]);
            return 1;
        }
        float tau = argc >= 4 ? (float)atof(argv[3]) : 0.55f;
        cos_v118_image_t img = { COS_V118_SRC_RAW_BYTES, b, n, {0} };
        cos_v118_projection_result_t r;
        cos_v118_project(&img, tau, &r, NULL);
        char js[2048];
        int w = cos_v118_result_to_json(&r, js, sizeof js);
        if (w > 0) { fwrite(js, 1, (size_t)w, stdout); fputc('\n', stdout); }
        free(b);
        return 0;
    }
    fprintf(stderr, "usage: creation_os_v118_vision --self-test | "
                    "--project FILE [TAU] | --parse-body FILE\n");
    return 0;
}
