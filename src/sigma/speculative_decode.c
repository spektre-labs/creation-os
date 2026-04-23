/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */

#include "speculative_decode.h"

#include "bitnet_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct cos_spec_decode_config g_cfg;
static int                         g_cfg_ok;

static const char *sd_env_or(const char *name, const char *fallback)
{
    const char *v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : fallback;
}

static int sd_env_int(const char *name, int fallback)
{
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
        return fallback;
    int n = atoi(v);
    return (n > 0) ? n : fallback;
}

static float sd_env_float(const char *name, float fallback)
{
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
        return fallback;
    return (float)strtod(v, NULL);
}

static float tok_s_from_result(const cos_bitnet_server_result_t *r)
{
    if (r == NULL || r->elapsed_ms <= 0.5)
        return 0.f;
    int n = r->token_count > 0 ? r->token_count : 1;
    return (float)n / (float)(r->elapsed_ms / 1000.0);
}

/** Single /completion against a running llama-server on `port`.
 *  Saves/restores COS_BITNET_SERVER_* env for the caller. */
static int sd_complete_on_port(int                         port,
                               const char                 *prompt,
                               const char                 *system_prompt,
                               int                         n_predict,
                               cos_bitnet_server_result_t *out)
{
    const char *old_port = getenv("COS_BITNET_SERVER_PORT");
    const char *old_ext  = getenv("COS_BITNET_SERVER_EXTERNAL");
    char        portbuf[24];

    snprintf(portbuf, sizeof portbuf, "%d", port);
    setenv("COS_BITNET_SERVER_EXTERNAL", "1", 1);
    setenv("COS_BITNET_SERVER_PORT", portbuf, 1);
    cos_bitnet_server_invalidate_config();

    cos_bitnet_server_params_t p;
    memset(&p, 0, sizeof(p));
    p.n_predict     = n_predict;
    p.n_probs       = 5;
    p.seed          = -1;
    p.temperature   = 0.0f;
    p.system_prompt = system_prompt;

    int rc = cos_bitnet_server_complete(prompt, &p, out);

    if (old_port != NULL)
        setenv("COS_BITNET_SERVER_PORT", old_port, 1);
    else
        unsetenv("COS_BITNET_SERVER_PORT");

    if (old_ext != NULL)
        setenv("COS_BITNET_SERVER_EXTERNAL", old_ext, 1);
    else
        unsetenv("COS_BITNET_SERVER_EXTERNAL");

    cos_bitnet_server_invalidate_config();
    return rc;
}

int cos_spec_decode_init(const struct cos_spec_decode_config *config)
{
    memset(&g_cfg, 0, sizeof(g_cfg));
    if (config != NULL) {
        g_cfg = *config;
        g_cfg_ok = 1;
        return 0;
    }

    snprintf(g_cfg.draft_model, sizeof(g_cfg.draft_model), "%s",
             sd_env_or("COS_SPEC_DRAFT_MODEL", "qwen3.5-2b"));
    snprintf(g_cfg.verify_model, sizeof(g_cfg.verify_model), "%s",
             sd_env_or("COS_SPEC_VERIFY_MODEL", "qwen3.5-9b"));
    g_cfg.draft_port        = sd_env_int("COS_SPEC_DRAFT_PORT", 8089);
    g_cfg.verify_port       = sd_env_int("COS_SPEC_VERIFY_PORT", 8088);
    g_cfg.tau_fast          = sd_env_float("COS_SPEC_TAU_FAST", 0.15f);
    g_cfg.tau_escalate      = sd_env_float("COS_SPEC_TAU_ESCALATE", 0.50f);
    g_cfg.draft_max_tokens  = sd_env_int("COS_SPEC_DRAFT_MAX_TOKENS", 256);
    g_cfg.verify_max_tokens = sd_env_int("COS_SPEC_VERIFY_MAX_TOKENS", 512);
    if (g_cfg.draft_max_tokens < 32)
        g_cfg.draft_max_tokens = 32;
    if (g_cfg.verify_max_tokens < 32)
        g_cfg.verify_max_tokens = 32;
    g_cfg_ok = 1;
    (void)g_cfg.tau_escalate;
    return 0;
}

int cos_spec_decode_run(const char                     *prompt,
                        const char                     *system_prompt,
                        struct cos_spec_decode_result *result)
{
    if (!g_cfg_ok)
        cos_spec_decode_init(NULL);
    if (prompt == NULL || result == NULL)
        return -1;
    memset(result, 0, sizeof(*result));
    result->verify_sigma = -1.f;

    cos_bitnet_server_result_t draft;
    memset(&draft, 0, sizeof(draft));

    if (sd_complete_on_port(g_cfg.draft_port, prompt, system_prompt,
                            g_cfg.draft_max_tokens, &draft) != 0)
        return -3;

    result->draft_sigma = draft.sigma;
    result->draft_tok_s = tok_s_from_result(&draft);

    float draft_ms = (float)draft.elapsed_ms;

    const char *dt = draft.text != NULL ? draft.text : "";
    size_t      nl = strlen(dt);
    if (nl >= COS_SPEC_DECODE_RESPONSE_CAP)
        nl = COS_SPEC_DECODE_RESPONSE_CAP - 1;
    memcpy(result->response, dt, nl);
    result->response[nl] = '\0';

    if (draft.sigma < g_cfg.tau_fast) {
        result->sigma      = draft.sigma;
        result->used_draft = 1;
        result->total_ms   = draft_ms;
        float guess_verify_ms =
            sd_env_float("COS_SPEC_VERIFY_MS_GUESS", 1200.f);
        result->savings_ms = guess_verify_ms > draft_ms
                                 ? guess_verify_ms - draft_ms
                                 : 0.f;
        return 0;
    }

    cos_bitnet_server_result_t ver;
    memset(&ver, 0, sizeof(ver));

    if (sd_complete_on_port(g_cfg.verify_port, prompt, system_prompt,
                            g_cfg.verify_max_tokens, &ver) != 0) {
        result->sigma      = draft.sigma;
        result->used_draft = 1;
        result->total_ms   = draft_ms;
        result->savings_ms = 0.f;
        return 0;
    }

    float verify_ms      = (float)ver.elapsed_ms;
    result->verify_sigma = ver.sigma;
    result->verify_tok_s = tok_s_from_result(&ver);
    result->sigma        = ver.sigma;
    result->used_draft   = 0;
    result->total_ms       = draft_ms + verify_ms;
    result->savings_ms     = 0.f;

    const char *vt = ver.text != NULL ? ver.text : "";
    nl = strlen(vt);
    if (nl >= COS_SPEC_DECODE_RESPONSE_CAP)
        nl = COS_SPEC_DECODE_RESPONSE_CAP - 1;
    memcpy(result->response, vt, nl);
    result->response[nl] = '\0';

    return 0;
}

void cos_spec_decode_print(const struct cos_spec_decode_result *r)
{
    if (r == NULL)
        return;
    fprintf(stderr,
            "[spec_decode] path=%s σ=%.3f draft_σ=%.3f verify_σ=%.3f "
            "draft_tok/s=%.1f verify_tok/s=%.1f total_ms=%.0f savings_ms=%.0f\n",
            r->used_draft ? "DRAFT(2B)" : "VERIFY(9B)",
            (double)r->sigma, (double)r->draft_sigma,
            (double)r->verify_sigma, (double)r->draft_tok_s,
            (double)r->verify_tok_s, (double)r->total_ms,
            (double)r->savings_ms);
}
