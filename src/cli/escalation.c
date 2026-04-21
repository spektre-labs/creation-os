/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *
 *  src/cli/escalation.c — see escalation.h.
 *
 *  Transport:    libcurl easy (SSL via system stack)
 *  Dispatch:     CREATION_OS_ESCALATION_PROVIDER ∈ {claude, openai,
 *                                                   gpt, deepseek}
 *  Logging:      ~/.cos/distill_pairs.jsonl (one row per call)
 *
 *  This module intentionally keeps JSON handling minimal: we build
 *  the request with snprintf + json_encode_string, and extract the
 *  response via a small key scanner (same approach as bitnet_server
 *  so we avoid pulling in a JSON dep for the whole tree).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef _DARWIN_C_SOURCE
#define _DARWIN_C_SOURCE
#endif

#include "escalation.h"
#include "stub_gen.h"

#include <errno.h>
#include <math.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>

/* g_cos_escalation_ctx + cos_cli_escalation_record_student live in
 * stub_gen.c so that binaries which only link stub_gen (cos-benchmark,
 * cos-cost, etc.) don't need the libcurl-aware escalation.c object. */

/* --- path helpers ------------------------------------------------- */

static const char *resolve_home(void) {
    const char *h = getenv("HOME");
    if (h != NULL && h[0] != '\0') return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw != NULL && pw->pw_dir != NULL) ? pw->pw_dir : ".";
}

static void ensure_cos_dir(void) {
    char p[1024];
    snprintf(p, sizeof p, "%s/.cos", resolve_home());
    mkdir(p, 0700);  /* ignore EEXIST */
}

const char *cos_cli_escalation_distill_path(void) {
    static char buf[1024];
    const char *override = getenv("CREATION_OS_DISTILL_LOG");
    if (override != NULL && override[0] != '\0') {
        snprintf(buf, sizeof buf, "%s", override);
        return buf;
    }
    snprintf(buf, sizeof buf, "%s/.cos/distill_pairs.jsonl",
             resolve_home());
    return buf;
}

/* --- provider resolution ----------------------------------------- */

typedef enum {
    PROV_NONE = 0,
    PROV_CLAUDE,
    PROV_OPENAI,
    PROV_DEEPSEEK,
} provider_t;

static provider_t resolve_provider(const char **name_out,
                                   const char **key_out,
                                   const char **model_out) {
    const char *prov = getenv("CREATION_OS_ESCALATION_PROVIDER");
    const char *model_override = getenv("CREATION_OS_ESCALATION_MODEL");
    if (prov == NULL || prov[0] == '\0') return PROV_NONE;

    if (strcmp(prov, "claude") == 0 || strcmp(prov, "anthropic") == 0) {
        const char *k = getenv("CREATION_OS_CLAUDE_API_KEY");
        if (k == NULL || k[0] == '\0') return PROV_NONE;
        *name_out  = "claude";
        *key_out   = k;
        *model_out = model_override ? model_override
                                    : "claude-3-5-haiku-20241022";
        return PROV_CLAUDE;
    }
    if (strcmp(prov, "openai") == 0 || strcmp(prov, "gpt") == 0) {
        const char *k = getenv("CREATION_OS_OPENAI_API_KEY");
        if (k == NULL || k[0] == '\0') return PROV_NONE;
        *name_out  = "openai";
        *key_out   = k;
        *model_out = model_override ? model_override : "gpt-4o-mini";
        return PROV_OPENAI;
    }
    if (strcmp(prov, "deepseek") == 0) {
        const char *k = getenv("CREATION_OS_DEEPSEEK_API_KEY");
        if (k == NULL || k[0] == '\0') return PROV_NONE;
        *name_out  = "deepseek";
        *key_out   = k;
        *model_out = model_override ? model_override : "deepseek-chat";
        return PROV_DEEPSEEK;
    }
    return PROV_NONE;
}

