/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * creation_os_server — entry point for the v106 σ-Server.
 *
 *     creation_os_server [--config PATH] [--host H] [--port N]
 *                        [--gguf PATH] [--model-id ID] [--n-ctx N]
 *                        [--tau-abstain F] [--aggregator mean|product]
 *                        [--web-root DIR] [--self-test] [-h|--help]
 *
 * Order of precedence (highest wins):
 *   1. CLI flags
 *   2. --config PATH  (or ~/.creation-os/config.toml if not given)
 *   3. COS_V101_AGG env var (affects aggregator default only)
 *   4. Built-in defaults (cos_v106_config_defaults)
 *
 * `--self-test` performs a zero-network sanity: parses a synthetic
 * JSON body through cos_v106_json_* helpers and asserts expected
 * outputs.  Used by `make check-v106` on hosts without the GGUF.
 */
#include "server.h"

#include "../v101/bridge.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int v106_self_test(void)
{
    /* 1. Config defaults. */
    cos_v106_config_t cfg;
    cos_v106_config_defaults(&cfg);
    if (strcmp(cfg.host, "127.0.0.1") != 0) return 1;
    if (cfg.port != 8080) return 2;
    if (strcmp(cfg.aggregator, "product") != 0) return 3;
    if (cfg.tau_abstain <= 0.0 || cfg.tau_abstain > 1.0) return 4;
    if (cfg.n_ctx != 2048) return 5;

    /* 2. JSON extraction. */
    const char *body =
        "{"
        "\"model\":\"bitnet-b1.58-2B\","
        "\"messages\":[{\"role\":\"system\",\"content\":\"sys\"},"
                       "{\"role\":\"user\",\"content\":\"hello\\nworld\"}],"
        "\"max_tokens\":64,"
        "\"stream\":true,"
        "\"temperature\":0.7"
        "}";
    char s[256];
    if (cos_v106_json_get_string(body, "model", s, sizeof s) <= 0) return 10;
    if (strcmp(s, "bitnet-b1.58-2B") != 0)  return 11;
    if (cos_v106_json_get_int(body, "max_tokens", -1) != 64) return 12;
    if (cos_v106_json_stream_flag(body) != 1) return 13;
    if (cos_v106_json_get_double(body, "temperature", -1.0) != 0.7) return 14;

    char user[64];
    int n = cos_v106_json_extract_last_user(body, user, sizeof user);
    if (n < 0) return 20;
    if (strcmp(user, "hello\nworld") != 0) return 21;

    /* 3. A body with no user role must fail cleanly. */
    const char *bad = "{\"messages\":[{\"role\":\"system\",\"content\":\"s\"}]}";
    if (cos_v106_json_extract_last_user(bad, user, sizeof user) != -1) return 30;

    /* 4. Stream flag: must not match "stream":"true" inside a string. */
    const char *tricky = "{\"description\":\"stream: true\"}";
    if (cos_v106_json_stream_flag(tricky) != 0) return 40;

    /* 5. Aggregator name round-trip. */
    if (strcmp(cos_v101_aggregator_name(COS_V101_AGG_MEAN),    "mean")    != 0) return 50;
    if (strcmp(cos_v101_aggregator_name(COS_V101_AGG_PRODUCT), "product") != 0) return 51;

    printf("v106 σ-Server: self-test OK (defaults, JSON helpers, aggregator names)\n");
    return 0;
}

static void usage(const char *argv0)
{
    fprintf(stderr,
            "usage: %s [options]\n"
            "\n"
            "server options:\n"
            "  --config PATH              config file (default ~/.creation-os/config.toml)\n"
            "  --host HOST                bind host (default 127.0.0.1)\n"
            "  --port N                   bind port (default 8080)\n"
            "\n"
            "model options:\n"
            "  --gguf PATH                GGUF model path\n"
            "  --model-id STRING          /v1/models id (default from file name)\n"
            "  --n-ctx N                  context window (default 2048)\n"
            "\n"
            "sigma options:\n"
            "  --tau-abstain F            abstention threshold (default 0.7)\n"
            "  --aggregator mean|product  default σ aggregator (default product)\n"
            "\n"
            "other:\n"
            "  --web-root DIR             static files root for /  (default ./web)\n"
            "  --self-test                run in-process sanity and exit 0\n"
            "  -h, --help                 print this message\n",
            argv0);
}

int main(int argc, char **argv)
{
    cos_v106_config_t cfg;
    cos_v106_config_defaults(&cfg);

    /* First pass: look for --config / --self-test / --help so we
     * can decide whether to load a file before applying CLI flags
     * on top of it. */
    const char *config_path = NULL;
    int want_self_test = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)    config_path = argv[++i];
        else if (strcmp(argv[i], "--self-test") == 0)             want_self_test = 1;
        else if (strcmp(argv[i], "-h") == 0 ||
                 strcmp(argv[i], "--help") == 0) { usage(argv[0]); return 0; }
    }

    if (want_self_test) return v106_self_test();

    char default_path[1024];
    if (!config_path && cos_v106_default_config_path(default_path, sizeof default_path) == 0) {
        /* Load only if the default path exists. */
        FILE *f = fopen(default_path, "r");
        if (f) { fclose(f); config_path = default_path; }
    }
    if (config_path) {
        int rc = cos_v106_config_load(&cfg, config_path);
        if (rc != 0 && rc != -1) {
            fprintf(stderr, "cos_v106: failed to load config %s (rc=%d)\n",
                    config_path, rc);
        }
    }

    /* Second pass: CLI flags on top of config file. */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "--config") == 0 || strcmp(a, "--self-test") == 0) {
            if (strcmp(a, "--config") == 0) i++;
            continue;
        }
        if (strcmp(a, "--host") == 0 && i + 1 < argc) {
            snprintf(cfg.host, sizeof cfg.host, "%s", argv[++i]);
        } else if (strcmp(a, "--port") == 0 && i + 1 < argc) {
            cfg.port = atoi(argv[++i]);
        } else if (strcmp(a, "--gguf") == 0 && i + 1 < argc) {
            snprintf(cfg.gguf_path, sizeof cfg.gguf_path, "%s", argv[++i]);
        } else if (strcmp(a, "--model-id") == 0 && i + 1 < argc) {
            snprintf(cfg.model_id, sizeof cfg.model_id, "%s", argv[++i]);
        } else if (strcmp(a, "--n-ctx") == 0 && i + 1 < argc) {
            cfg.n_ctx = atoi(argv[++i]);
        } else if (strcmp(a, "--tau-abstain") == 0 && i + 1 < argc) {
            cfg.tau_abstain = atof(argv[++i]);
        } else if (strcmp(a, "--aggregator") == 0 && i + 1 < argc) {
            snprintf(cfg.aggregator, sizeof cfg.aggregator, "%s", argv[++i]);
        } else if (strcmp(a, "--web-root") == 0 && i + 1 < argc) {
            snprintf(cfg.web_root, sizeof cfg.web_root, "%s", argv[++i]);
        } else {
            fprintf(stderr, "cos_v106: unknown argument '%s'\n", a);
            usage(argv[0]);
            return 2;
        }
    }

    /* If no model_id was set, derive one from the gguf basename. */
    if (cfg.model_id[0] == '\0' || strcmp(cfg.model_id, "creation-os") == 0) {
        const char *slash = strrchr(cfg.gguf_path, '/');
        const char *base = slash ? slash + 1 : cfg.gguf_path;
        if (*base) snprintf(cfg.model_id, sizeof cfg.model_id, "%s", base);
    }

    return cos_v106_server_run(&cfg);
}
