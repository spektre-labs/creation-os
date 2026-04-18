/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v112 σ-Agent — OpenAI-compatible function calling with σ-gated
 * tool selection.  Implementation.  See tools.h for the API story.
 */
#include "tools.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- */
/* Small lexer helpers — no external JSON dep; we parse the subset  */
/* of OpenAI tools-API shape that matters for the σ-gate.           */
/* ---------------------------------------------------------------- */

static const char *skip_ws(const char *s) {
    while (*s && isspace((unsigned char)*s)) ++s;
    return s;
}

/* Find the first occurrence of `needle` starting at s, skipping
 * content inside string values (characters between a pair of un-
 * escaped `"`).  Needles that themselves begin with `"` match key
 * positions (the state toggle happens AFTER the check). */
static const char *find_unquoted(const char *s, const char *needle) {
    size_t L = strlen(needle);
    int in_str = 0;
    for (; *s; ++s) {
        if (*s == '\\' && s[1]) { ++s; continue; }
        if (!in_str && strncmp(s, needle, L) == 0) return s;
        if (*s == '"') in_str = !in_str;
    }
    return NULL;
}

/* Scan from `start` for a balanced `{...}` object and return the
 * pointer AFTER the closing brace (writes the length to *out_len).
 * Start must point at the opening `{`.  NULL on imbalance. */
static const char *scan_object(const char *start, size_t *out_len) {
    if (*start != '{') return NULL;
    int depth = 0, in_str = 0;
    const char *p = start;
    for (; *p; ++p) {
        if (*p == '\\' && p[1]) { ++p; continue; }
        if (*p == '"') in_str = !in_str;
        if (in_str) continue;
        if (*p == '{') ++depth;
        else if (*p == '}') {
            if (--depth == 0) {
                if (out_len) *out_len = (size_t)(p + 1 - start);
                return p + 1;
            }
        }
    }
    return NULL;
}

/* Copy a JSON string value pointed at by `src` (which must be the
 * opening `"` of the value) into `out` without surrounding quotes
 * and with a minimal escape set (\\, \", \n, \t).  Returns bytes
 * written, -1 on overflow or malformed. */
static int copy_json_string(const char *src, char *out, size_t cap) {
    if (*src != '"') return -1;
    size_t w = 0;
    const char *p = src + 1;
    while (*p && *p != '"') {
        if (w + 1 >= cap) return -1;
        if (*p == '\\' && p[1]) {
            char c = p[1];
            switch (c) {
                case 'n': out[w++] = '\n'; break;
                case 't': out[w++] = '\t'; break;
                case 'r': out[w++] = '\r'; break;
                case '"': out[w++] = '"'; break;
                case '\\': out[w++] = '\\'; break;
                case '/': out[w++] = '/'; break;
                default:  out[w++] = c; break;
            }
            p += 2;
        } else {
            out[w++] = *p++;
        }
    }
    if (*p != '"') return -1;
    out[w] = '\0';
    return (int)w;
}

/* Look up `"key":"value"` (string-valued) inside the JSON object
 * [start, start+len).  Writes the value into `out` (unescaped) and
 * returns bytes written, or -1 if not found. */
