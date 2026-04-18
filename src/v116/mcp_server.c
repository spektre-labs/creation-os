/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v116 σ-MCP — minimal JSON-RPC 2.0 dispatcher + stdio loop.
 *
 * The JSON parser here is intentionally scoped to the MCP shape we
 * emit/accept: one top-level object, no deep nesting beyond two
 * levels, values are strings / numbers / booleans / objects.  This
 * keeps the kernel zero-dep and matches the discipline used by
 * v106/v112/v114.
 */

#include "mcp_server.h"
#include "../v115/memory.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

void cos_v116_config_defaults(cos_v116_config_t *cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->server_name    = "creation-os";
    cfg->server_version = "0.116.0";
    cfg->transport      = "stdio";
}

/* ------------------------------------------------------------------ */
/* JSON helpers (minimal, local to v116)                               */
/* ------------------------------------------------------------------ */

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
        ++p;
    return p;
}

/* Locate `"key":` at the top level of an object starting at `obj`.
 * Returns pointer to the character after ':' (start of the value) or
 * NULL if not found.  Honours string escapes and nested braces/
 * brackets.  `obj` must point at the '{' or just after it; the
 * search terminates at the matching '}'. */
static const char *find_key(const char *obj, const char *end,
                            const char *key) {
    if (!obj || obj >= end) return NULL;
    const char *p = obj;
    if (*p == '{') ++p;
    size_t klen = strlen(key);
    int depth = 0;
    while (p < end) {
        p = skip_ws(p, end);
        if (p >= end) break;
        if (*p == '}' && depth == 0) return NULL;

        /* Expect either a quoted key or a comma. */
        if (*p == ',') { ++p; continue; }
        if (*p != '"') { ++p; continue; }
        const char *ks = p + 1;
        const char *ke = ks;
        while (ke < end && *ke != '"') {
            if (*ke == '\\' && ke + 1 < end) ke += 2;
            else ++ke;
        }
        if (ke >= end) return NULL;
        int match = ((size_t)(ke - ks) == klen) && !memcmp(ks, key, klen);
        p = ke + 1;
        p = skip_ws(p, end);
        if (p >= end || *p != ':') return NULL;
        ++p;
        p = skip_ws(p, end);
        if (match) return p;
        /* Skip the value: handle strings, numbers, objects, arrays. */
        if (*p == '"') {
            ++p;
            while (p < end && *p != '"') {
                if (*p == '\\' && p + 1 < end) p += 2;
                else ++p;
            }
            if (p < end) ++p;
        } else if (*p == '{' || *p == '[') {
            char open = *p;
            char close = (open == '{') ? '}' : ']';
            int d = 1;
            ++p;
            while (p < end && d > 0) {
                if (*p == '"') {
                    ++p;
                    while (p < end && *p != '"') {
                        if (*p == '\\' && p + 1 < end) p += 2;
                        else ++p;
                    }
                    if (p < end) ++p;
                } else if (*p == open) { ++d; ++p; }
                else if (*p == close) { --d; ++p; }
                else ++p;
            }
        } else {
            while (p < end && *p != ',' && *p != '}' && *p != ']') ++p;
        }
        (void)depth;
    }
    return NULL;
}

/* Copy a JSON string value starting at `v` (which points at '"') into
 * `out`.  Returns pointer to the character after the closing quote,
 * or NULL on error.  Minimal unescape (common escapes only). */
static const char *read_str(const char *v, const char *end,
                            char *out, size_t cap) {
    if (!v || v >= end || *v != '"' || cap == 0) return NULL;
    ++v;
    size_t o = 0;
    while (v < end && *v != '"') {
        if (*v == '\\' && v + 1 < end) {
            char c = v[1];
            char out_c = c;
            switch (c) {
            case 'n': out_c = '\n'; break;
            case 'r': out_c = '\r'; break;
            case 't': out_c = '\t'; break;
            case 'b': out_c = '\b'; break;
            case 'f': out_c = '\f'; break;
            case '"':
            case '\\':
            case '/': out_c = c; break;
            case 'u':
                /* ignore \uXXXX — replace with '?' for now */
                out_c = '?';
                if (v + 5 < end) { v += 4; }
                break;
            default:  out_c = c;
            }
            if (o + 1 < cap) out[o++] = out_c;
            v += 2;
        } else {
            if (o + 1 < cap) out[o++] = *v;
            ++v;
        }
    }
    if (v >= end) return NULL;
    out[o < cap ? o : cap - 1] = '\0';
    return v + 1;
}