int cos_cli_escalation_diag(char *provider_out, size_t prov_cap,
                            char *model_out,    size_t model_cap) {
    const char *pn = "stub";
    const char *pk = NULL;
    const char *pm = "none";
    provider_t p = resolve_provider(&pn, &pk, &pm);
    if (provider_out && prov_cap > 0)
        snprintf(provider_out, prov_cap, "%s", (p == PROV_NONE ? "stub" : pn));
    if (model_out && model_cap > 0)
        snprintf(model_out, model_cap, "%s", (p == PROV_NONE ? "none" : pm));
    return (p == PROV_NONE) ? 0 : 1;
}

/* --- minimal JSON string encoder (copy of bitnet_server's approach) --- */

static size_t je_append(char *dst, size_t cap, size_t w, const char *s, size_t n) {
    if (w + n >= cap) return 0;
    memcpy(dst + w, s, n);
    return w + n;
}

static size_t json_encode_str(char *dst, size_t cap, const char *s) {
    if (dst == NULL || s == NULL || cap < 3) return 0;
    size_t w = 0;
    dst[w++] = '"';
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        char buf[8];
        int n = 0;
        switch (*p) {
            case '"':  buf[0]='\\'; buf[1]='"';  n=2; break;
            case '\\': buf[0]='\\'; buf[1]='\\'; n=2; break;
            case '\n': buf[0]='\\'; buf[1]='n';  n=2; break;
            case '\r': buf[0]='\\'; buf[1]='r';  n=2; break;
            case '\t': buf[0]='\\'; buf[1]='t';  n=2; break;
            case '\b': buf[0]='\\'; buf[1]='b';  n=2; break;
            case '\f': buf[0]='\\'; buf[1]='f';  n=2; break;
            default:
                if (*p < 0x20) {
                    n = snprintf(buf, sizeof buf, "\\u%04x", *p);
                } else {
                    buf[0] = (char)*p; n = 1;
                }
        }
        w = je_append(dst, cap, w, buf, (size_t)n);
        if (w == 0) return 0;
    }
    if (w + 1 >= cap) return 0;
    dst[w++] = '"';
    return w;
}

/* --- response scanner (locate a JSON key + return pointer to val) --- */

static const char *js_find_key(const char *hay, size_t hlen,
                               const char *key) {
    size_t klen = strlen(key);
    for (size_t i = 0; i + klen + 2 < hlen; i++) {
        if (hay[i] != '"') continue;
        if (strncmp(hay + i + 1, key, klen) != 0) continue;
        if (hay[i + 1 + klen] != '"') continue;
        const char *q = hay + i + 1 + klen + 1;
        while (q < hay + hlen && (*q == ' ' || *q == '\t')) q++;
        if (q >= hay + hlen || *q != ':') continue;
        q++;
        while (q < hay + hlen && (*q == ' ' || *q == '\t')) q++;
        if (q >= hay + hlen) return NULL;
        return q;
    }
    return NULL;
}

/* Extract a JSON string value starting at *p (pointing at opening
 * quote).  Writes into `out` (cap `cap`).  Returns new *p (past
 * closing quote) or NULL on error. */
