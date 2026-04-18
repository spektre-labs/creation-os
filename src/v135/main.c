/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v135 σ-Symbolic — CLI wrapper.
 */
#include "symbolic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v135_symbolic --self-test\n"
        "  creation_os_v135_symbolic --demo\n"
        "  creation_os_v135_symbolic --query 'pred(X, const)' \\\n"
        "      --fact 'p1(a,b)' --fact 'p1(c,d)' ...\n"
        "  creation_os_v135_symbolic --check subject pred object "
             "[--fact ...] [--functional pred]\n");
    return 2;
}

static int cmd_query(int argc, char **argv) {
    cos_v135_kb_t kb; cos_v135_kb_init(&kb);
    const char *query = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--query") && i + 1 < argc)      query = argv[++i];
        else if (!strcmp(argv[i], "--fact") && i + 1 < argc)  cos_v135_kb_assert(&kb, argv[++i]);
        else if (!strcmp(argv[i], "--functional") && i+1<argc)cos_v135_kb_mark_functional(&kb, argv[++i]);
    }
    if (!query) return usage();
    cos_v135_query_t q;
    if (cos_v135_parse_query(&kb, query, &q) != 0) {
        fprintf(stderr, "parse error\n"); return 1;
    }
    cos_v135_solution_t sols[COS_V135_MAX_SOLUTIONS];
    int n = cos_v135_solve(&kb, &q, sols, COS_V135_MAX_SOLUTIONS);
    printf("solutions=%d\n", n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < sols[i].n_bindings; ++j)
            printf("  %s = %s%s", sols[i].bindings[j].var,
                   cos_v135_kb_atom(&kb, sols[i].bindings[j].atom),
                   j + 1 < sols[i].n_bindings ? "," : "\n");
        if (sols[i].n_bindings == 0) printf("  (ground)\n");
    }
    return 0;
}

static int cmd_check(int argc, char **argv) {
    if (argc < 5) return usage();
    const char *subj = argv[2], *pred = argv[3], *obj = argv[4];
    cos_v135_kb_t kb; cos_v135_kb_init(&kb);
    for (int i = 5; i < argc; ++i) {
        if (!strcmp(argv[i], "--fact") && i + 1 < argc)
            cos_v135_kb_assert(&kb, argv[++i]);
        else if (!strcmp(argv[i], "--functional") && i + 1 < argc)
            cos_v135_kb_mark_functional(&kb, argv[++i]);
    }
    cos_v135_verdict_t v = cos_v135_check_triple(&kb, subj, pred, obj);
    const char *name = v == COS_V135_CONFIRM   ? "CONFIRM"
                     : v == COS_V135_CONTRADICT? "CONTRADICT"
                     :                           "UNKNOWN";
    printf("verdict=%s\n", name);
    return 0;
}

static int cmd_demo(void) {
    cos_v135_kb_t kb; cos_v135_kb_init(&kb);
    cos_v135_kb_assert(&kb, "works_at(lauri, spektre_labs)");
    cos_v135_kb_assert(&kb, "located_in(spektre_labs, helsinki)");
    cos_v135_kb_assert(&kb, "developed(lauri, creation_os)");
    cos_v135_kb_mark_functional(&kb, "works_at");
    cos_v135_query_t q;
    cos_v135_parse_query(&kb, "?- works_at(X, spektre_labs).", &q);
    cos_v135_solution_t sols[16];
    int n = cos_v135_solve(&kb, &q, sols, 16);
    printf("query: ?- works_at(X, spektre_labs).\n");
    printf("solutions=%d\n", n);
    for (int i = 0; i < n; ++i)
        printf("  X = %s\n", cos_v135_kb_atom(&kb, sols[i].bindings[0].atom));
    cos_v135_verdict_t v =
        cos_v135_check_triple(&kb, "lauri", "works_at", "openai");
    printf("check(lauri works_at openai) = %s\n",
        v == COS_V135_CONTRADICT ? "CONTRADICT"
      : v == COS_V135_CONFIRM    ? "CONFIRM" : "UNKNOWN");
    cos_v135_route_cfg_t rc = { .tau_symbolic = 0.30f,
                                .is_logical_question = 1 };
    printf("route(σ=0.10)=%d route(σ=0.50)=%d\n",
        cos_v135_route(0.10f, &rc), cos_v135_route(0.50f, &rc));
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v135_self_test();
    if (!strcmp(argv[1], "--demo"))      return cmd_demo();
    if (!strcmp(argv[1], "--check"))     return cmd_check(argc, argv);
    /* anything else: treat as query mode */
    return cmd_query(argc, argv);
}