/* Extract "method" and "id" from a JSON-RPC 2.0 request string.
 * `id_out` is written as a raw JSON snippet (e.g. "5", "\"abc\"")
 * — the responder splices it back verbatim.  Returns 1 if a method
 * was found. */
typedef struct {
    char  method[64];
    char  id_raw[128];
    int   has_id;
    const char *params;     /* start of params value, or NULL */
    const char *params_end; /* end of buffer */
} mcp_req_t;

static int parse_request(const char *buf, size_t len, mcp_req_t *r) {
    const char *end = buf + len;
    memset(r, 0, sizeof *r);
    const char *m = find_key(buf, end, "method");
    if (!m) return 0;
    if (!read_str(m, end, r->method, sizeof r->method)) return 0;

    const char *id = find_key(buf, end, "id");
    if (id) {
        const char *s = skip_ws(id, end);
        const char *e = s;
        int depth = 0;
        while (e < end) {
            if ((*e == ',' || *e == '}') && depth == 0) break;
            if (*e == '{' || *e == '[') ++depth;
            if (*e == '}' || *e == ']') --depth;
            ++e;
        }
        size_t n = (size_t)(e - s);
        if (n >= sizeof r->id_raw) n = sizeof r->id_raw - 1;
        memcpy(r->id_raw, s, n);
        r->id_raw[n] = '\0';
        /* trim trailing whitespace */
        while (n > 0 && (r->id_raw[n - 1] == ' ' || r->id_raw[n - 1] == '\t'))
            r->id_raw[--n] = '\0';
        r->has_id = (n > 0);
    }
    r->params     = find_key(buf, end, "params");
    r->params_end = end;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Writer helpers                                                      */
/* ------------------------------------------------------------------ */

static int appendf(char *buf, size_t cap, size_t *off, const char *fmt, ...) {
    if (!buf || *off >= cap) return -1;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *off, cap - *off, fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= cap - *off) return -1;
    *off += (size_t)n;
    return 0;
}

static int append_esc(char *buf, size_t cap, size_t *off, const char *s) {
    if (!buf || *off >= cap) return -1;
    size_t o = *off;
    if (o + 1 >= cap) return -1;
    buf[o++] = '"';
    for (const char *p = s ? s : ""; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        const char *esc = NULL;
        char small[8];
        switch (c) {
        case '"':  esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\n': esc = "\\n";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        default:
            if (c < 0x20) {
                snprintf(small, sizeof small, "\\u%04x", c);
                esc = small;
            }
        }
        if (esc) {
            size_t el = strlen(esc);
            if (o + el + 1 >= cap) return -1;
            memcpy(buf + o, esc, el);
            o += el;
        } else {
            if (o + 2 >= cap) return -1;
            buf[o++] = (char)c;
        }
    }
    if (o + 2 >= cap) return -1;
    buf[o++] = '"';
    buf[o]   = '\0';
    *off = o;
    return 0;
}

static int write_error(char *out, size_t cap, const char *id_raw,
                       int code, const char *msg, const char *data_json) {
    size_t o = 0;
    if (appendf(out, cap, &o,
                "{\"jsonrpc\":\"2.0\",\"id\":%s,\"error\":{\"code\":%d,\"message\":",
                (id_raw && *id_raw) ? id_raw : "null", code) < 0) return -1;
    if (append_esc(out, cap, &o, msg) < 0) return -1;
    if (data_json && *data_json) {
        if (appendf(out, cap, &o, ",\"data\":%s", data_json) < 0) return -1;
    }
    if (appendf(out, cap, &o, "}}") < 0) return -1;
    return (int)o;
}

static int write_result_prefix(char *out, size_t cap, size_t *off,
                               const char *id_raw) {
    return appendf(out, cap, off,
                   "{\"jsonrpc\":\"2.0\",\"id\":%s,\"result\":",
                   (id_raw && *id_raw) ? id_raw : "null");
}

/* ------------------------------------------------------------------ */
/* Static primitive catalog                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    const char *description;
    const char *input_schema_json; /* pre-minified JSON Schema */
} tool_desc_t;