static const char *js_read_string(const char *p, const char *end,
                                  char *out, size_t cap) {
    if (p >= end || *p != '"') return NULL;
    p++;
    size_t w = 0;
    while (p < end && *p != '"') {
        char c;
        if (*p == '\\' && p + 1 < end) {
            char esc = p[1];
            p += 2;
            switch (esc) {
                case 'n': c = '\n'; break;
                case 't': c = '\t'; break;
                case 'r': c = '\r'; break;
                case '"': c = '"';  break;
                case '\\':c = '\\'; break;
                case '/': c = '/';  break;
                case 'u': {
                    if (p + 4 > end) return NULL;
                    unsigned int cp = 0;
                    for (int i = 0; i < 4; i++) {
                        char h = p[i];
                        cp <<= 4;
                        if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                        else return NULL;
                    }
                    p += 4;
                    if (cp < 0x80u) {
                        if (w + 1 >= cap) return NULL;
                        out[w++] = (char)cp;
                    } else if (cp < 0x800u) {
                        if (w + 2 >= cap) return NULL;
                        out[w++] = (char)(0xC0 | (cp >> 6));
                        out[w++] = (char)(0x80 | (cp & 0x3F));
                    } else {
                        if (w + 3 >= cap) return NULL;
                        out[w++] = (char)(0xE0 | (cp >> 12));
                        out[w++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                        out[w++] = (char)(0x80 | (cp & 0x3F));
                    }
                    continue;
                }
                default:  c = esc; break;
            }
        } else {
            c = *p++;
        }
        if (w + 1 >= cap) return NULL;
        out[w++] = c;
    }
    if (p >= end) return NULL;
    out[w] = '\0';
    return p + 1;
}

/* --- libcurl write buffer ---------------------------------------- */

typedef struct {
    char  *data;
    size_t len, cap;
    uint64_t bytes_recv;
} resp_buf_t;

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *ud) {
    size_t n = size * nmemb;
    resp_buf_t *r = (resp_buf_t *)ud;
    if (r->len + n + 1 > r->cap) {
        size_t new_cap = (r->cap == 0) ? 8192 : r->cap * 2;
        while (new_cap < r->len + n + 1) new_cap *= 2;
        char *nd = realloc(r->data, new_cap);
        if (nd == NULL) return 0;
        r->data = nd;
        r->cap  = new_cap;
    }
    memcpy(r->data + r->len, ptr, n);
    r->len += n;
    r->data[r->len] = '\0';
    r->bytes_recv += n;
    return n;
}

/* --- provider: Claude -------------------------------------------- */

static int call_claude(const char *prompt, const char *key, const char *model,
                       int max_tokens,
                       char *out_text, size_t text_cap,
                       float *out_sigma,
                       uint64_t *bytes_sent, uint64_t *bytes_recv) {
    /* Build body */
    static char body[32768];
    size_t bw = 0;
    int n;
    n = snprintf(body + bw, sizeof body - bw,
                 "{\"model\":\"%s\",\"max_tokens\":%d,\"messages\":[{\"role\":\"user\",\"content\":",
                 model, max_tokens);
    if (n <= 0) return -1; bw += (size_t)n;
    size_t sw = json_encode_str(body + bw, sizeof body - bw, prompt);
    if (sw == 0) return -1; bw += sw;
    n = snprintf(body + bw, sizeof body - bw, "}]}");
    if (n <= 0) return -1; bw += (size_t)n;

    resp_buf_t resp = {0};
    CURL *c = curl_easy_init();
    if (c == NULL) return -1;

    struct curl_slist *hdr = NULL;
    char auth[512];
    snprintf(auth, sizeof auth, "x-api-key: %s", key);
    hdr = curl_slist_append(hdr, auth);
    hdr = curl_slist_append(hdr, "anthropic-version: 2023-06-01");
    hdr = curl_slist_append(hdr, "content-type: application/json");

    curl_easy_setopt(c, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bw);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);

    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);

    *bytes_sent = (uint64_t)bw;
    *bytes_recv = resp.bytes_recv;

    int result = -1;
    if (rc == CURLE_OK && status >= 200 && status < 300 && resp.data != NULL) {
        /* Claude response:
         *   {"content":[{"type":"text","text":"..."}],"usage":{...}} */
        const char *cp = js_find_key(resp.data, resp.len, "content");
        if (cp != NULL && *cp == '[') {
            /* find first {"type":"text","text":"..."} */
            const char *tp = js_find_key(cp, resp.len - (cp - resp.data), "text");
            /* skip "type" keys — we want the one whose value is a string */
            while (tp != NULL && *tp != '"') {
                /* not a string value; advance and retry past this key */
                const char *base = tp + 1;
                tp = js_find_key(base, resp.len - (base - resp.data), "text");
            }
            if (tp != NULL) {
                if (js_read_string(tp, resp.data + resp.len,
                                   out_text, text_cap) != NULL) {
                    result = 0;
                }
            }
        }
        /* Claude does not expose logprobs; use a confidence floor. */
        *out_sigma = 0.10f;
    } else {
        if (resp.data != NULL) {
            size_t n0 = resp.len < text_cap - 1 ? resp.len : text_cap - 1;
            memcpy(out_text, resp.data, n0);
            out_text[n0] = '\0';
        } else {
            snprintf(out_text, text_cap, "claude HTTP %ld (%s)",
                     status, curl_easy_strerror(rc));
        }
        *out_sigma = 1.0f;
    }

    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    free(resp.data);
    return result;
}

