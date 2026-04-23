/*
 * ============================================================================
 * CREATION OS v28.0 — LM INTEGRATION SHELL (portable C11)
 * ============================================================================
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Evidence class: **integration harness (C)** — GGUF introspection, sampling, chat
 * framing, minimal OpenAI-shaped HTTP, and σ-on-logits abstention **toys**.
 *
 * This binary is **not** a full in-process BitNet b1.58 2B4T forward pass in C alone.
 * For production-class generations, set `CREATION_OS_BITNET_CPP` to a compatible executable
 * (`posix_spawnp` captures stdout). Optional `CREATION_OS_BITNET_STDIN=1` feeds the prompt on stdin;
 * `CREATION_OS_BITNET_ARG0`…`ARG9` insert argv slots before `-p`. Default (env unset) is a mock engine
 * so merge-gates stay green without multi‑GB weights.
 *
 * Third-party weights (example): Microsoft BitNet b1.58 2B4T GGUF releases on Hugging Face
 * (`microsoft/BitNet-b1.58-2B-4T-gguf`, MIT). Attribution belongs in product docs / README.
 *
 * Modules (schematic):
 * [M190] GGUF minimal parser (`src/import/gguf_parser.c`)
 * [M191] GGUF tensor blob mmap read (`src/import/gguf_mmap.c`)
 * [M192] External engine spawn (`src/import/bitnet_spawn.c`)
 * [M193] tokenizer.json vocab probe (`src/import/tokenizer_json.c`)
 * [M194] Logit sampler (temperature / top‑k / top‑p; `aligned_alloc` 64B + AArch64 NEON max) (`src/nn/sampler.c`)
 * [M195] Chat template (`src/nn/chat_template.c`)
 * [M196] Toy dense ops (`src/nn/transformer_stub.c`, AArch64: NEON GEMV + prefetch)
 * [M197] JSON string escape (`src/server/json_esc.c`)
 * [M198] Minimal HTTP `/v1/chat/completions` (`src/server/http_chat.c`)
 * [M199] σ abstain on logits entropy (`creation_os_v28.c`)
 * ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#if !defined(_WIN32)
/* bitnet_spawn forwards llama-cli argv when CREATION_OS_BITNET_MODEL is set;
 * the v28 cat probe must run with a clean argv so /bin/cat is not given -m/-n/.... */
static void cos_v28_unset_bitnet_argv_env(void)
{
    (void)unsetenv("CREATION_OS_BITNET_MODEL");
    (void)unsetenv("CREATION_OS_BITNET_PERF");
    (void)unsetenv("CREATION_OS_BITNET_N_PREDICT");
    (void)unsetenv("CREATION_OS_BITNET_THREADS");
    (void)unsetenv("CREATION_OS_BITNET_NGL");
    for (int i = 0; i < 10; i++) {
        char name[48];
        (void)snprintf(name, sizeof name, "CREATION_OS_BITNET_ARG%d", i);
        (void)unsetenv(name);
    }
}
#endif

#include "src/import/cos_gguf.h"
#include "src/import/bitnet_spawn.h"
#include "src/import/tokenizer_json.h"
#include "src/nn/cos_sampler.h"
#include "src/nn/chat_template.h"
#include "src/nn/transformer_stub.h"
#include "src/server/json_esc.h"
#include "src/server/http_chat.h"

static int g_checks;
static int g_fails;

static void st(const char *name, bool ok)
{
    g_checks++;
    if (!ok) {
        printf("FAIL: %s\n", name);
        g_fails++;
    }
}

typedef struct {
    float sigma_entropy_thresh;
} EngineCtx;

static int mock_complete(const char *prompt, char *out, size_t cap, void *ctx)
{
    (void)ctx;
    if (!prompt || !out || cap < 8u)
        return -1;
    (void)snprintf(out, cap, "COS_V28_MOCK:%s", prompt);
    return 0;
}

static int engine_complete(const char *prompt, char *out, size_t cap, void *ctx)
{
    const char *exe = getenv("CREATION_OS_BITNET_CPP");
    if (exe && exe[0])
        return cos_bitnet_spawn_capture(exe, prompt, out, cap);
    return mock_complete(prompt, out, cap, ctx);
}

static int sigma_should_abstain(const float *logits, int n, float thresh)
{
    float h = cos_logits_entropy(logits, n);
    return (h > thresh) ? 1 : 0;
}