static const tool_desc_t kTools[] = {
    { "cos_chat",
      "σ-gated chat completion: returns a model reply plus the 8-channel "
      "σ-profile and an abstain flag when σ_product > τ.",
      "{\"type\":\"object\",\"properties\":"
      "{\"prompt\":{\"type\":\"string\"},"
      "\"model\":{\"type\":\"string\"}},\"required\":[\"prompt\"]}"
    },
    { "cos_reason",
      "Run the v111.2 σ-Reason multi-path MCTS loop; abstains with a "
      "diagnostic if σ_product exceeds τ_reason on the chosen waypoint.",
      "{\"type\":\"object\",\"properties\":"
      "{\"prompt\":{\"type\":\"string\"},"
      "\"max_paths\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":8}},"
      "\"required\":[\"prompt\"]}"
    },
    { "cos_memory_search",
      "Retrieve σ-weighted episodic memories from the Creation OS SQLite "
      "store.  Uncertain memories are automatically down-ranked.",
      "{\"type\":\"object\",\"properties\":"
      "{\"query\":{\"type\":\"string\"},"
      "\"top_k\":{\"type\":\"integer\",\"minimum\":1,\"maximum\":32}},"
      "\"required\":[\"query\"]}"
    },
    { "cos_sandbox_execute",
      "Run code inside the v113 σ-Sandbox (subprocess + rlimit + deadline). "
      "Refuses to execute if sigma_product exceeds τ_code.",
      "{\"type\":\"object\",\"properties\":"
      "{\"code\":{\"type\":\"string\"},"
      "\"language\":{\"type\":\"string\",\"enum\":[\"python\",\"bash\",\"sh\"]},"
      "\"timeout\":{\"type\":\"integer\"},"
      "\"sigma_product\":{\"type\":\"number\"}},"
      "\"required\":[\"code\"]}"
    },
    { "cos_sigma_profile",
      "Return the most recent 8-channel σ-profile, thresholds τ, and "
      "abstention statistics.  Read-only; no model invocation.",
      "{\"type\":\"object\",\"properties\":{}}"
    },
};
static const int kToolsN = (int)(sizeof kTools / sizeof kTools[0]);

typedef struct {
    const char *uri;
    const char *name;
    const char *description;
    const char *mime_type;
} resource_desc_t;

static const resource_desc_t kResources[] = {
    { "cos://memory/{session_id}", "session memory",
      "Chat history and episodic memory for a given session_id.",
      "application/json" },
    { "cos://knowledge/{doc_id}",  "knowledge entry",
      "A single ingested knowledge-base chunk with metadata.",
      "application/json" },
    { "cos://sigma/history",        "σ history",
      "Recent σ-profiles from Creation OS generations, most recent first.",
      "application/json" },
};
static const int kResourcesN = (int)(sizeof kResources / sizeof kResources[0]);

typedef struct {
    const char *name;
    const char *description;
    const char *template;
} prompt_desc_t;

static const prompt_desc_t kPrompts[] = {
    { "cos_analyze",
      "Ask Creation OS to analyse content with σ-governance (explicit "
      "abstention when uncertain).",
      "Analyze the following with σ-governance.  Cite uncertainty "
      "explicitly and abstain on any claim where σ_product > τ.\n\n"
      "---\n{{content}}\n---\n"
    },
    { "cos_verify",
      "Verify a claim.  Creation OS returns PASS, FAIL, or ABSTAIN "
      "with a σ-profile attached.",
      "Verify the following claim.  Return one of:\n"
      "  PASS     — σ_product < τ AND claim holds\n"
      "  FAIL     — σ_product < τ AND claim is wrong\n"
      "  ABSTAIN  — σ_product >= τ (explain which channel fired)\n\n"
      "Claim: {{claim}}\n"
    },
};
static const int kPromptsN = (int)(sizeof kPrompts / sizeof kPrompts[0]);

/* ------------------------------------------------------------------ */
/* Method handlers                                                     */
/* ------------------------------------------------------------------ */

