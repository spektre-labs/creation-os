/*
 * creation_os_suite_stub — print suite mode metadata (prompt stub + tool names).
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * This is a lab helper: it does not run an LLM, HTTP server, or IDE integration.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"

static int self_test(void)
{
    int n = 0;
    const char **t = get_tools_for_mode(MODE_CODE, &n);
    if (n != 9 || !t)
        return 1;
    const char *p = get_system_prompt(MODE_CODE);
    if (!p || !strstr(p, "coding"))
        return 1;
    return 0;
}

int main(int argc, char **argv)
{
    const char *mode = MODE_CHAT;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test"))
            return self_test() == 0 ? 0 : 2;
        if (!strcmp(argv[i], "--mode") && i + 1 < argc)
            mode = argv[++i];
    }

    printf("mode=%s\n", mode);
    printf("%s\n", get_system_prompt(mode));
    int n = 0;
    const char **tools = get_tools_for_mode(mode, &n);
    printf("tools (%d):", n);
    for (int i = 0; i < n; i++)
        printf(" %s", tools[i]);
    printf("\n");
    return 0;
}
