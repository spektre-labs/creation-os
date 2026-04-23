/*
 * Multimodal perception client: local llama-server (text), vision + audio HTTP.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include "perception.h"

#include "engram_episodic.h"
#include "error_attribution.h"
#include "inference_cache.h"
#include "knowledge_graph.h"

#include "bitnet_server.h"
#include "bitnet_sigma.h"

#include <curl/curl.h>

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

typedef struct {
    char   *buf;
    size_t len;
} curl_mem_t;

static size_t curl_append(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    curl_mem_t *m = (curl_mem_t *)userdata;
    size_t      n = size * nmemb;
    char       *p = realloc(m->buf, m->len + n + 1);
    if (!p)
        return 0;
    m->buf = p;
    memcpy(m->buf + m->len, ptr, n);
    m->len += n;
    m->buf[m->len] = '\0';
    return n;
}

static int64_t wall_ms(void)
{
#if defined(CLOCK_REALTIME)
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) == 0)
        return (int64_t)ts.tv_sec * 1000LL + (int64_t)ts.tv_nsec / 1000000LL;
#endif
    return (int64_t)time(NULL) * 1000LL;
}

static int read_file_all(const char *path, unsigned char **out, size_t *out_len)
{
    if (!path || !out || !out_len)
        return -1;
    *out     = NULL;
    *out_len = 0;
    FILE *fp = fopen(path, "rb");
    if (!fp)
        return -1;
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return -1;
    }
    long sz = ftell(fp);
    if (sz < 0 || sz > (long)(16 * 1024 * 1024)) {
        fclose(fp);
        return -1;
    }
    rewind(fp);
    unsigned char *b = (unsigned char *)malloc((size_t)sz + 1);
    if (!b) {
        fclose(fp);
        return -1;
    }
    if (fread(b, 1, (size_t)sz, fp) != (size_t)sz) {
        free(b);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    b[(size_t)sz] = '\0';
    *out     = b;
    *out_len = (size_t)sz;
    return 0;
}

static size_t b64_encode(const unsigned char *data, size_t len, char *out,
                         size_t cap)
{
    static const char *b64 =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = (uint32_t)data[i] << 16;
        if (i + 1 < len)
            v |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < len)
            v |= (uint32_t)data[i + 2];
        if (o + 4 >= cap)
            break;
        out[o++] = b64[(v >> 18) & 63];
        out[o++] = b64[(v >> 12) & 63];
        if (i + 1 < len)
            out[o++] = b64[(v >> 6) & 63];
        else
            out[o++] = '=';
        if (i + 2 < len)
            out[o++] = b64[v & 63];
        else
            out[o++] = '=';
    }
    if (o < cap)
        out[o] = '\0';
    return o;
}

static int json_esc(const char *s, char *out, size_t cap)
{
    size_t j = 0;
    if (!s)
        s = "";
    for (; *s && j + 2 < cap; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') {
            if (j + 3 >= cap)
                break;
            out[j++] = '\\';
            out[j++] = (char)c;
        } else if (c < 0x20u) {
            int n = snprintf(out + j, cap - j, "\\u%04x", (unsigned)c);
            if (n < 0 || (size_t)n >= cap - j)
                break;
            j += (size_t)n;
        } else
            out[j++] = (char)c;
    }
    out[j] = '\0';
    return (int)j;
}

static const char *mime_from_path(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot)
        return "application/octet-stream";
    if (strcasecmp(dot, ".png") == 0)
        return "image/png";
    if (strcasecmp(dot, ".jpg") == 0 || strcasecmp(dot, ".jpeg") == 0)
        return "image/jpeg";
    if (strcasecmp(dot, ".webp") == 0)
        return "image/webp";
    if (strcasecmp(dot, ".gif") == 0)
        return "image/gif";
    return "image/jpeg";
}

static int http_post_json(const char *url, const char *body, curl_mem_t *resp)
{
    CURL *c = curl_easy_init();
    if (!c)
        return -1;
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_POST, 1L);
    struct curl_slist *hdr = NULL;
    hdr      = curl_slist_append(hdr, "Content-Type: application/json");
    curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdr);
    curl_easy_setopt(c, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(c, CURLOPT_POSTFIELDSIZE, (long)strlen(body));
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_append);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 300L);
    CURLcode rc = curl_easy_perform(c);
    long       code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_slist_free_all(hdr);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK || code < 200 || code >= 300)
        return -1;
    return 0;
}

static int http_post_multipart_transcribe(const char *base_url,
                                          const char *file_path, curl_mem_t *resp)
{
    CURL *c = curl_easy_init();
    if (!c)
        return -1;
    char url[512];
    snprintf(url, sizeof url, "%s/v1/audio/transcriptions", base_url);

    curl_mime *mime = curl_mime_init(c);
    curl_mimepart *p = curl_mime_addpart(mime);
    curl_mime_name(p, "file");
    curl_mime_filedata(p, file_path);
    p = curl_mime_addpart(mime);
    curl_mime_name(p, "model");
    const char *am = getenv("COS_AUDIO_MODEL");
    curl_mime_data(p, (am && am[0]) ? am : "whisper-1", CURL_ZERO_TERMINATED);

    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_MIMEPOST, mime);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_append);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, resp);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 600L);
    CURLcode rc = curl_easy_perform(c);
    long       code = 0;
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_mime_free(mime);
    curl_easy_cleanup(c);
    if (rc != CURLE_OK || code < 200 || code >= 300)
        return -1;
    return 0;
}

static int extract_json_string_after(const char *body, const char *key,
                                     char *out, size_t cap)
{
    const char *p = strstr(body, key);
    if (!p)
        return -1;
    p += strlen(key);
    while (*p && *p != ':')
        p++;
    if (*p != ':')
        return -1;
    p++;
    while (*p && (*p == ' ' || *p == '\t'))
        p++;
    if (*p != '"')
        return -1;
    p++;
    size_t j = 0;
    while (*p && j + 1 < cap) {
        if (*p == '\\' && p[1]) {
            if (j + 1 >= cap)
                break;
            out[j++] = p[1];
            p += 2;
            continue;
        }
        if (*p == '"')
            break;
        out[j++] = *p++;
    }
    out[j] = '\0';
    return (j > 0 || (p && *p == '"')) ? 0 : -1;
}

static float sigma_from_logprob_scan(const char *body)
{
    float worst = 0.f;
    const char *p = body;
    while ((p = strstr(p, "\"logprob\"")) != NULL) {
        p += 9;
        while (*p && (*p == ' ' || *p == ':' || *p == '\t'))
            p++;
        if (*p != '-')
            continue;
        char *end = NULL;
        double lp = strtod(p, &end);
        if (end == p)
            continue;
        double prob = exp(lp);
        if (prob > 1.0)
            prob = 1.0;
        if (prob < 0.0)
            prob = 0.0;
        float sig = (float)(1.0 - prob);
        if (sig > worst)
            worst = sig;
        p = end;
    }
    return worst;
}

static int extract_assistant_text(const char *body, char *out, size_t cap)
{
    const char *msg = strstr(body, "\"message\"");
    if (!msg)
        return extract_json_string_after(body, "\"content\"", out, cap);
    const char *ckey = strstr(msg, "\"content\"");
    if (!ckey)
        return -1;
    const char *t = strstr(ckey, "\"text\"");
    if (t && t - ckey < 800) {
        return extract_json_string_after(t, "\"text\"", out, cap);
    }
    return extract_json_string_after(ckey, "\"content\"", out, cap);
}

static void vision_base_url(char *out, size_t cap)
{
    const char *h = getenv("COS_VISION_SERVER");
    if (!h || !h[0])
        h = "127.0.0.1:8089";
    if (strncmp(h, "http://", 7) == 0 || strncmp(h, "https://", 8) == 0)
        snprintf(out, cap, "%s", h);
    else
        snprintf(out, cap, "http://%s", h);
}

static void audio_base_url(char *out, size_t cap)
{
    const char *h = getenv("COS_AUDIO_SERVER");
    if (!h || !h[0])
        h = "127.0.0.1:8090";
    if (strncmp(h, "http://", 7) == 0 || strncmp(h, "https://", 8) == 0)
        snprintf(out, cap, "%s", h);
    else
        snprintf(out, cap, "http://%s", h);
}

static int run_vision(const char *image_path, const char *user_txt,
                      struct cos_perception_result *out)
{
    unsigned char *raw = NULL;
    size_t         rz  = 0;
    if (read_file_all(image_path, &raw, &rz) != 0)
        return -1;
    char *b64 = (char *)malloc(rz * 2 + 8);
    if (!b64) {
        free(raw);
        return -1;
    }
    b64_encode(raw, rz, b64, rz * 2 + 8);
    free(raw);

    char mime[64];
    snprintf(mime, sizeof mime, "%s", mime_from_path(image_path));

    char esc[8192];
    json_esc(user_txt && user_txt[0] ? user_txt : "Describe the image.", esc,
             sizeof esc);

    const char *model = getenv("COS_VISION_MODEL");
    if (!model || !model[0])
        model = "vision";

    char base[256];
    vision_base_url(base, sizeof base);
    char url[640];
    snprintf(url, sizeof url, "%s/v1/chat/completions", base);

    static char json[2097152];
    int n = snprintf(json, sizeof json,
                     "{\"model\":\"%s\",\"max_tokens\":512,\"temperature\":0.2,"
                     "\"logprobs\":true,\"top_logprobs\":5,"
                     "\"messages\":[{\"role\":\"user\",\"content\":["
                     "{\"type\":\"text\",\"text\":\"%s\"},"
                     "{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:%s;"
                     "base64,%s\"}}]}]}",
                     model, esc, mime, b64);
    free(b64);
    if (n < 0 || (size_t)n >= sizeof json)
        return -1;

    curl_mem_t resp = {0};
    if (http_post_json(url, json, &resp) != 0) {
        free(resp.buf);
        return -1;
    }
    if (extract_assistant_text(resp.buf ? resp.buf : "", out->description,
                               sizeof out->description)
        != 0) {
        snprintf(out->description, sizeof out->description, "%s",
                 "(vision: parse error)");
        out->sigma = 1.0f;
        free(resp.buf);
        return -1;
    }
    float lp_sig = sigma_from_logprob_scan(resp.buf ? resp.buf : "");
    free(resp.buf);
    if (lp_sig > 1e-6f)
        out->sigma = lp_sig;
    else
        out->sigma = cos_bitnet_sigma_for_local_output(out->description);
    return 0;
}

static int run_audio_transcribe(const char *audio_path, char *text, size_t tcap,
                                float *sigma_tr)
{
    char base[256];
    audio_base_url(base, sizeof base);
    curl_mem_t resp = {0};
    if (http_post_multipart_transcribe(base, audio_path, &resp) != 0) {
        free(resp.buf);
        return -1;
    }
    if (extract_json_string_after(resp.buf ? resp.buf : "", "\"text\"", text,
                                   tcap)
        != 0) {
        free(resp.buf);
        return -1;
    }
    free(resp.buf);
    *sigma_tr = cos_bitnet_sigma_for_local_output(text);
    return 0;
}

static int run_text_pipeline(const char *prompt, struct cos_perception_result *out)
{
    cos_bitnet_server_params_t p;
    memset(&p, 0, sizeof p);
    p.n_predict   = 512;
    p.temperature = 0.2f;
    cos_bitnet_server_result_t br;
    memset(&br, 0, sizeof br);
    int rc = cos_bitnet_server_complete(prompt, &p, &br);
    if (rc != 0 || br.text == NULL) {
        snprintf(out->description, sizeof out->description, "%s",
                 "(text backend unavailable)");
        out->sigma = 1.0f;
        return -1;
    }
    snprintf(out->description, sizeof out->description, "%s", br.text);
    out->sigma = br.sigma;
    return 0;
}

static void fill_attribution(struct cos_perception_result *r)
{
    struct cos_error_attribution a =
        cos_error_attribute(r->sigma, r->sigma, 0.f, r->sigma);
    r->attribution = a.source;
}

static void persist_episode_kg(const struct cos_perception_input *in,
                               const struct cos_perception_result *r)
{
    char blob[8192];
    snprintf(blob, sizeof blob,
             "[perception mod=%d path=%s] %s\n%s", in->modality,
             in->filepath[0] ? in->filepath : "-", in->text[0] ? in->text : "",
             r->description);
    if (cos_kg_init() == 0)
        (void)cos_kg_extract_and_store(blob);

    struct cos_engram_episode ep;
    memset(&ep, 0, sizeof ep);
    ep.timestamp_ms   = wall_ms();
    ep.prompt_hash    = cos_engram_prompt_hash(blob);
    ep.sigma_combined = r->sigma;
    ep.action         = 0;
    ep.was_correct    = -1;
    ep.attribution    = r->attribution;
    ep.ttt_applied    = 0;
    (void)cos_engram_episode_store(&ep);
}

int cos_perception_init(void)
{
    static int curl_ok = 0;
    if (!curl_ok) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_ok = 1;
    }
    (void)cos_kg_init();
    return 0;
}

int cos_perception_process(const struct cos_perception_input *input,
                           struct cos_perception_result       *result)
{
    if (!input || !result)
        return -1;
    memset(result, 0, sizeof(*result));
    cos_perception_init();
    int64_t t0        = wall_ms();
    result->modality  = input->modality;
    result->sigma     = 1.0f;
    result->attribution = COS_ERR_REASONING;

    int rc = -1;
    switch (input->modality) {
    case COS_PERCEPTION_TEXT:
        rc = run_text_pipeline(
            input->text[0] ? input->text : "", result);
        break;
    case COS_PERCEPTION_IMAGE:
        if (!input->filepath[0]) {
            snprintf(result->description, sizeof result->description, "%s",
                     "(no image path)");
            break;
        }
        {
            struct stat st;
            if (stat(input->filepath, &st) != 0) {
                snprintf(result->description, sizeof result->description,
                         "(image not readable)");
                break;
            }
        }
        rc = run_vision(input->filepath,
                        input->text[0] ? input->text : NULL, result);
        break;
    case COS_PERCEPTION_AUDIO:
        if (!input->filepath[0]) {
            snprintf(result->description, sizeof result->description, "%s",
                     "(no audio path)");
            break;
        }
        {
            struct stat st;
            if (stat(input->filepath, &st) != 0) {
                snprintf(result->description, sizeof result->description,
                         "(audio not readable)");
                break;
            }
            char trans[4096];
            float s_tr = 1.f;
            if (run_audio_transcribe(input->filepath, trans, sizeof trans,
                                     &s_tr)
                != 0) {
                snprintf(result->description, sizeof result->description, "%s",
                         "(audio transcription failed)");
                result->sigma = 1.0f;
                break;
            }
            char prompt[8192];
            snprintf(prompt, sizeof prompt, "%s\n%s",
                     input->text[0] ? input->text : "", trans);
            struct cos_perception_result txt;
            memset(&txt, 0, sizeof txt);
            if (run_text_pipeline(prompt, &txt) != 0) {
                snprintf(result->description, sizeof result->description,
                         "[audio] %s", trans);
                result->sigma = fminf(1.0f, s_tr);
                rc            = 0;
                break;
            }
            snprintf(result->description, sizeof result->description, "%s",
                     txt.description);
            result->sigma =
                fminf(1.0f, s_tr * txt.sigma);
            rc = 0;
        }
        break;
    default:
        snprintf(result->description, sizeof result->description,
                 "(unknown modality)");
        break;
    }

    fill_attribution(result);
    result->latency_ms = wall_ms() - t0;
    if (rc == 0)
        persist_episode_kg(input, result);
    return rc;
}

float cos_perception_confidence(const struct cos_perception_result *result)
{
    if (!result)
        return 0.f;
    float s = result->sigma;
    if (s < 0.f)
        s = 0.f;
    if (s > 1.f)
        s = 1.f;
    return 1.0f - s;
}

#ifdef CREATION_OS_ENABLE_SELF_TESTS
void cos_perception_self_test(void)
{
    struct cos_perception_result r;
    memset(&r, 0, sizeof r);
    r.sigma = 0.25f;
    if (fabsf(cos_perception_confidence(&r) - 0.75f) > 0.01f)
        abort();
    struct cos_perception_input in;
    memset(&in, 0, sizeof in);
    memset(&r, 0, sizeof r);
    if (cos_perception_process(NULL, &r) != -1)
        abort();
    in.modality = 99;
    if (cos_perception_process(&in, &r) != -1)
        abort();
}
#endif