static int handle_initialize(const cos_v116_config_t *cfg,
                             const mcp_req_t *r,
                             char *out, size_t cap) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o,
        "{\"protocolVersion\":\"2025-06-18\","
        "\"capabilities\":{"
          "\"tools\":{\"listChanged\":false},"
          "\"resources\":{\"listChanged\":false,\"subscribe\":false},"
          "\"prompts\":{\"listChanged\":false},"
          "\"experimental\":{\"sigmaGovernance\":{\"enabled\":true,"
            "\"channels\":[\"entropy\",\"sigma_mean\",\"sigma_max_token\","
            "\"sigma_product\",\"sigma_tail_mass\",\"sigma_n_effective\","
            "\"margin\",\"stability\"]}}"
        "},"
        "\"serverInfo\":{\"name\":") < 0) return -1;
    if (append_esc(out, cap, &o, cfg->server_name ? cfg->server_name : "creation-os") < 0) return -1;
    if (appendf(out, cap, &o, ",\"version\":") < 0) return -1;
    if (append_esc(out, cap, &o, cfg->server_version ? cfg->server_version : "0.116.0") < 0) return -1;
    if (appendf(out, cap, &o, "},\"instructions\":") < 0) return -1;
    if (append_esc(out, cap, &o,
        "Creation OS is a σ-governed AGI kernel.  Every tool response "
        "carries an 8-channel σ-profile; the model should treat high "
        "σ_product as a mandatory abstain signal rather than a hint.")
        < 0) return -1;
    if (appendf(out, cap, &o, "}}") < 0) return -1;
    return (int)o;
}

static int handle_tools_list(const mcp_req_t *r, char *out, size_t cap) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"tools\":[") < 0) return -1;
    for (int i = 0; i < kToolsN; ++i) {
        if (i && appendf(out, cap, &o, ",") < 0) return -1;
        if (appendf(out, cap, &o, "{\"name\":") < 0) return -1;
        if (append_esc(out, cap, &o, kTools[i].name) < 0) return -1;
        if (appendf(out, cap, &o, ",\"description\":") < 0) return -1;
        if (append_esc(out, cap, &o, kTools[i].description) < 0) return -1;
        if (appendf(out, cap, &o, ",\"inputSchema\":%s}", kTools[i].input_schema_json) < 0) return -1;
    }
    if (appendf(out, cap, &o, "]}}") < 0) return -1;
    return (int)o;
}

static int handle_resources_list(const mcp_req_t *r, char *out, size_t cap) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"resources\":[") < 0) return -1;
    for (int i = 0; i < kResourcesN; ++i) {
        if (i && appendf(out, cap, &o, ",") < 0) return -1;
        if (appendf(out, cap, &o, "{\"uri\":") < 0) return -1;
        if (append_esc(out, cap, &o, kResources[i].uri) < 0) return -1;
        if (appendf(out, cap, &o, ",\"name\":") < 0) return -1;
        if (append_esc(out, cap, &o, kResources[i].name) < 0) return -1;
        if (appendf(out, cap, &o, ",\"description\":") < 0) return -1;
        if (append_esc(out, cap, &o, kResources[i].description) < 0) return -1;
        if (appendf(out, cap, &o, ",\"mimeType\":") < 0) return -1;
        if (append_esc(out, cap, &o, kResources[i].mime_type) < 0) return -1;
        if (appendf(out, cap, &o, "}") < 0) return -1;
    }
    if (appendf(out, cap, &o, "]}}") < 0) return -1;
    return (int)o;
}

static int handle_prompts_list(const mcp_req_t *r, char *out, size_t cap) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"prompts\":[") < 0) return -1;
    for (int i = 0; i < kPromptsN; ++i) {
        if (i && appendf(out, cap, &o, ",") < 0) return -1;
        if (appendf(out, cap, &o, "{\"name\":") < 0) return -1;
        if (append_esc(out, cap, &o, kPrompts[i].name) < 0) return -1;
        if (appendf(out, cap, &o, ",\"description\":") < 0) return -1;
        if (append_esc(out, cap, &o, kPrompts[i].description) < 0) return -1;
        /* Arguments: inferred from the template {{…}} holes. */
        const char *argname = (strcmp(kPrompts[i].name, "cos_verify") == 0)
                            ? "claim" : "content";
        if (appendf(out, cap, &o, ",\"arguments\":[{\"name\":") < 0) return -1;
        if (append_esc(out, cap, &o, argname) < 0) return -1;
        if (appendf(out, cap, &o, ",\"required\":true}]}") < 0) return -1;
    }
    if (appendf(out, cap, &o, "]}}") < 0) return -1;
    return (int)o;
}