/* --- provider: OpenAI / DeepSeek (shared OpenAI-compat shape) ----- */

static int call_openai_compat(const char *url, const char *prompt,
                              const char *key, const char *model,
                              int max_tokens, int enable_logprobs,
                              char *out_text, size_t text_cap,
                              float *out_sigma,
                              uint64_t *bytes_sent, uint64_t *bytes_recv) {
    static char body[32768];
    size_t bw = 0;
    int n;
    n = snprintf(body + bw, sizeof body - bw,
                 "{\"model\":\"%s\",\"max_tokens\":%d,\"messages\":[{\"role\":\"user\",\"content\":",
                 model, max_tokens);
    if (n <= 0) return -1; bw += (size_t)n;
    size_t sw = json_encode_str(body + bw, sizeof body - bw, prompt);
    if (sw == 0) return -1; bw += sw;
    if (enable_logprobs) {
        n = snprintf(body + bw, sizeof body - bw,
                     "}],\"logprobs\":true,\"top_logprobs\":5}");
    } else {
        n = snprintf(body + bw, sizeof body - bw, "}]}");
    }
    if (n <= 0) return -1; bw += (size_t)n;

    resp_buf_t resp = {0};
    CURL *c = curl_easy_init();
    if (c == NULL) return -1;

    struct curl_slist *hdr = NULL;
    char auth[512];
    snprintf(auth, sizeof auth, "Authorization: Bearer %s", key);
    hdr = curl_slist_append(hdr, auth);
    hdr = curl_slist_append(hdr, "Content-Type: application/json");

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)bw);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 60L);

    CURLcode rc = curl_easy_perform(c);
    long status = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &status);

    *bytes_sent = (uint64_t)bw;
    *bytes_recv = resp.bytes_recv;

    int result = -1;
    *out_sigma = 1.0f;

    if (rc == CURLE_OK && status >= 200 && status < 300 && resp.data != NULL) {
        /* OpenAI: choices[0].message.content */
        const char *mp = js_find_key(resp.data, resp.len, "message");
        const char *tp = NULL;
        if (mp != NULL) {
            tp = js_find_key(mp, resp.len - (mp - resp.data), "content");
        }
        if (tp == NULL) {
            tp = js_find_key(resp.data, resp.len, "content");
        }
        if (tp != NULL && *tp == '"') {
            if (js_read_string(tp, resp.data + resp.len,
                               out_text, text_cap) != NULL) {
                result = 0;
            }
        }
        /* σ from logprobs if present.  Structure:
         *   "logprobs":{"content":[{"token":"x","logprob":-0.1,
         *       "top_logprobs":[...]}, ...]}
         * σ_token = 1 - exp(logprob_chosen).  We take max over
         * content[] entries. */
        if (enable_logprobs) {
            const char *lp = js_find_key(resp.data, resp.len, "logprobs");
            if (lp != NULL) {
                const char *cp = js_find_key(lp, resp.len - (lp - resp.data),
                                             "content");
                if (cp != NULL && *cp == '[') {
                    float max_sigma = 0.0f;
                    const char *q = cp;
                    while (q < resp.data + resp.len) {
                        const char *lgp = js_find_key(q,
                                            resp.len - (q - resp.data),
                                            "logprob");
                        if (lgp == NULL) break;
                        double lg = strtod(lgp, NULL);
                        float pr = (float)exp(lg);
                        if (pr < 0.0f) pr = 0.0f;
                        if (pr > 1.0f) pr = 1.0f;
                        float s = 1.0f - pr;
                        if (s > max_sigma) max_sigma = s;
                        q = lgp + 1;
                    }
                    *out_sigma = (max_sigma > 0.0f) ? max_sigma : 0.05f;
                }
            }
        }
        if (*out_sigma >= 1.0f) *out_sigma = 0.10f;  /* fallback floor */
    } else {
        if (resp.data != NULL) {
            size_t n0 = resp.len < text_cap - 1 ? resp.len : text_cap - 1;
            memcpy(out_text, resp.data, n0);
            out_text[n0] = '\0';
        } else {
            snprintf(out_text, text_cap, "API HTTP %ld (%s)",
                     status, curl_easy_strerror(rc));
        }
    }

    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    free(resp.data);
    return result;
}

