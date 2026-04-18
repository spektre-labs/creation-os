/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v114 σ-Swarm — CLI wrapper.  Runs self-test or parses/arbitrates a
 * config + prompt on stdin for the merge-gate smoke scripts.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "swarm.h"

static int usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s --self-test\n"
            "  %s --run <config.toml> '<prompt>'\n"
            "  %s --parse-config <config.toml>\n",
            argv0, argv0, argv0);
    return 2;
}

static char *slurp(const char *path) {
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
    return buf;
}

int main(int argc, char **argv)
{
    if (argc < 2) return usage(argv[0]);

    if (strcmp(argv[1], "--self-test") == 0) {
        return cos_v114_self_test();
    }

    if (strcmp(argv[1], "--parse-config") == 0 && argc >= 3) {
        char *t = slurp(argv[2]);
        if (!t) { perror(argv[2]); return 1; }
        cos_v114_config_t cfg;
        int n = cos_v114_parse_config(t, &cfg);
        free(t);
        if (n < 0) { fprintf(stderr, "parse failed\n"); return 1; }
        printf("specialists=%d routing=%s threshold=%d epsilon=%.3f\n",
               n, cfg.routing, cfg.consensus_threshold,
               (double)cfg.consensus_epsilon);
        for (size_t i = 0; i < cfg.n; ++i) {
            printf("  [%zu] %s  role=\"%s\"  gguf=\"%s\"\n",
                   i, cfg.specialists[i].name, cfg.specialists[i].role,
                   cfg.specialists[i].gguf_path);
        }
        return 0;
    }

    if (strcmp(argv[1], "--run") == 0 && argc >= 4) {
        char *t = slurp(argv[2]);
        if (!t) { perror(argv[2]); return 1; }
        cos_v114_config_t cfg;
        int n = cos_v114_parse_config(t, &cfg);
        free(t);
        if (n <= 0) { fprintf(stderr, "no specialists\n"); return 1; }

        cos_v114_candidate_t cands[COS_V114_MAX_SPECIALISTS];
        int nc = cos_v114_stub_run(&cfg, argv[3], cands,
                                   COS_V114_MAX_SPECIALISTS);
        cos_v114_decision_t dec;
        cos_v114_arbitrate(cands, (size_t)nc, &cfg, &dec);
        static char hdr[2048], bdy[16 * 1024];
        cos_v114_build_response(cands, (size_t)nc, &dec,
                                "test-swarm", 1700000000,
                                hdr, sizeof hdr, bdy, sizeof bdy);
        fputs(hdr, stdout);
        puts("");
        puts(bdy);
        return 0;
    }

    return usage(argv[0]);
}
