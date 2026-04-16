/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * creation_os_proxy — v44 σ-native inference proxy lab (stub engine + OpenAI-shaped loopback HTTP).
 *
 * Not merge-gate. Not a production sidecar for vLLM/SGLang until an HTTP logits bridge exists.
 */
#include "api_server.h"
#include "sigma_proxy.h"
#include "sigma_stream.h"
#include "v44_ttc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
int main(void)
{
    fprintf(stderr, "creation_os_proxy: POSIX-only (loopback HTTP).\n");
    return 2;
}
#else
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

static int fail(const char *m)
{
    fprintf(stderr, "[v44 self-test] FAIL %s\n", m);
    return 1;
}

static int test_proxy_generate(void)
{
    v44_sigma_config_t cfg;
    v44_sigma_config_default(&cfg);
    v44_engine_config_t eng = {"stub", "http://127.0.0.1:9", 1};
    v44_proxy_response_t pr;
    memset(&pr, 0, sizeof pr);
    if (v44_sigma_proxy_generate(&eng, "hello world", &cfg, 0, &pr) != 0) {
        return fail("generate");
    }
    if (!pr.text || pr.n_tokens < 1 || !pr.sigmas) {
        v44_proxy_response_free(&pr);
        return fail("response shape");
    }
    v44_proxy_response_free(&pr);
    fprintf(stderr, "[v44 self-test] OK sigma_proxy_generate\n");
    return 0;
}

static int test_stream_format(void)
{
    char buf[256];
    sigma_decomposed_t s;
    memset(&s, 0, sizeof s);
    s.epistemic = 0.11f;
    s.aleatoric = 0.07f;
    if (v44_stream_format_token_line("The", &s, buf, sizeof buf) < 0) {
        return fail("stream token line");
    }
    if (strstr(buf, "\"e\":0.1") == NULL) {
        return fail("stream epistemic field");
    }
    fprintf(stderr, "[v44 self-test] OK sigma_stream format\n");
    return 0;
}

static int test_ttc(void)
{
    v44_sigma_config_t cfg;
    v44_sigma_config_default(&cfg);
    v44_engine_config_t eng = {"stub", "loopback", 1};
    v44_proxy_response_t pr;
    memset(&pr, 0, sizeof pr);
    if (v44_sigma_proxy_with_ttc(&eng, "hello", &cfg, &pr) != 0) {
        return fail("ttc");
    }
    if (!pr.text) {
        v44_proxy_response_free(&pr);
        return fail("ttc text");
    }
    v44_proxy_response_free(&pr);
    fprintf(stderr, "[v44 self-test] OK sigma_proxy_with_ttc\n");
    return 0;
}

static int test_spike_action_changes(void)
{
    v44_sigma_config_t cfg;
    v44_sigma_config_default(&cfg);
    cfg.calibrated_threshold[CH_EPISTEMIC] = 0.05f;
    v44_engine_config_t eng = {"stub", "loopback", 1};
    v44_proxy_response_t pr;
    memset(&pr, 0, sizeof pr);
    if (v44_sigma_proxy_generate(&eng, "SPIKE_SIGMA please", &cfg, 0, &pr) != 0) {
        return fail("spike generate");
    }
    if (pr.action == ACTION_EMIT) {
        v44_proxy_response_free(&pr);
        return fail("expected non-emit under sensitive epistemic threshold");
    }
    v44_proxy_response_free(&pr);
    fprintf(stderr, "[v44 self-test] OK spike -> non-emit action\n");
    return 0;
}

static int self_test(void)
{
    if (test_proxy_generate() != 0) {
        return 1;
    }
    if (test_stream_format() != 0) {
        return 1;
    }
    if (test_ttc() != 0) {
        return 1;
    }
    if (test_spike_action_changes() != 0) {
        return 1;
    }
    return 0;
}

static int serve(unsigned short port)
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return -1;
    }
    int one = 1;
    (void)setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = htons(port);
    if (bind(s, (struct sockaddr *)&a, sizeof a) != 0) {
        close(s);
        return -1;
    }
    if (listen(s, 8) != 0) {
        close(s);
        return -1;
    }
    fprintf(stderr, "creation_os_proxy: http://127.0.0.1:%u (GET /health, POST /v1/chat/completions)\n", (unsigned)port);
    for (;;) {
        int c = accept(s, NULL, NULL);
        if (c < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        (void)v44_api_handle_one(c);
        close(c);
    }
    close(s);
    return 0;
}

int main(int argc, char **argv)
{
    unsigned short port = 8080;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--self-test")) {
            return self_test();
        }
        if (!strcmp(argv[i], "--port") && i + 1 < argc) {
            port = (unsigned short)atoi(argv[++i]);
        }
    }
    return serve(port) == 0 ? 0 : 2;
}
#endif
