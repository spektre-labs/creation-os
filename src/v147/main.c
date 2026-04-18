/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v147 σ-Reflect — CLI.
 */
#include "reflect.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(const char *a0) {
    fprintf(stderr,
        "usage: %s --self-test\n"
        "       %s --demo [--pure ANSWER]\n"
        "       %s --divergence-demo\n",
        a0, a0, a0);
    return 2;
}

static void seed_trace(cos_v147_trace_t *t) {
    cos_v147_trace_init(t, "42", 0.20f);
    cos_v147_trace_add(t, "parse the question", 0.15f);
    cos_v147_trace_add(t, "recall the formula", 0.22f);
    cos_v147_trace_add(t, "apply with values",  0.67f);
    cos_v147_trace_add(t, "check arithmetic",   0.18f);
}

static int demo(const char *pure_answer) {
    cos_v147_trace_t t;
    seed_trace(&t);
    cos_v147_reflection_t r;
    cos_v147_reflect(&t, pure_answer ? pure_answer : "42", 0.22f, &r);
    char tb[1024], rb[1024];
    cos_v147_trace_to_json(&t, tb, sizeof tb);
    cos_v147_reflection_to_json(&r, &t, rb, sizeof rb);
    printf("{\"v147_demo\":true,\"trace\":%s,\"reflection\":%s}\n",
           tb, rb);
    return 0;
}

static int divergence_demo(void) {
    return demo("WRONG");
}

int main(int argc, char **argv) {
    if (argc < 2) return usage(argv[0]);
    if (!strcmp(argv[1], "--self-test"))      return cos_v147_self_test();
    if (!strcmp(argv[1], "--divergence-demo")) return divergence_demo();
    if (!strcmp(argv[1], "--demo")) {
        const char *pure = "42";
        for (int i = 2; i < argc; ++i) {
            if (!strcmp(argv[i], "--pure") && i + 1 < argc) {
                pure = argv[++i];
            }
        }
        return demo(pure);
    }
    return usage(argv[0]);
}