static int handle_prompts_get(const mcp_req_t *r, char *out, size_t cap) {
    /* Expect params: {"name":"cos_analyze","arguments":{"content":"…"}} */
    const char *p = r->params;
    const char *end = r->params_end;
    char name[64] = {0};
    if (!p) return write_error(out, cap, r->id_raw, -32602, "missing params", NULL);
    const char *nv = find_key(p, end, "name");
    if (!nv || !read_str(nv, end, name, sizeof name))
        return write_error(out, cap, r->id_raw, -32602, "missing name", NULL);
    const prompt_desc_t *pd = NULL;
    for (int i = 0; i < kPromptsN; ++i)
        if (strcmp(kPrompts[i].name, name) == 0) { pd = &kPrompts[i]; break; }
    if (!pd)
        return write_error(out, cap, r->id_raw, -32601, "unknown prompt", NULL);

    /* Optional single argument extraction. */
    char arg[4096] = {0};
    const char *argkey = (strcmp(pd->name, "cos_verify") == 0) ? "claim" : "content";
    const char *av = find_key(p, end, "arguments");
    if (av && *av == '{') {
        const char *aend = end;
        const char *v = find_key(av, aend, argkey);
        if (v) read_str(v, aend, arg, sizeof arg);
    }

    /* Fill {{content}} / {{claim}} one-shot. */
    char rendered[8192];
    size_t ro = 0;
    const char *tpl = pd->template;
    const char *hole = strstr(tpl, "{{");
    while (tpl && *tpl && ro + 2 < sizeof rendered) {
        if (hole && tpl == hole) {
            const char *close = strstr(tpl, "}}");
            if (!close) break;
            size_t an = strlen(arg);
            if (ro + an >= sizeof rendered) an = sizeof rendered - ro - 1;
            memcpy(rendered + ro, arg, an);
            ro += an;
            tpl = close + 2;
            hole = strstr(tpl, "{{");
        } else {
            rendered[ro++] = *tpl++;
        }
    }
    rendered[ro < sizeof rendered ? ro : sizeof rendered - 1] = '\0';

    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"description\":") < 0) return -1;
    if (append_esc(out, cap, &o, pd->description) < 0) return -1;
    if (appendf(out, cap, &o, ",\"messages\":[{\"role\":\"user\","
                              "\"content\":{\"type\":\"text\",\"text\":")
        < 0) return -1;
    if (append_esc(out, cap, &o, rendered) < 0) return -1;
    if (appendf(out, cap, &o, "}}]}}") < 0) return -1;
    return (int)o;
}

/* Shared "unavailable" error for tools that need backends not wired in
 * the current process (e.g. no GGUF bridge in stdio self-test). */
static int tool_unavailable(const char *id_raw, const char *tool,
                            char *out, size_t cap, const char *reason) {
    char data[256];
    snprintf(data, sizeof data,
             "{\"tool\":\"%s\",\"status\":\"unavailable\",\"reason\":\"%s\"}",
             tool, reason ? reason : "backend not attached");
    return write_error(out, cap, id_raw, -32000, "tool unavailable", data);
}

static int write_tool_text(char *out, size_t cap, const char *id_raw,
                           const char *text, const char *sigma_json) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"content\":[{\"type\":\"text\",\"text\":") < 0) return -1;
    if (append_esc(out, cap, &o, text) < 0) return -1;
    if (appendf(out, cap, &o, "}],\"isError\":false") < 0) return -1;
    if (sigma_json && *sigma_json) {
        if (appendf(out, cap, &o, ",\"sigma\":%s", sigma_json) < 0) return -1;
    }
    if (appendf(out, cap, &o, "}}") < 0) return -1;
    return (int)o;
}

