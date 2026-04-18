/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v137 σ-Compile — CLI wrapper.
 */
#include "compile.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v137_compile --self-test\n"
        "  creation_os_v137_compile --emit [--tau t0,t1,...,t7] [--out path.c]\n"
        "  creation_os_v137_compile --bench [--iters N]\n");
    return 2;
}

static int parse_tau(const char *s, float tau[8]) {
    for (int i = 0; i < 8; ++i) tau[i] = 0.50f;
    if (!s) return 0;
    int k = 0;
    while (*s && k < 8) {
        char *end = NULL;
        double v = strtod(s, &end);
        if (end == s) break;
        tau[k++] = (float)v;
        s = (*end == ',') ? end + 1 : end;
    }
    return k;
}

static int cmd_emit(int argc, char **argv) {
    const char *out = NULL;
    float tau[8];
    for (int i = 0; i < 8; ++i) tau[i] = 0.50f;
    for (int i = 0; i < argc; ++i) {
        if (!strcmp(argv[i], "--tau") && i + 1 < argc) parse_tau(argv[++i], tau);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc) out = argv[++i];
    }
    return cos_v137_emit_source(tau, out);
}

static int cmd_bench(int argc, char **argv) {
    int iters = 2000000;
    for (int i = 0; i < argc; ++i)
        if (!strcmp(argv[i], "--iters") && i + 1 < argc) iters = atoi(argv[++i]);
    if (iters <= 0) iters = 2000000;

    const float tau[8] = {0.50f,0.50f,0.50f,0.50f,
                          0.50f,0.50f,0.50f,0.50f};
    float pool[32 * 8];
    for (int i = 0; i < 32 * 8; ++i) pool[i] = (float)(i & 7) * 0.10f;
    uint64_t ck = 0;
    double ns_int = cos_v137_bench_interpreted(tau, pool, 32, iters, &ck);
    double ns_cmp = cos_v137_bench_compiled   (     pool, 32, iters, &ck);
    printf("iters=%d\n", iters);
    printf("ns_per_call_interpreted=%.4f\n", ns_int);
    printf("ns_per_call_compiled=%.4f\n",    ns_cmp);
    printf("speedup=%.4fx\n", ns_int > 0 ? ns_int / ns_cmp : 0.0);
    printf("checksum=%llu\n", (unsigned long long)ck);
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v137_self_test();
    if (!strcmp(argv[1], "--emit"))      return cmd_emit(argc - 2, argv + 2);
    if (!strcmp(argv[1], "--bench"))     return cmd_bench(argc - 2, argv + 2);
    return usage();
}
