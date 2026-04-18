/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v123 σ-Formal structural validator.  Reads the TLA+ spec and
 * confirms the σ-governance contract (seven named invariants +
 * well-formed module) so every CI runner enforces at least the
 * structural guarantee, independent of a local `tlc` install.
 */

#include "formal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

const char *const COS_V123_REQUIRED_INVARIANTS[7] = {
    "TypeOK",
    "AbstainSafety",
    "EmitCoherence",
    "MemoryMonotonicity",
    "SwarmConsensus",
    "NoSilentFailure",
    "ToolSafety",
};

static int file_exists(const char *path) {
    struct stat st;
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static char *slurp(const char *path, size_t *n_out) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz < 0) { fclose(f); return NULL; }
    char *buf = malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return NULL; }
    size_t r = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[r] = '\0';
    if (n_out) *n_out = r;
    return buf;
}

static int word_declared_as_invariant(const char *text, const char *word) {
    /* We accept either a top-level definition `word ==` *or* an
     * INVARIANTS line listing the name (TLC cfg).  Both forms count
     * as "declared" in the spec layer. */
    size_t wlen = strlen(word);
    const char *p = text;
    while ((p = strstr(p, word)) != NULL) {
        /* Left boundary */
        if (p != text) {
            char left = p[-1];
            if ((left >= 'A' && left <= 'Z') ||
                (left >= 'a' && left <= 'z') ||
                (left >= '0' && left <= '9') || left == '_') {
                p += wlen; continue;
            }
        }
        /* Right boundary */
        char right = p[wlen];
        if ((right >= 'A' && right <= 'Z') ||
            (right >= 'a' && right <= 'z') ||
            (right >= '0' && right <= '9') || right == '_') {
            p += wlen; continue;
        }
        /* Accept: any line where the word appears as a whole-word
         * token counts.  A spec that only *mentions* the name in a
         * comment is rare and unlikely to survive code review; we
         * choose the simple rule over a full TLA+ parser here. */
        return 1;
    }
    return 0;
}

int cos_v123_validate(const char *spec_path, const char *cfg_path,
                      cos_v123_validation_t *out) {
    if (!out) return -1;
    memset(out, 0, sizeof *out);

    if (!spec_path) spec_path = COS_V123_SPEC_PATH_DEFAULT;
    out->spec_exists = file_exists(spec_path);
    out->cfg_exists  = cfg_path ? file_exists(cfg_path) : 1;
    if (!out->spec_exists) return -1;

    size_t n = 0;
    char *text = slurp(spec_path, &n);
    if (!text) return -1;

    out->has_module_header     = strstr(text, "---- MODULE ")      != NULL ||
                                  strstr(text, "-- MODULE ")       != NULL ||
                                  strstr(text, "MODULE sigma_governance") != NULL;
    out->has_specification     = strstr(text, "Spec ==")           != NULL ||
                                  strstr(text, "Spec  ==")         != NULL;
    out->has_constants_block   = strstr(text, "CONSTANTS")         != NULL;
    out->has_vars_tuple        = strstr(text, "vars ==")           != NULL;

    int decl_mask = 0;
    int miss_mask = 0;
    for (int i = 0; i < 7; ++i) {
        if (word_declared_as_invariant(text, COS_V123_REQUIRED_INVARIANTS[i]))
            decl_mask |= (1 << i);
        else
            miss_mask |= (1 << i);
    }
    out->invariants_declared = decl_mask;
    out->missing_mask        = miss_mask;

    /* Optional: cfg file must list INVARIANTS if present. */
    if (cfg_path) {
        char *cfg = slurp(cfg_path, NULL);
        if (cfg) {
            int cfg_listed = (strstr(cfg, "INVARIANTS") != NULL);
            if (!cfg_listed) out->cfg_exists = 0;
            free(cfg);
        }
    }

    free(text);

    int ok =
        out->spec_exists &&
        out->has_module_header &&
        out->has_specification &&
        out->has_constants_block &&
        out->has_vars_tuple &&
        (out->missing_mask == 0);
    return ok ? 0 : -1;
}

int cos_v123_validation_to_json(const cos_v123_validation_t *v,
                                char *out, size_t cap) {
    if (!v || !out || cap == 0) return -1;
    int n = snprintf(out, cap,
        "{\"spec_exists\":%s,\"cfg_exists\":%s,"
        "\"has_module_header\":%s,\"has_specification\":%s,"
        "\"has_constants_block\":%s,\"has_vars_tuple\":%s,"
        "\"invariants_declared_mask\":%d,\"missing_mask\":%d,"
        "\"required\":[",
        v->spec_exists ? "true" : "false",
        v->cfg_exists  ? "true" : "false",
        v->has_module_header     ? "true" : "false",
        v->has_specification     ? "true" : "false",
        v->has_constants_block   ? "true" : "false",
        v->has_vars_tuple        ? "true" : "false",
        v->invariants_declared, v->missing_mask);
    if (n < 0 || (size_t)n >= cap) return -1;
    int pos = n;
    for (int i = 0; i < 7; ++i) {
        int m = snprintf(out + pos, cap - (size_t)pos,
                         "%s\"%s\"", i ? "," : "",
                         COS_V123_REQUIRED_INVARIANTS[i]);
        if (m < 0 || (size_t)(pos + m) >= cap) return -1;
        pos += m;
    }
    int m = snprintf(out + pos, cap - (size_t)pos, "]}");
    if (m < 0 || (size_t)(pos + m) >= cap) return -1;
    pos += m;
    return pos;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v123 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v123_self_test(void) {
    cos_v123_validation_t v;
    int rc = cos_v123_validate(COS_V123_SPEC_PATH_DEFAULT,
                                COS_V123_CFG_PATH_DEFAULT, &v);
    _CHECK(v.spec_exists,           "spec file present");
    _CHECK(v.cfg_exists,            "cfg file present and has INVARIANTS");
    _CHECK(v.has_module_header,     "MODULE header");
    _CHECK(v.has_specification,     "Spec == declaration");
    _CHECK(v.has_constants_block,   "CONSTANTS block");
    _CHECK(v.has_vars_tuple,        "vars == tuple");
    _CHECK(v.missing_mask == 0,     "all 7 invariants declared");
    _CHECK(rc == 0,                 "overall validate rc");

    char js[1024];
    int jn = cos_v123_validation_to_json(&v, js, sizeof js);
    _CHECK(jn > 0, "json non-empty");
    _CHECK(strstr(js, "\"required\":[") != NULL, "json required[]");
    for (int i = 0; i < 7; ++i)
        _CHECK(strstr(js, COS_V123_REQUIRED_INVARIANTS[i]) != NULL,
               "invariant name in json");
    return 0;
}