static int handle_tools_call(const cos_v116_config_t *cfg,
                             const mcp_req_t *r, char *out, size_t cap) {
    const char *p = r->params;
    const char *end = r->params_end;
    if (!p) return write_error(out, cap, r->id_raw, -32602, "missing params", NULL);
    char name[64] = {0};
    const char *nv = find_key(p, end, "name");
    if (!nv || !read_str(nv, end, name, sizeof name))
        return write_error(out, cap, r->id_raw, -32602, "missing tool name", NULL);

    /* Dispatch */
    if (strcmp(name, "cos_memory_search") == 0) {
        if (!cfg->memory_store)
            return tool_unavailable(r->id_raw, name, out, cap,
                                    "memory_store not attached (v115 disabled)");
        char q[1024] = {0};
        int  top_k = 5;
        const char *av = find_key(p, end, "arguments");
        if (av && *av == '{') {
            const char *v = find_key(av, end, "query");
            if (v) read_str(v, end, q, sizeof q);
            const char *tv = find_key(av, end, "top_k");
            if (tv) { top_k = atoi(tv); if (top_k <= 0) top_k = 5; }
        }
        if (!q[0])
            return write_error(out, cap, r->id_raw, -32602, "missing query", NULL);
        cos_v115_memory_row_t rows[COS_V115_TOPK_MAX];
        int n = cos_v115_search_episodic(cfg->memory_store, q,
                                          top_k > COS_V115_TOPK_MAX ? COS_V115_TOPK_MAX : top_k,
                                          rows, COS_V115_TOPK_MAX);
        if (n < 0)
            return tool_unavailable(r->id_raw, name, out, cap, "search failed");
        char json[16384];
        cos_v115_search_results_to_json(rows, n, 1.0f, top_k, json, sizeof json);
        size_t o = 0;
        if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
        if (appendf(out, cap, &o,
                    "{\"content\":[{\"type\":\"text\",\"text\":") < 0) return -1;
        if (append_esc(out, cap, &o, json) < 0) return -1;
        if (appendf(out, cap, &o,
                    "}],\"structuredContent\":%s,\"isError\":false}}", json) < 0)
            return -1;
        return (int)o;
    }

    if (strcmp(name, "cos_sigma_profile") == 0) {
        /* Always available: returns the canonical 8-channel schema
         * even when no bridge is attached (zeros). */
        const char *sigma_json =
            "{\"channels\":{\"entropy\":0.0,\"sigma_mean\":0.0,"
            "\"sigma_max_token\":0.0,\"sigma_product\":0.0,"
            "\"sigma_tail_mass\":0.0,\"sigma_n_effective\":0.0,"
            "\"margin\":0.0,\"stability\":0.0},"
            "\"tau\":{\"tool\":0.60,\"code\":0.50,\"reason\":0.55},"
            "\"backend\":\"stub\"}";
        return write_tool_text(out, cap, r->id_raw,
                               "σ-profile snapshot (stub mode — no bridge "
                               "attached).  Channels are zeroed; thresholds "
                               "are canonical Creation OS defaults.",
                               sigma_json);
    }

    if (strcmp(name, "cos_chat") == 0 ||
        strcmp(name, "cos_reason") == 0) {
        /* Without the v101 bridge we cannot produce real output. */
        if (!cfg->bridge)
            return tool_unavailable(r->id_raw, name, out, cap,
                                    "no GGUF bridge attached — run 'cos serve' "
                                    "with --gguf to enable");
        /* Stub "attached" path: return a deterministic note so hosts
         * can wire test harnesses.  Real dispatch is wired by the HTTP
         * layer which already owns the bridge handle. */
        return write_tool_text(out, cap, r->id_raw,
                               "[creation-os] bridge attached; use HTTP layer "
                               "for real σ-gated completion",
                               NULL);
    }

    if (strcmp(name, "cos_sandbox_execute") == 0) {
        /* Delegated to the HTTP layer at /v1/sandbox/execute in the
         * full server; stdio self-test leaves it as advertised-but-
         * not-wired to keep v116 standalone minimal. */
        return tool_unavailable(r->id_raw, name, out, cap,
                                "use POST /v1/sandbox/execute on the HTTP layer");
    }

    return write_error(out, cap, r->id_raw, -32601, "unknown tool", NULL);
}

static int handle_resources_read(const mcp_req_t *r, char *out, size_t cap) {
    const char *p = r->params;
    const char *end = r->params_end;
    if (!p) return write_error(out, cap, r->id_raw, -32602, "missing params", NULL);
    char uri[256] = {0};
    const char *v = find_key(p, end, "uri");
    if (!v || !read_str(v, end, uri, sizeof uri))
        return write_error(out, cap, r->id_raw, -32602, "missing uri", NULL);

    /* Match by prefix. */
    const char *text = NULL;
    if (!strncmp(uri, "cos://sigma/history",       19)) {
        text = "{\"history\":[],\"note\":\"sigma/history is maintained by the v106 server; "
               "this stdio endpoint only exposes the schema.\"}";
    } else if (!strncmp(uri, "cos://memory/",       13)) {
        text = "{\"session\":\"requested\",\"entries\":[],\"note\":\"attach memory_store to hydrate.\"}";
    } else if (!strncmp(uri, "cos://knowledge/",    16)) {
        text = "{\"doc\":\"requested\",\"chunks\":[],\"note\":\"attach memory_store to hydrate.\"}";
    }
    if (!text)
        return write_error(out, cap, r->id_raw, -32602,
                           "unknown resource uri", NULL);

    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{\"contents\":[{\"uri\":") < 0) return -1;
    if (append_esc(out, cap, &o, uri) < 0) return -1;
    if (appendf(out, cap, &o,
                ",\"mimeType\":\"application/json\",\"text\":") < 0) return -1;
    if (append_esc(out, cap, &o, text) < 0) return -1;
    if (appendf(out, cap, &o, "}]}}") < 0) return -1;
    return (int)o;
}