/* --- cost estimates (EUR) ---------------------------------------- */
/*
 * Rough USD→EUR ≈ 0.93.  Token counts are approximated from byte
 * counts (≈ 4 chars / token English text).  These are order-of-
 * magnitude estimates for the cost ledger; exact billing lives on
 * the provider dashboard.
 */

static double estimate_cost(provider_t p,
                            uint64_t bytes_sent, uint64_t bytes_recv) {
    double tok_in  = (double)bytes_sent / 4.0;
    double tok_out = (double)bytes_recv / 4.0;
    double in_eur_per_mtok  = 0.0;
    double out_eur_per_mtok = 0.0;
    switch (p) {
        case PROV_CLAUDE:   /* claude-3-5-haiku */
            in_eur_per_mtok  = 0.80 * 0.93;
            out_eur_per_mtok = 4.00 * 0.93;
            break;
        case PROV_OPENAI:   /* gpt-4o-mini */
            in_eur_per_mtok  = 0.15 * 0.93;
            out_eur_per_mtok = 0.60 * 0.93;
            break;
        case PROV_DEEPSEEK:
            in_eur_per_mtok  = 0.14 * 0.93;
            out_eur_per_mtok = 0.28 * 0.93;
            break;
        default: return 0.0;
    }
    return (tok_in  * in_eur_per_mtok  / 1.0e6)
         + (tok_out * out_eur_per_mtok / 1.0e6);
}

/* --- distill pair logger ----------------------------------------- */

static void append_distill_pair(const char *prompt,
                                const char *student, float student_sigma,
                                const char *teacher, float teacher_sigma,
                                const char *provider, const char *model,
                                double cost_eur,
                                uint64_t bytes_sent, uint64_t bytes_recv) {
    ensure_cos_dir();
    const char *path = cos_cli_escalation_distill_path();
    FILE *f = fopen(path, "a");
    if (f == NULL) return;

    /* Build the row with json_encode_str so quotes in student/teacher
     * text are correctly escaped. */
    static char enc[16384];
    size_t w = 0;
    int n;
    n = snprintf(enc + w, sizeof enc - w,
                 "{\"ts\":%lld,\"prompt\":", (long long)time(NULL));
    if (n > 0) w += (size_t)n;
    size_t sw;
    sw = json_encode_str(enc + w, sizeof enc - w, prompt ? prompt : "");
    w = (sw > 0) ? sw : w;  /* json_encode_str returns new w on success */

    n = snprintf(enc + w, sizeof enc - w, ",\"student\":");
    if (n > 0) w += (size_t)n;
    sw = json_encode_str(enc + w, sizeof enc - w, student ? student : "");
    w = (sw > 0) ? sw : w;

    n = snprintf(enc + w, sizeof enc - w,
                 ",\"student_sigma\":%.4f,\"teacher\":",
                 (double)student_sigma);
    if (n > 0) w += (size_t)n;
    sw = json_encode_str(enc + w, sizeof enc - w, teacher ? teacher : "");
    w = (sw > 0) ? sw : w;

    n = snprintf(enc + w, sizeof enc - w,
                 ",\"teacher_sigma\":%.4f,\"provider\":\"%s\","
                 "\"model\":\"%s\",\"cost_eur\":%.6f,"
                 "\"bytes_sent\":%llu,\"bytes_recv\":%llu}\n",
                 (double)teacher_sigma, provider, model, cost_eur,
                 (unsigned long long)bytes_sent,
                 (unsigned long long)bytes_recv);
    if (n > 0) w += (size_t)n;

    fwrite(enc, 1, w, f);
    fclose(f);
}