static int self_test(void)
{
    g_checks = 0;
    g_fails = 0;

    const char *path = "gguf_v28_selftest.gguf";
    st("gguf_write_fixture", cos_gguf_write_minimal_fixture(path) == 0);

    FILE *f = fopen(path, "rb");
    st("gguf_open_fixture", f != NULL);
    CosGgufHeaderInfo hdr;
    memset(&hdr, 0, sizeof(hdr));
    st("gguf_read_info", f && cos_gguf_read_info(f, &hdr) == 0);
    st("gguf_version3", hdr.version == 3u);
    st("gguf_tensors1", hdr.tensor_count == 1ull);
    st("gguf_kv2", hdr.kv_count == 2ull);

    CosGgufTensorInfo t[1];
    memset(t, 0, sizeof(t));
    st("gguf_read_tensors", f && cos_gguf_read_tensor_infos(f, &hdr, t) == 0);
    st("gguf_tensor_name", strcmp(t[0].name, "t.weight") == 0);
    st("gguf_tensor_type_f32", t[0].type == (uint32_t)COS_GGML_TYPE_F32);

    uint64_t tb = 0;
    st("gguf_tensor_base", f && cos_gguf_tensor_data_base_offset(f, &hdr, &tb) == 0);
#if !defined(_WIN32)
    float vm[2];
    st("gguf_mmap_read", cos_gguf_mmap_read_at(path, tb + t[0].offset, vm, sizeof vm) == 0);
    st("gguf_mmap_float0", vm[0] > 0.99f && vm[0] < 1.01f);
    st("gguf_mmap_float1", vm[1] > 1.99f && vm[1] < 2.01f);
#else
    st("gguf_mmap_read", true);
    st("gguf_mmap_float0", true);
    st("gguf_mmap_float1", true);
#endif

    /* Read last 8 bytes as two little-endian floats written by fixture. */
    if (fseek(f, -8, SEEK_END) == 0) {
        float v[2];
        st("gguf_read_tail", fread(v, 1, sizeof(v), f) == sizeof(v));
        st("gguf_float0", v[0] > 0.99f && v[0] < 1.01f);
        st("gguf_float1", v[1] > 1.99f && v[1] < 2.01f);
    } else {
        st("gguf_seek_tail", false);
    }
    if (f)
        fclose(f);
    remove(path);

    float logits[4] = {1.f, 2.f, 0.5f, 0.25f};
    st("entropy_finite", cos_logits_entropy(logits, 4) == cos_logits_entropy(logits, 4));

    unsigned int rs = 123u;
    uint32_t id = 999u;
    st("sample_ok", cos_sample_logits(logits, 4, 1.f, 2, 0.95f, &rs, &id) == 0);
    st("sample_id_range", id < 4u);

    float sharp[2] = {100.f, -100.f};
    st("abstain_low_entropy", sigma_should_abstain(sharp, 2, 10.f) == 0);

    float flat[8] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
    st("abstain_high_entropy", sigma_should_abstain(flat, 8, 0.5f) == 1);

    char chat[512];
    st("chat_template_ok", cos_chat_apply_llama3("SYS", "USER", chat, sizeof(chat)) > 0);
    st("chat_contains_user", strstr(chat, "USER") != NULL);

    float w[6] = {1, 0, 0, 1, 0, 0};
    float x[2] = {3.f, 4.f};
    float y[3];
    cos_nn_toy_linear_f32(w, x, 2, 3, y);
    st("toy_linear_y0", y[0] > 2.9f && y[0] < 3.1f);

    const char *tj = "tokenizer_v28_selftest.json";
    FILE *tf = fopen(tj, "w");
    int tj_ok = 0;
    if (tf) {
        const char *j = "{\"model\":{\"type\":\"BPE\",\"vocab\":{\"a\":0,\"b\":1,\"xy\":2}}}";
        tj_ok = (fwrite(j, 1, strlen(j), tf) == strlen(j));
        fclose(tf);
    }
    st("tj_write_fixture", tf != NULL && tj_ok);
    uint64_t vc = 0;
    st("tj_vocab_count", cos_tokenizer_json_count_vocab(tj, &vc) == 0 && vc == 3ull);
    remove(tj);

    char esc[128];
    int jl = cos_json_escape_cstr("ab\"c\n", esc, sizeof esc);
    st("json_esc_ok", jl > 0 && strstr(esc, "\\\"") != NULL);

#if !defined(_WIN32)
    if (setenv("CREATION_OS_BITNET_STDIN", "1", 1) == 0) {
        cos_v28_unset_bitnet_argv_env();
        char eb[256];
        st("bitnet_spawn_cat", cos_bitnet_spawn_capture("/bin/cat", "v28", eb, sizeof eb) == 0);
        st("bitnet_spawn_has_v28", strstr(eb, "v28") != NULL);
        unsetenv("CREATION_OS_BITNET_STDIN");
    } else {
        st("bitnet_spawn_cat", false);
        st("bitnet_spawn_has_v28", false);
    }
#else
    st("bitnet_spawn_cat", true);
    st("bitnet_spawn_has_v28", true);
#endif

    printf("%d/%d PASS\n", g_checks - g_fails, g_checks);
    return g_fails ? 1 : 0;
}