static int lookup_string_field(const char *start, size_t len,
                               const char *key, char *out, size_t cap) {
    char pattern[96];
    int n = snprintf(pattern, sizeof pattern, "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof pattern) return -1;
    const char *end = start + len;
    const char *p = start;
    while (p < end) {
        const char *hit = strstr(p, pattern);
        if (!hit || hit >= end) return -1;
        const char *colon = hit + strlen(pattern);
        while (colon < end && isspace((unsigned char)*colon)) ++colon;
        if (colon >= end || *colon != ':') { p = hit + 1; continue; }
        ++colon;
        while (colon < end && isspace((unsigned char)*colon)) ++colon;
        if (colon >= end || *colon != '"') { p = hit + 1; continue; }
        return copy_json_string(colon, out, cap);
    }
    return -1;
}

/* Look up `"key": { ... }` (object-valued) — writes the object
 * literal (braces included) into `out`. */
static int lookup_object_field(const char *start, size_t len,
                               const char *key, char *out, size_t cap) {
    char pattern[96];
    int n = snprintf(pattern, sizeof pattern, "\"%s\"", key);
    if (n < 0 || (size_t)n >= sizeof pattern) return -1;
    const char *end = start + len;
    const char *p = start;
    while (p < end) {
        const char *hit = strstr(p, pattern);
        if (!hit || hit >= end) return -1;
        const char *colon = hit + strlen(pattern);
        while (colon < end && isspace((unsigned char)*colon)) ++colon;
        if (colon >= end || *colon != ':') { p = hit + 1; continue; }
        ++colon;
        while (colon < end && isspace((unsigned char)*colon)) ++colon;
        if (colon >= end || *colon != '{') { p = hit + 1; continue; }
        size_t olen = 0;
        const char *after = scan_object(colon, &olen);
        if (!after) return -1;
        if (olen + 1 > cap) return -1;
        memcpy(out, colon, olen);
        out[olen] = '\0';
        return (int)olen;
    }
    return -1;
}

/* ---------------------------------------------------------------- */
/* Public: parse tools[] ------------------------------------------ */
/* ---------------------------------------------------------------- */

int cos_v112_parse_tools(const char *body,
                         cos_v112_tool_def_t *out, size_t cap)
{
    if (!body || !out || cap == 0) return 0;
    const char *arr = find_unquoted(body, "\"tools\"");
    if (!arr) return 0;
    arr = strchr(arr, '[');
    if (!arr) return 0;
    ++arr;
    size_t n = 0;
    while (*arr && n < cap) {
        arr = skip_ws(arr);
        if (*arr == ']') break;
        if (*arr != '{') { ++arr; continue; }
        size_t obj_len = 0;
        const char *next = scan_object(arr, &obj_len);
        if (!next) break;

        char function_obj[COS_V112_TOOL_PARAMS_MAX];
        int fo = lookup_object_field(arr, obj_len, "function",
                                     function_obj, sizeof function_obj);
        if (fo > 0) {
            cos_v112_tool_def_t *t = &out[n];
            memset(t, 0, sizeof *t);
            lookup_string_field(function_obj, (size_t)fo, "name",
                                t->name, sizeof t->name);
            lookup_string_field(function_obj, (size_t)fo, "description",
                                t->description, sizeof t->description);
            lookup_object_field(function_obj, (size_t)fo, "parameters",
                                t->parameters_json,
                                sizeof t->parameters_json);
            if (t->name[0]) ++n;
        }
        arr = next;
        arr = skip_ws(arr);
        if (*arr == ',') ++arr;
    }
    return (int)n;
}

int cos_v112_parse_tool_choice(const char *body,
                               cos_v112_tool_choice_t *out)
{
    if (!out) return -1;
    out->mode = COS_V112_TC_AUTO;
    out->forced_name[0] = '\0';
    if (!body) return 0;
    const char *hit = find_unquoted(body, "\"tool_choice\"");
    if (!hit) return 0;
    const char *colon = strchr(hit, ':');
    if (!colon) return 0;
    ++colon;
    colon = skip_ws(colon);
    if (*colon == '"') {
        char val[32];
        if (copy_json_string(colon, val, sizeof val) < 0) return 0;
        if (strcmp(val, "auto") == 0)     out->mode = COS_V112_TC_AUTO;
        else if (strcmp(val, "none") == 0) out->mode = COS_V112_TC_NONE;
        else                                out->mode = COS_V112_TC_AUTO;
        return 0;
    }
    if (*colon == '{') {
        size_t olen = 0;
        if (!scan_object(colon, &olen)) return 0;
        char fn_obj[256];
        int fo = lookup_object_field(colon, olen, "function",
                                     fn_obj, sizeof fn_obj);
        if (fo > 0) {
            if (lookup_string_field(fn_obj, (size_t)fo, "name",
                                    out->forced_name,
                                    sizeof out->forced_name) > 0) {
                out->mode = COS_V112_TC_FORCED;
                return 0;
            }
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* Parse <tool_call>{...}</tool_call> out of model output text.     */
/* ---------------------------------------------------------------- */

static int try_parse_tool_call_json(const char *obj_start, size_t obj_len,
                                    cos_v112_tool_call_report_t *r)
{
    /* Accepted shapes:
     *   {"name":"foo","arguments":{…}}
     *   {"name":"foo","arguments":"{…}"}  (stringified JSON)
     *   {"tool_call":{"name":"foo","arguments":{…}}}
     *   {"function":"foo","arguments":{…}}
     */
    char inner_obj[COS_V112_TOOL_ARGS_MAX];
    int inner_len = lookup_object_field(obj_start, obj_len, "tool_call",
                                        inner_obj, sizeof inner_obj);
    const char *start  = obj_start;
    size_t      length = obj_len;
    if (inner_len > 0) {
        start  = inner_obj;
        length = (size_t)inner_len;
    }

    if (lookup_string_field(start, length, "name",
                            r->tool_name, sizeof r->tool_name) <= 0 &&
        lookup_string_field(start, length, "function",
                            r->tool_name, sizeof r->tool_name) <= 0) {
        return -1;
    }

    int got_args_obj = lookup_object_field(start, length, "arguments",
                                           r->tool_arguments_json,
                                           sizeof r->tool_arguments_json);
    if (got_args_obj <= 0) {
        int sv = lookup_string_field(start, length, "arguments",
                                     r->tool_arguments_json,
                                     sizeof r->tool_arguments_json);
        if (sv <= 0) {
            /* No arguments → treat as {} */
            r->tool_arguments_json[0] = '{';
            r->tool_arguments_json[1] = '}';
            r->tool_arguments_json[2] = '\0';
        }
    }
    return 0;
}

int cos_v112_parse_tool_call_text(const char *text,
                                  cos_v112_tool_call_report_t *r)
{
    if (!text || !r) return -1;

    /* Try <tool_call>...</tool_call> first. */
    const char *open = strstr(text, "<tool_call>");
    const char *close_tag = "</tool_call>";
    if (!open) {
        open = strstr(text, "<function_call>");
        close_tag = "</function_call>";
    }
    const char *json_start = NULL;
    size_t      json_len   = 0;
    if (open) {
        const char *obj = strchr(open, '{');
        const char *close = strstr(open, close_tag);
        if (!obj || !close || obj >= close) return -1;
        size_t olen = 0;
        const char *after = scan_object(obj, &olen);
        if (!after || after > close) return -1;
        json_start = obj;
        json_len   = olen;
    } else {
        /* Fallback: top-level JSON in the output. */
        const char *obj = strchr(text, '{');
        if (!obj) return 0;
        size_t olen = 0;
        if (!scan_object(obj, &olen)) return 0;
        /* Only accept if the object contains "name" or "tool_call" as
         * a first-class key. */
        if (!find_unquoted(obj, "\"name\"") &&
            !find_unquoted(obj, "\"tool_call\"") &&
            !find_unquoted(obj, "\"function\"")) return 0;
        json_start = obj;
        json_len   = olen;
    }

    if (try_parse_tool_call_json(json_start, json_len, r) != 0) return -1;
    r->tool_called = 1;
    /* synthesize a deterministic-ish id */
    snprintf(r->tool_call_id, sizeof r->tool_call_id,
             "call_cos_%s", r->tool_name[0] ? r->tool_name : "anon");
    return 1;
}

/* ---------------------------------------------------------------- */
/* σ-gate decision                                                  */
/* ---------------------------------------------------------------- */

void cos_v112_opts_defaults(cos_v112_opts_t *opts)
{
    if (!opts) return;
    opts->tau_tool     = 0.65f;
    opts->max_tokens   = 256;
    opts->always_parse = 0;
}

static float _sigma_product(const float profile[8])
{
    double acc = 1.0;
    int    n   = 0;
    for (int i = 0; i < 8; ++i) {
        if (profile[i] < 0.f) continue;
        double v = profile[i];
        if (v < 1e-6) v = 1e-6;
        acc *= v;
        ++n;
    }
    if (n == 0) return 0.f;
    return (float)pow(acc, 1.0 / (double)n);
}

static float _sigma_mean(const float profile[8])
{
    double acc = 0.0;
    int    n   = 0;
    for (int i = 0; i < 8; ++i) {
        if (profile[i] < 0.f) continue;
        acc += profile[i];
        ++n;
    }
    return (n == 0) ? 0.f : (float)(acc / n);
}

static cos_v112_collapsed_channel_t _most_collapsed(const float profile[8])
{
    /* Return the index of the channel with the HIGHEST σ (the one
     * that collapsed the most; our convention: σ=1 is "maximally
     * confused", σ=0 is "sharp"). */
    int   best_i = -1;
    float best_v = -1.f;
    for (int i = 0; i < 8; ++i) {
        if (profile[i] > best_v) { best_v = profile[i]; best_i = i; }
    }
    return (cos_v112_collapsed_channel_t)best_i;
}

static const char *_channel_label(cos_v112_collapsed_channel_t c)
{
    switch (c) {
        case COS_V112_CHAN_ENTROPY:     return "entropy";
        case COS_V112_CHAN_SIGMA_MEAN:  return "sigma_mean";
        case COS_V112_CHAN_MAX_TOKEN:   return "max_token";
        case COS_V112_CHAN_PRODUCT:     return "product";
        case COS_V112_CHAN_TAIL_MASS:   return "tail_mass";
        case COS_V112_CHAN_N_EFFECTIVE: return "n_effective";
        case COS_V112_CHAN_MARGIN:      return "margin";
        case COS_V112_CHAN_STABILITY:   return "stability";
        default:                        return "none";
    }
}

/* ---------------------------------------------------------------- */
/* Render manifest prompt                                            */
/* ---------------------------------------------------------------- */

int cos_v112_render_tool_manifest(const cos_v112_tool_def_t *tools,
                                  size_t n_tools,
                                  const cos_v112_tool_choice_t *choice,
                                  char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    int w = 0;
    int n = snprintf(out + w, cap - w,
                     "You have access to the following tools.  "
                     "If one applies, reply with exactly one line:\n"
                     "<tool_call>{\"name\":\"TOOL_NAME\","
                     "\"arguments\":{...}}</tool_call>\n"
                     "Otherwise, answer in natural language.\n\n"
                     "Tools:\n");
    if (n < 0 || (size_t)n >= cap - w) return -1;
    w += n;
    for (size_t i = 0; i < n_tools; ++i) {
        n = snprintf(out + w, cap - w,
                     "- %s: %s\n  parameters: %s\n",
                     tools[i].name[0] ? tools[i].name : "(unnamed)",
                     tools[i].description[0] ? tools[i].description : "(no description)",
                     tools[i].parameters_json[0] ? tools[i].parameters_json : "{}");
        if (n < 0 || (size_t)n >= cap - w) return -1;
        w += n;
    }
    if (choice && choice->mode == COS_V112_TC_NONE) {
        n = snprintf(out + w, cap - w,
                     "\nPolicy: you MUST NOT call any tool; answer in text.\n");
        if (n > 0 && (size_t)n < cap - w) w += n;
    } else if (choice && choice->mode == COS_V112_TC_FORCED &&
               choice->forced_name[0]) {
        n = snprintf(out + w, cap - w,
                     "\nPolicy: you MUST call the tool `%s`.\n",
                     choice->forced_name);
        if (n > 0 && (size_t)n < cap - w) w += n;
    } else {
        n = snprintf(out + w, cap - w,
                     "\nPolicy: use a tool only if one clearly applies.\n");
        if (n > 0 && (size_t)n < cap - w) w += n;
    }
    return w;
}

/* ---------------------------------------------------------------- */
/* Driver                                                             */
/* ---------------------------------------------------------------- */

int cos_v112_run_tool_selection(cos_v101_bridge_t *bridge,
                                const char *user_prompt,
                                const cos_v112_tool_def_t *tools,
                                size_t n_tools,
                                const cos_v112_tool_choice_t *choice,
                                const cos_v112_opts_t *opts_in,
                                cos_v112_tool_call_report_t *r)
{
    if (!r || !user_prompt) return -1;
    memset(r, 0, sizeof *r);
    r->collapsed = COS_V112_CHAN_NONE;

    cos_v112_opts_t opts;
    cos_v112_opts_defaults(&opts);
    if (opts_in) opts = *opts_in;

    if (!bridge) {
        /* Stub mode — deterministic, useful for tests. */
        snprintf(r->raw_output, sizeof r->raw_output,
                 "[stub] no model loaded; σ-gate abstained by contract.");
        r->abstained = 1;
        r->tau_tool  = opts.tau_tool;
        r->sigma_product = 1.0f;
        r->sigma_mean    = 1.0f;
        snprintf(r->abstain_reason, sizeof r->abstain_reason,
                 "no_model_loaded");
        return 0;
    }

    /* Compose manifest + user prompt. */
    char manifest[8192];
    int  mlen = cos_v112_render_tool_manifest(tools, n_tools, choice,
                                              manifest, sizeof manifest);
    if (mlen < 0) return -1;

    char full_prompt[16384];
    int  plen = snprintf(full_prompt, sizeof full_prompt,
                         "%s\nUser: %s\nAssistant:",
                         manifest, user_prompt);
    if (plen < 0 || (size_t)plen >= sizeof full_prompt) return -1;

    float profile[8] = {0};
    int   abstained_bridge = 0;
    const char *stops[] = {"</tool_call>", "</function_call>", "\nUser:"};
    int out_n = cos_v101_bridge_generate(bridge, full_prompt,
                                         stops, 3,
                                         opts.max_tokens,
                                         /*sigma_threshold*/ 0.f,
                                         r->raw_output, sizeof r->raw_output,
                                         profile, &abstained_bridge);
    if (out_n < 0) return -1;

    /* If the bridge stopped at </tool_call>, append it so our parser
     * has the closing tag to key on. */
    if (strstr(r->raw_output, "<tool_call>") &&
        !strstr(r->raw_output, "</tool_call>")) {
        size_t cur = strlen(r->raw_output);
        if (cur + 14 < sizeof r->raw_output) {
            memcpy(r->raw_output + cur, "</tool_call>", 13);
            r->raw_output[cur + 12] = '\0';
        }
    }

    /* σ telemetry. */
    memcpy(r->sigma_profile, profile, sizeof r->sigma_profile);
    r->sigma_mean      = _sigma_mean(profile);
    r->sigma_product   = _sigma_product(profile);
    r->sigma_max_token = profile[COS_V112_CHAN_MAX_TOKEN];
    r->collapsed       = _most_collapsed(profile);

    /* Parse tool call if present. */
    if (opts.always_parse ||
        strstr(r->raw_output, "<tool_call>") ||
        strstr(r->raw_output, "<function_call>") ||
        strstr(r->raw_output, "\"tool_call\"") ||
        strstr(r->raw_output, "\"function\"")) {
        cos_v112_parse_tool_call_text(r->raw_output, r);
    }

    /* Gate. */
    r->tau_tool = opts.tau_tool;
    if (r->tool_called && r->sigma_product > opts.tau_tool) {
        r->abstained = 1;
        snprintf(r->abstain_reason, sizeof r->abstain_reason,
                 "low_confidence_tool_selection (channel=%s, σ=%.3f>τ=%.2f)",
                 _channel_label(r->collapsed), r->sigma_product, opts.tau_tool);
    } else if (choice && choice->mode == COS_V112_TC_FORCED &&
               !r->tool_called) {
        r->abstained = 1;
        snprintf(r->abstain_reason, sizeof r->abstain_reason,
                 "forced_tool_not_emitted");
    }
    return 0;
}

/* ---------------------------------------------------------------- */
/* JSON output                                                       */
/* ---------------------------------------------------------------- */

static int json_escape_into(const char *src, char *dst, size_t cap)
{
    size_t w = 0;
    for (const char *p = src; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if (w + 7 >= cap) return -1;
        switch (c) {
            case '"':  dst[w++] = '\\'; dst[w++] = '"'; break;
            case '\\': dst[w++] = '\\'; dst[w++] = '\\'; break;
            case '\n': dst[w++] = '\\'; dst[w++] = 'n'; break;
            case '\r': dst[w++] = '\\'; dst[w++] = 'r'; break;
            case '\t': dst[w++] = '\\'; dst[w++] = 't'; break;
            default:
                if (c < 0x20) {
                    int n = snprintf(dst + w, cap - w, "\\u%04x", c);
                    if (n < 0 || (size_t)n >= cap - w) return -1;
                    w += (size_t)n;
                } else {
                    dst[w++] = (char)c;
                }
        }
    }
    dst[w] = '\0';
    return (int)w;
}

int cos_v112_report_to_message_json(const cos_v112_tool_call_report_t *r,
                                    char *out, size_t cap)
{
    if (!r || !out) return -1;
    int w = 0;
    int n;

    if (r->tool_called && !r->abstained) {
        /* Standard OpenAI tool_calls shape. */
        char args_esc[COS_V112_TOOL_ARGS_MAX];
        /* arguments stringified in the OpenAI spec */
        if (json_escape_into(r->tool_arguments_json,
                             args_esc, sizeof args_esc) < 0) return -1;
        n = snprintf(out + w, cap - w,
                     "{\"role\":\"assistant\",\"content\":null,"
                     "\"tool_calls\":[{"
                     "\"id\":\"%s\","
                     "\"type\":\"function\","
                     "\"function\":{\"name\":\"%s\","
                     "\"arguments\":\"%s\"}}],"
                     "\"sigma_product\":%.4f,"
                     "\"sigma_mean\":%.4f,"
                     "\"cos_abstained\":false"
                     "}",
                     r->tool_call_id, r->tool_name, args_esc,
                     r->sigma_product, r->sigma_mean);
        if (n < 0 || (size_t)n >= cap - w) return -1;
        return n;
    }

    if (r->abstained) {
        char reason_esc[128];
        json_escape_into(r->abstain_reason, reason_esc, sizeof reason_esc);
        if (r->tool_called) {
            /* Model DID pick a tool, but σ was too high. */
            char args_esc[COS_V112_TOOL_ARGS_MAX];
            json_escape_into(r->tool_arguments_json,
                             args_esc, sizeof args_esc);
            n = snprintf(out + w, cap - w,
                         "{\"role\":\"assistant\",\"content\":null,"
                         "\"cos_abstained\":true,"
                         "\"cos_abstain_reason\":\"%s\","
                         "\"cos_sigma_product\":%.4f,"
                         "\"cos_sigma_mean\":%.4f,"
                         "\"cos_collapsed_channel\":\"%s\","
                         "\"cos_parsed_tool\":{"
                         "\"name\":\"%s\",\"arguments\":\"%s\"}"
                         "}",
                         reason_esc, r->sigma_product, r->sigma_mean,
                         _channel_label(r->collapsed),
                         r->tool_name, args_esc);
        } else {
            n = snprintf(out + w, cap - w,
                         "{\"role\":\"assistant\",\"content\":null,"
                         "\"cos_abstained\":true,"
                         "\"cos_abstain_reason\":\"%s\","
                         "\"cos_sigma_product\":%.4f,"
                         "\"cos_sigma_mean\":%.4f,"
                         "\"cos_collapsed_channel\":\"%s\""
                         "}",
                         reason_esc, r->sigma_product, r->sigma_mean,
                         _channel_label(r->collapsed));
        }
        if (n < 0 || (size_t)n >= cap - w) return -1;
        return n;
    }

    /* No tool, no abstention → plain text reply. */
    char content_esc[2048];
    if (json_escape_into(r->raw_output, content_esc,
                         sizeof content_esc) < 0) return -1;
    n = snprintf(out + w, cap - w,
                 "{\"role\":\"assistant\",\"content\":\"%s\","
                 "\"sigma_product\":%.4f,"
                 "\"sigma_mean\":%.4f,"
                 "\"cos_abstained\":false}",
                 content_esc, r->sigma_product, r->sigma_mean);
    if (n < 0 || (size_t)n >= cap - w) return -1;
    return n;
}

/* ---------------------------------------------------------------- */
/* Self-test                                                          */
/* ---------------------------------------------------------------- */

#define _CHECK(cond, msg) do {                                        \
    if (!(cond)) { fprintf(stderr, "FAIL %s\n", (msg)); ++_fail; }     \
    else                                                              \
        ++_pass;                                                      \
} while (0)

int cos_v112_self_test(void)
{
    int _pass = 0, _fail = 0;

    /* 1. parse_tools on two-tool body. */
    const char *body1 =
        "{\"messages\":[],\"tools\":["
        "{\"type\":\"function\",\"function\":{"
            "\"name\":\"weather\","
            "\"description\":\"Get current weather for a location.\","
            "\"parameters\":{\"type\":\"object\",\"properties\":{"
                "\"location\":{\"type\":\"string\"}}}}},"
        "{\"type\":\"function\",\"function\":{"
            "\"name\":\"add\","
            "\"description\":\"Add two numbers.\","
            "\"parameters\":{\"type\":\"object\",\"properties\":{"
                "\"a\":{\"type\":\"number\"},\"b\":{\"type\":\"number\"}}}}}"
        "]}";
    cos_v112_tool_def_t tools[4];
    int nt = cos_v112_parse_tools(body1, tools, 4);
    _CHECK(nt == 2, "parse_tools: count");
    _CHECK(strcmp(tools[0].name, "weather") == 0, "parse_tools[0].name");
    _CHECK(strcmp(tools[1].name, "add") == 0, "parse_tools[1].name");
    _CHECK(strstr(tools[0].description, "weather") != NULL,
           "parse_tools[0].description");
    _CHECK(strstr(tools[0].parameters_json, "location") != NULL,
           "parse_tools[0].parameters");

    /* 2. tool_choice: string + object. */
    cos_v112_tool_choice_t ch;
    cos_v112_parse_tool_choice("{\"tool_choice\":\"auto\"}", &ch);
    _CHECK(ch.mode == COS_V112_TC_AUTO, "tool_choice auto");
    cos_v112_parse_tool_choice("{\"tool_choice\":\"none\"}", &ch);
    _CHECK(ch.mode == COS_V112_TC_NONE, "tool_choice none");
    cos_v112_parse_tool_choice(
        "{\"tool_choice\":{\"type\":\"function\","
        "\"function\":{\"name\":\"weather\"}}}", &ch);
    _CHECK(ch.mode == COS_V112_TC_FORCED, "tool_choice forced mode");
    _CHECK(strcmp(ch.forced_name, "weather") == 0, "tool_choice forced name");

    /* 3. parse_tool_call_text — the happy path. */
    cos_v112_tool_call_report_t r;
    memset(&r, 0, sizeof r);
    int pc = cos_v112_parse_tool_call_text(
        "<tool_call>{\"name\":\"weather\",\"arguments\":"
        "{\"location\":\"Helsinki\"}}</tool_call>", &r);
    _CHECK(pc == 1, "parse_tool_call_text returns 1");
    _CHECK(strcmp(r.tool_name, "weather") == 0, "tool_name parsed");
    _CHECK(strstr(r.tool_arguments_json, "Helsinki") != NULL,
           "tool_arguments_json");

    /* 4. parse_tool_call_text — no tag, plain text. */
    memset(&r, 0, sizeof r);
    int pc2 = cos_v112_parse_tool_call_text(
        "The capital of Finland is Helsinki.", &r);
    _CHECK(pc2 == 0, "plain text → no tool_call");

    /* 5. parse_tool_call_text — fallback {"function":...}. */
    memset(&r, 0, sizeof r);
    int pc3 = cos_v112_parse_tool_call_text(
        "{\"function\":\"add\",\"arguments\":{\"a\":2,\"b\":3}}", &r);
    _CHECK(pc3 == 1, "fallback {function}");
    _CHECK(strcmp(r.tool_name, "add") == 0, "fallback name");

    /* 6. σ-gate: stub run (no bridge) always abstains honestly. */
    cos_v112_tool_call_report_t r2;
    int rc = cos_v112_run_tool_selection(NULL, "hi", tools, 2, NULL, NULL,
                                         &r2);
    _CHECK(rc == 0, "stub run returns 0");
    _CHECK(r2.abstained == 1, "stub run abstains");
    _CHECK(strcmp(r2.abstain_reason, "no_model_loaded") == 0,
           "stub abstain reason");

    /* 7. Manifest render. */
    char manif[2048];
    int  mlen = cos_v112_render_tool_manifest(tools, 2, NULL,
                                              manif, sizeof manif);
    _CHECK(mlen > 0, "manifest renders");
    _CHECK(strstr(manif, "weather") != NULL, "manifest contains weather");
    _CHECK(strstr(manif, "add") != NULL, "manifest contains add");

    /* 8. Report → JSON: tool_called + not abstained. */
    cos_v112_tool_call_report_t r3;
    memset(&r3, 0, sizeof r3);
    r3.tool_called   = 1;
    r3.sigma_product = 0.22f;
    r3.sigma_mean    = 0.20f;
    snprintf(r3.tool_name, sizeof r3.tool_name, "weather");
    snprintf(r3.tool_arguments_json, sizeof r3.tool_arguments_json,
             "{\"location\":\"Helsinki\"}");
    snprintf(r3.tool_call_id, sizeof r3.tool_call_id, "call_cos_weather");
    char js[2048];
    int  jl = cos_v112_report_to_message_json(&r3, js, sizeof js);
    _CHECK(jl > 0, "report_to_message_json ok");
    _CHECK(strstr(js, "\"tool_calls\"") != NULL, "tool_calls emitted");
    _CHECK(strstr(js, "Helsinki") != NULL, "arguments in json");
    _CHECK(strstr(js, "cos_abstained\":false") != NULL, "abstained flag");

    /* 9. Report → JSON: abstained with parsed tool (diagnostic shape). */
    memset(&r3, 0, sizeof r3);
    r3.tool_called   = 1;
    r3.abstained     = 1;
    r3.sigma_product = 0.83f;
    r3.sigma_mean    = 0.80f;
    r3.collapsed     = COS_V112_CHAN_N_EFFECTIVE;
    snprintf(r3.tool_name, sizeof r3.tool_name, "execute_code");
    snprintf(r3.tool_arguments_json, sizeof r3.tool_arguments_json,
             "{\"code\":\"???\"}");
    snprintf(r3.abstain_reason, sizeof r3.abstain_reason,
             "low_confidence_tool_selection");
    jl = cos_v112_report_to_message_json(&r3, js, sizeof js);
    _CHECK(jl > 0, "abstain json ok");
    _CHECK(strstr(js, "cos_abstained\":true") != NULL, "abstain true");
    _CHECK(strstr(js, "n_effective") != NULL, "channel label");
    _CHECK(strstr(js, "cos_parsed_tool") != NULL, "parsed tool kept");

    if (_fail > 0) {
        fprintf(stderr, "v112 self-test: %d PASS / %d FAIL\n", _pass, _fail);
        return 1;
    }
    printf("v112 self-test: %d PASS / 0 FAIL\n", _pass);
    return 0;
}