/* --- main dispatch ----------------------------------------------- */

int cos_cli_escalate_api(const char *prompt, void *ctx,
                         const char **out_text, float *out_sigma,
                         double *out_cost_eur,
                         uint64_t *out_bytes_sent,
                         uint64_t *out_bytes_recv) {
    const char *name  = NULL;
    const char *key   = NULL;
    const char *model = NULL;
    provider_t  prov  = resolve_provider(&name, &key, &model);

    static char teacher_buf[16384];
    teacher_buf[0] = '\0';

    if (prov == PROV_NONE) {
        /* No provider configured → fall through to the deterministic
         * stub so self-tests keep seeing the canonical "escalated"
         * shape without network I/O. */
        return cos_cli_stub_escalate(prompt, ctx, out_text, out_sigma,
                                     out_cost_eur,
                                     out_bytes_sent, out_bytes_recv);
    }

    int max_tokens = 1024;
    const char *mt_env = getenv("CREATION_OS_ESCALATION_MAX_TOKENS");
    if (mt_env != NULL) {
        int v = atoi(mt_env);
        if (v > 0) max_tokens = v;
    }

    uint64_t sent = 0, recv = 0;
    float teacher_sigma = 1.0f;
    int rc = -1;

    switch (prov) {
        case PROV_CLAUDE:
            rc = call_claude(prompt, key, model, max_tokens,
                             teacher_buf, sizeof teacher_buf,
                             &teacher_sigma, &sent, &recv);
            break;
        case PROV_OPENAI:
            rc = call_openai_compat(
                "https://api.openai.com/v1/chat/completions",
                prompt, key, model, max_tokens, /*logprobs*/1,
                teacher_buf, sizeof teacher_buf,
                &teacher_sigma, &sent, &recv);
            break;
        case PROV_DEEPSEEK:
            rc = call_openai_compat(
                "https://api.deepseek.com/chat/completions",
                prompt, key, model, max_tokens, /*logprobs*/0,
                teacher_buf, sizeof teacher_buf,
                &teacher_sigma, &sent, &recv);
            break;
        default: break;
    }

    double cost = estimate_cost(prov, sent, recv);

    if (rc != 0) {
        /* API call failed → fall back to stub so the session keeps
         * going (displayed output will be the error text captured in
         * teacher_buf). */
        *out_text       = teacher_buf[0] ? teacher_buf : "escalation failed";
        *out_sigma      = 1.0f;
        *out_cost_eur   = cost;
        *out_bytes_sent = sent;
        *out_bytes_recv = recv;
        return 0;  /* pipeline treats this as escalated=true regardless */
    }

    /* Log distill pair.  If there is no student context (no prior
     * generate() call in this session) we still log — the teacher-
     * only row is useful for provenance. */
    append_distill_pair(
        prompt,
        g_cos_escalation_ctx.student_valid
            ? g_cos_escalation_ctx.student_text : "",
        g_cos_escalation_ctx.student_valid
            ? g_cos_escalation_ctx.student_sigma : 1.0f,
        teacher_buf, teacher_sigma,
        name, model, cost, sent, recv);

    *out_text       = teacher_buf;
    *out_sigma      = teacher_sigma;
    *out_cost_eur   = cost;
    *out_bytes_sent = sent;
    *out_bytes_recv = recv;
    return 0;
}