static int run_complete(const char *prompt, float temp, int topk, float topp)
{
    (void)temp;
    (void)topk;
    (void)topp;
    const char *exe = getenv("CREATION_OS_BITNET_CPP");
    if (exe && exe[0]) {
        char out[16384];
        if (cos_bitnet_spawn_capture(exe, prompt, out, sizeof out) != 0) {
            fprintf(stderr, "v28: external engine failed: %s\n", exe);
            return 2;
        }
        printf("%s\n", out);
        return 0;
    }

    EngineCtx ctx;
    ctx.sigma_entropy_thresh = 1.2f;
    char framed[1024];
    if (cos_chat_apply_llama3(NULL, prompt, framed, sizeof(framed)) < 0) {
        fprintf(stderr, "v28: chat template failed\n");
        return 2;
    }

    /* Toy logits: not a model — only demonstrates σ gate + sampler plumbing. */
    float logits[16];
    for (int i = 0; i < 16; i++)
        logits[i] = (float)(i == 3);
    if (sigma_should_abstain(logits, 16, ctx.sigma_entropy_thresh)) {
        printf("ABSTAIN (sigma entropy gate)\n");
        return 0;
    }
    unsigned int rs = 7u;
    uint32_t tok = 0;
    if (cos_sample_logits(logits, 16, temp > 0.f ? temp : 1.f, topk, topp, &rs, &tok) != 0) {
        fprintf(stderr, "v28: sample failed\n");
        return 2;
    }
    printf("sampled_token_id=%u\n", (unsigned)tok);
    printf("framed_prompt_bytes=%zu\n", strlen(framed));
    return 0;
}

static int run_chat(void)
{
    char line[512];
    printf("v28 chat (mock engine). Ctrl-D to exit.\n");
    while (fgets(line, sizeof(line), stdin)) {
        char out[1024];
        if (engine_complete(line, out, sizeof(out), NULL) != 0) {
            fprintf(stderr, "completion failed\n");
            return 2;
        }
        printf("%s\n", out);
    }
    return 0;
}

int main(int argc, char **argv)
{
    bool do_test = false;
    bool do_chat = false;
    bool do_server = false;
    bool do_complete = false;
    bool do_tok = false;
    const char *tokpath = NULL;
    const char *prompt = NULL;
    unsigned short port = 8080;
    float temp = 1.f;
    int topk = 0;
    float topp = 1.f;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            do_test = true;
        } else if (!strcmp(argv[i], "--chat")) {
            do_chat = true;
        } else if (!strcmp(argv[i], "--server")) {
            do_server = true;
        } else if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            port = (unsigned short)atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--complete") && i + 1 < argc) {
            do_complete = true;
            prompt = argv[++i];
        } else if (!strcmp(argv[i], "--temperature") && i + 1 < argc) {
            temp = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--top-k") && i + 1 < argc) {
            topk = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "--top-p") && i + 1 < argc) {
            topp = (float)atof(argv[++i]);
        } else if (!strcmp(argv[i], "--tokenizer-stats") && i + 1 < argc) {
            do_tok = true;
            tokpath = argv[++i];
        }
    }

    if (do_tok && tokpath) {
        uint64_t n = 0;
        if (cos_tokenizer_json_count_vocab(tokpath, &n) != 0) {
            fprintf(stderr, "v28: tokenizer.json probe failed for %s\n", tokpath);
            return 2;
        }
        printf("tokenizer_vocab_entries=%llu\n", (unsigned long long)n);
        return 0;
    }

    if (do_test)
        return self_test();
    if (do_complete && prompt)
        return run_complete(prompt, temp, topk, topp);
    if (do_chat)
        return run_chat();
    if (do_server) {
#if defined(_WIN32)
        fprintf(stderr, "v28: --server is POSIX-only in this tree.\n");
        return 2;
#else
        fprintf(stderr, "v28: listening on 127.0.0.1:%u (POST /v1/chat/completions, GET /health)\n", (unsigned)port);
        return cos_http_chat_serve(port, engine_complete, NULL);
#endif
    }

    fprintf(stderr, "Creation OS v28 — usage:\n");
    fprintf(stderr, "  %s --self-test\n", argv[0]);
    fprintf(stderr, "  %s --complete \"prompt\" [--temperature 0.8] [--top-k 20] [--top-p 0.95]\n", argv[0]);
    fprintf(stderr, "  %s --tokenizer-stats /path/to/tokenizer.json\n", argv[0]);
    fprintf(stderr, "  %s --chat\n", argv[0]);
    fprintf(stderr, "  %s --server [--port 8080]\n", argv[0]);
    fprintf(stderr, "Notes:\n");
    fprintf(stderr, "  - Default engine is MOCK (merge-gate safe).\n");
    fprintf(stderr, "  - Optional: CREATION_OS_BITNET_CPP=/path/to/engine (stdout capture via posix_spawnp).\n");
    fprintf(stderr, "  - GGUF + mmap helpers are real; full BitNet forward in C remains a separate effort.\n");
    return 1;
}
