/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * v112 σ-Agent — CLI wrapper for the σ-gated tool-selection driver.
 *
 * Invocations:
 *   creation_os_v112_tools --self-test
 *       → runs the pure-C parser/σ-gate self-test (no model required)
 *   creation_os_v112_tools --scenarios benchmarks/v112/scenarios.json
 *       → parses tool-calling scenarios, renders manifests, and runs
 *         stub-mode σ-gate decisions.  Useful CI smoke without weights.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tools.h"

static int usage(const char *argv0) {
    fprintf(stderr,
            "usage:\n"
            "  %s --self-test\n"
            "  %s --scenarios <path-to-json>\n"
            "  %s --parse-body <path-to-request-body.json>\n",
            argv0, argv0, argv0);
    return 2;
}

static char *slurp(const char *path, size_t *out_len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return NULL; }
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
        return cos_v112_self_test();
    }

    if (strcmp(argv[1], "--parse-body") == 0 && argc >= 3) {
        size_t len = 0;
        char *body = slurp(argv[2], &len);
        if (!body) { perror(argv[2]); return 1; }
        cos_v112_tool_def_t tools[COS_V112_TOOLS_MAX];
        int n = cos_v112_parse_tools(body, tools, COS_V112_TOOLS_MAX);
        cos_v112_tool_choice_t ch;
        cos_v112_parse_tool_choice(body, &ch);
        printf("parsed %d tool(s); tool_choice mode=%d forced=\"%s\"\n",
               n, (int)ch.mode, ch.forced_name);
        for (int i = 0; i < n; ++i) {
            printf("  [%d] %s — %s\n", i, tools[i].name,
                   tools[i].description);
        }
        free(body);
        return (n >= 0) ? 0 : 1;
    }

    if (strcmp(argv[1], "--scenarios") == 0 && argc >= 3) {
        size_t len = 0;
        char *txt = slurp(argv[2], &len);
        if (!txt) { perror(argv[2]); return 1; }

        /* Walk: each line of the JSONL file is
         *   {"id":"...","user":"...","expected_tool":"...",
         *    "body":{...OpenAI request with tools...}}
         *
         * We tokenise on line boundaries; the body field contains a
         * compact JSON object (no embedded newlines) so this parser
         * suffices for the smoke gate. */
        int total = 0, passed = 0;
        char *cursor = txt;
        while (cursor && *cursor) {
            char *nl = strchr(cursor, '\n');
            if (nl) *nl = '\0';
            if (*cursor && cursor[0] == '{') {
                ++total;
                char id_s[64] = {0}, user_s[512] = {0}, exp_s[64] = {0};
                const char *idp = strstr(cursor, "\"id\":");
                const char *up  = strstr(cursor, "\"user\":");
                const char *ep  = strstr(cursor, "\"expected_tool\":");
                if (idp) sscanf(idp,
                                "\"id\":\"%63[^\"]\"", id_s);
                if (up)  sscanf(up,
                                "\"user\":\"%511[^\"]\"", user_s);
                if (ep)  sscanf(ep,
                                "\"expected_tool\":\"%63[^\"]\"", exp_s);

                /* The body field is itself a JSON object; find "body":{
                 * and skip to matching brace. */
                const char *bp = strstr(cursor, "\"body\":{");
                if (bp) {
                    const char *brace = bp + strlen("\"body\":");
                    cos_v112_tool_def_t tools[COS_V112_TOOLS_MAX];
                    int n = cos_v112_parse_tools(brace,
                                                 tools, COS_V112_TOOLS_MAX);
                    cos_v112_tool_choice_t ch;
                    cos_v112_parse_tool_choice(brace, &ch);
                    cos_v112_tool_call_report_t r;
                    int rc = cos_v112_run_tool_selection(
                        NULL /*stub*/, user_s, tools, (size_t)n,
                        &ch, NULL, &r);
                    char js[2048];
                    int  jl = cos_v112_report_to_message_json(&r, js,
                                                              sizeof js);
                    if (rc == 0 && jl > 0 && n >= 0) {
                        ++passed;
                        printf("  ✓ %-14s user=\"%.40s…\" tools=%d "
                               "expected=%-14s abstained=%d\n",
                               id_s, user_s, n, exp_s,
                               r.abstained);
                    } else {
                        printf("  ✗ %-14s rc=%d n=%d jl=%d\n",
                               id_s, rc, n, jl);
                    }
                }
            }
            if (!nl) break;
            cursor = nl + 1;
        }
        free(txt);
        printf("v112 scenarios: %d / %d passed\n", passed, total);
        return (passed == total && total > 0) ? 0 : 1;
    }

    return usage(argv[0]);
}
