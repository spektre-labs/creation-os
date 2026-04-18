/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v115 CLI:
 *   --self-test                        run pure-C self-test
 *   --db PATH                          override $HOME/.creation-os/memory.sqlite
 *   --write-episodic "text" --sigma S [--session X --tags "a,b"]
 *   --search "query" [--top-k N]       print JSON results
 *   --counts                           print row counts JSON
 */

#include "memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(void) {
    fprintf(stderr,
        "usage: creation_os_v115_memory [--self-test] "
        "[--db PATH] [--write-episodic TEXT --sigma F [--session S --tags T]] "
        "[--search Q [--top-k K]] [--counts]\n");
}

int main(int argc, char **argv) {
    const char *db        = NULL;
    const char *write_txt = NULL;
    const char *search_q  = NULL;
    const char *session   = NULL;
    const char *tags      = NULL;
    float       sigma     = 0.5f;
    int         top_k     = 5;
    int         do_self   = 0;
    int         do_counts = 0;

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--self-test"))      do_self   = 1;
        else if (!strcmp(argv[i], "--counts"))    do_counts = 1;
        else if (!strcmp(argv[i], "--db") && i + 1 < argc)             db        = argv[++i];
        else if (!strcmp(argv[i], "--write-episodic") && i + 1 < argc) write_txt = argv[++i];
        else if (!strcmp(argv[i], "--sigma") && i + 1 < argc)          sigma     = (float)atof(argv[++i]);
        else if (!strcmp(argv[i], "--session") && i + 1 < argc)        session   = argv[++i];
        else if (!strcmp(argv[i], "--tags") && i + 1 < argc)           tags      = argv[++i];
        else if (!strcmp(argv[i], "--search") && i + 1 < argc)         search_q  = argv[++i];
        else if (!strcmp(argv[i], "--top-k") && i + 1 < argc)          top_k     = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "v115: unknown arg %s\n", argv[i]);
            usage();
            return 2;
        }
    }

    if (do_self) {
        int rc = cos_v115_self_test();
        fprintf(stdout, rc == 0 ? "v115 self-test: PASS\n"
                                : "v115 self-test: FAIL\n");
        return rc ? 1 : 0;
    }

    cos_v115_config_t cfg;
    cos_v115_config_defaults(&cfg);
    if (db) snprintf(cfg.db_path, sizeof cfg.db_path, "%s", db);

    cos_v115_store_t *s = NULL;
    int rc = cos_v115_open(&cfg, &s);
    if (rc != 0 || !s) {
        fprintf(stderr,
                rc > 0 ? "v115: memory unavailable on this build (built with COS_V115_NO_SQLITE)\n"
                       : "v115: open failed\n");
        return rc > 0 ? 0 : 1;
    }

    int exit_code = 0;
    if (write_txt) {
        int64_t id = cos_v115_write_episodic(s, write_txt, sigma, session, tags);
        if (id <= 0) { fprintf(stderr, "v115: write failed\n"); exit_code = 1; }
        else         printf("{\"id\":%lld}\n", (long long)id);
    }

    if (search_q) {
        cos_v115_memory_row_t rows[COS_V115_TOPK_MAX];
        int n = cos_v115_search_episodic(s, search_q, top_k, rows, COS_V115_TOPK_MAX);
        if (n < 0) { fprintf(stderr, "v115: search failed\n"); exit_code = 1; }
        else {
            char buf[32768];
            int jn = cos_v115_search_results_to_json(rows, n, 1.0f, top_k,
                                                      buf, sizeof buf);
            if (jn < 0) {
                fprintf(stderr, "v115: json overflow\n");
                exit_code = 1;
            } else {
                fwrite(buf, 1, (size_t)jn, stdout);
                fputc('\n', stdout);
            }
        }
    }

    if (do_counts) {
        int e = cos_v115_count_episodic (s);
        int k = cos_v115_count_knowledge(s);
        int c = cos_v115_count_chat     (s);
        printf("{\"episodic\":%d,\"knowledge\":%d,\"chat\":%d}\n", e, k, c);
    }

    cos_v115_close(s);
    return exit_code;
}