/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

static int write_result_prefix_and_close(char *out, size_t cap, const mcp_req_t *r) {
    size_t o = 0;
    if (write_result_prefix(out, cap, &o, r->id_raw) < 0) return -1;
    if (appendf(out, cap, &o, "{}}") < 0) return -1;
    return (int)o;
}

int cos_v116_dispatch(const cos_v116_config_t *cfg_in,
                      const char *req, size_t len,
                      char *out, size_t cap) {
    cos_v116_config_t cfg;
    if (cfg_in) cfg = *cfg_in; else cos_v116_config_defaults(&cfg);
    if (!out || cap == 0) return -1;
    out[0] = '\0';

    mcp_req_t r;
    if (!parse_request(req, len, &r))
        return write_error(out, cap, "null", -32700, "parse error", NULL);

    /* Notifications (no id) → suppress response. */
    if (!r.has_id) {
        /* Known notifications we silently accept. */
        if (!strcmp(r.method, "notifications/initialized") ||
            !strcmp(r.method, "initialized") ||
            !strcmp(r.method, "notifications/cancelled")) {
            out[0] = '\0';
            return 0;
        }
        out[0] = '\0';
        return 0;
    }

    if (!strcmp(r.method, "initialize"))        return handle_initialize(&cfg, &r, out, cap);
    if (!strcmp(r.method, "ping"))              return write_result_prefix_and_close(out, cap, &r);
    if (!strcmp(r.method, "shutdown"))          return write_result_prefix_and_close(out, cap, &r);
    if (!strcmp(r.method, "tools/list"))        return handle_tools_list(&r, out, cap);
    if (!strcmp(r.method, "tools/call"))        return handle_tools_call(&cfg, &r, out, cap);
    if (!strcmp(r.method, "resources/list"))    return handle_resources_list(&r, out, cap);
    if (!strcmp(r.method, "resources/read"))    return handle_resources_read(&r, out, cap);
    if (!strcmp(r.method, "prompts/list"))      return handle_prompts_list(&r, out, cap);
    if (!strcmp(r.method, "prompts/get"))       return handle_prompts_get(&r, out, cap);

    return write_error(out, cap, r.id_raw, -32601, "method not found", NULL);
}

/* ------------------------------------------------------------------ */
/* stdio loop                                                          */
/* ------------------------------------------------------------------ */

int cos_v116_run_stdio(const cos_v116_config_t *cfg, FILE *in, FILE *out) {
    if (!in || !out) return -1;
    char buf[65536];
    char resp[131072];
    while (fgets(buf, sizeof buf, in)) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
            buf[--len] = '\0';
        if (len == 0) continue;
        int n = cos_v116_dispatch(cfg, buf, len, resp, sizeof resp);
        if (n < 0) continue;
        if (n == 0) continue;     /* notification — no reply */
        fputs(resp, out);
        fputc('\n', out);
        fflush(out);
        if (strstr(buf, "\"shutdown\"")) break;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Self-test                                                           */
/* ------------------------------------------------------------------ */

#define _CHECK(cond, msg) do { if (!(cond)) { \
    fprintf(stderr, "v116 self-test FAIL: %s\n", msg); return 1; } } while (0)

int cos_v116_self_test(void) {
    cos_v116_config_t cfg;
    cos_v116_config_defaults(&cfg);

    char buf[131072];
    int n;

    /* 1) initialize: must carry protocolVersion + serverInfo. */
    const char *init = "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\","
                       "\"params\":{\"protocolVersion\":\"2025-06-18\"}}";
    n = cos_v116_dispatch(&cfg, init, strlen(init), buf, sizeof buf);
    _CHECK(n > 0, "initialize response");
    _CHECK(strstr(buf, "\"protocolVersion\":\"2025-06-18\""),
           "initialize carries protocolVersion");
    _CHECK(strstr(buf, "creation-os"), "serverInfo name present");
    _CHECK(strstr(buf, "sigmaGovernance"), "sigma capability advertised");

    /* 2) tools/list has 5 tools. */
    const char *tl = "{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}";
    n = cos_v116_dispatch(&cfg, tl, strlen(tl), buf, sizeof buf);
    _CHECK(n > 0, "tools/list response");
    for (int i = 0; i < kToolsN; ++i)
        _CHECK(strstr(buf, kTools[i].name) != NULL, "tool name present");

    /* 3) resources/list has 3 resources. */
    const char *rl = "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"resources/list\"}";
    n = cos_v116_dispatch(&cfg, rl, strlen(rl), buf, sizeof buf);
    _CHECK(n > 0, "resources/list response");
    _CHECK(strstr(buf, "cos://memory/") != NULL, "memory resource");
    _CHECK(strstr(buf, "cos://knowledge/") != NULL, "knowledge resource");
    _CHECK(strstr(buf, "cos://sigma/history") != NULL, "sigma history resource");

    /* 4) prompts/list has 2 prompts. */
    const char *pl = "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"prompts/list\"}";
    n = cos_v116_dispatch(&cfg, pl, strlen(pl), buf, sizeof buf);
    _CHECK(n > 0, "prompts/list response");
    _CHECK(strstr(buf, "cos_analyze") != NULL, "cos_analyze prompt");
    _CHECK(strstr(buf, "cos_verify")  != NULL, "cos_verify prompt");

    /* 5) prompts/get renders template. */
    const char *pg = "{\"jsonrpc\":\"2.0\",\"id\":5,\"method\":\"prompts/get\","
                     "\"params\":{\"name\":\"cos_analyze\","
                     "\"arguments\":{\"content\":\"hello world\"}}}";
    n = cos_v116_dispatch(&cfg, pg, strlen(pg), buf, sizeof buf);
    _CHECK(n > 0, "prompts/get response");
    _CHECK(strstr(buf, "hello world") != NULL, "prompt fills {{content}}");
    _CHECK(strstr(buf, "σ-governance") != NULL ||
           strstr(buf, "\\u03c3-governance") != NULL,
           "prompt carries σ language");

    /* 6) tools/call for sigma_profile always succeeds. */
    const char *tc1 = "{\"jsonrpc\":\"2.0\",\"id\":6,\"method\":\"tools/call\","
                      "\"params\":{\"name\":\"cos_sigma_profile\",\"arguments\":{}}}";
    n = cos_v116_dispatch(&cfg, tc1, strlen(tc1), buf, sizeof buf);
    _CHECK(n > 0, "sigma_profile call");
    _CHECK(strstr(buf, "\"channels\"") != NULL, "sigma_profile has channels");
    _CHECK(strstr(buf, "sigma_product") != NULL, "sigma_product channel present");

    /* 7) tools/call cos_memory_search without a store returns structured error. */
    const char *tc2 = "{\"jsonrpc\":\"2.0\",\"id\":7,\"method\":\"tools/call\","
                      "\"params\":{\"name\":\"cos_memory_search\","
                      "\"arguments\":{\"query\":\"paris\"}}}";
    n = cos_v116_dispatch(&cfg, tc2, strlen(tc2), buf, sizeof buf);
    _CHECK(n > 0, "memory_search unavailable response");
    _CHECK(strstr(buf, "\"error\"") != NULL, "error surfaced");
    _CHECK(strstr(buf, "unavailable") != NULL, "unavailable marker");

    /* 8) Unknown method → -32601. */
    const char *um = "{\"jsonrpc\":\"2.0\",\"id\":8,\"method\":\"does/not/exist\"}";
    n = cos_v116_dispatch(&cfg, um, strlen(um), buf, sizeof buf);
    _CHECK(n > 0, "unknown method response");
    _CHECK(strstr(buf, "-32601") != NULL, "method-not-found error");

    /* 9) Notifications silently accepted. */
    const char *no = "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}";
    n = cos_v116_dispatch(&cfg, no, strlen(no), buf, sizeof buf);
    _CHECK(n == 0 && buf[0] == '\0', "notification suppressed");

    /* 10) Parse error on garbage input. */
    const char *bad = "not json at all";
    n = cos_v116_dispatch(&cfg, bad, strlen(bad), buf, sizeof buf);
    _CHECK(n > 0, "parse error response");
    _CHECK(strstr(buf, "-32700") != NULL, "parse error code");

    return 0;
}
