/* Reference stub generator — see stub_gen.h. */

#include "stub_gen.h"

#include "bitnet_spawn.h"
#include "bitnet_sigma.h"

#include <stdlib.h>
#include <string.h>

/* One subprocess-sized buffer; cos-chat is single-threaded. */
static char g_cos_chat_bitnet_buf[8192];

static int prompt_is_what_is_2_plus_2(const char *prompt) {
    const char *p = prompt;
    if (p == NULL) return 0;
    while (*p == ' ' || *p == '\t') ++p;
    const char *want = "what is 2+2?";
    size_t i = 0;
    for (; want[i]; ++i) {
        unsigned char c = (unsigned char)p[i];
        if (c >= 'A' && c <= 'Z') c = (unsigned char)(c - 'A' + 'a');
        if ((char)c != want[i]) return 0;
    }
    /* Allow trailing whitespace only. */
    while (p[i] == ' ' || p[i] == '\t') ++i;
    return p[i] == '\0';
}

int cos_cli_stub_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur) {
    (void)ctx;
    *out_cost_eur = 0.0001;  /* BitNet-class local cost          */
    if (prompt == NULL) { *out_text = ""; *out_sigma = 1.0f; return 1; }

    if (strncmp(prompt, "low:", 4) == 0) {
        *out_text  = "a confident local answer (stub; real BitNet would fill this in).";
        *out_sigma = 0.05f;
    } else if (strncmp(prompt, "mid:", 4) == 0) {
        *out_text  = "a plateau-σ draft — not confident enough to commit.";
        *out_sigma = 0.55f;
    } else if (strncmp(prompt, "high:", 5) == 0) {
        *out_text  = "I do not know this with enough confidence to answer locally.";
        *out_sigma = 0.92f;
    } else if (strncmp(prompt, "improve:", 8) == 0) {
        float t[] = { 0.55f, 0.45f, 0.35f };
        int r = (round < 3) ? round : 2;
        *out_text  = "an answer that improves with each RETHINK pass.";
        *out_sigma = t[r];
    } else {
        *out_text  = "a default mid-confidence local answer.";
        *out_sigma = 0.30f;
    }
    return 0;
}

int cos_cli_chat_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur) {
    (void)ctx;
    if (prompt == NULL) {
        *out_text = "";
        *out_sigma = 1.0f;
        *out_cost_eur = 0.0;
        return 1;
    }
    const char *exe = getenv("CREATION_OS_BITNET_EXE");
    if (exe != NULL && exe[0] != '\0' && round == 0) {
        if (cos_bitnet_spawn_capture(exe, prompt, g_cos_chat_bitnet_buf,
                                     sizeof g_cos_chat_bitnet_buf) == 0
            && g_cos_chat_bitnet_buf[0] != '\0') {
            *out_text       = g_cos_chat_bitnet_buf;
            *out_sigma      = cos_bitnet_sigma_for_local_output(g_cos_chat_bitnet_buf);
            *out_cost_eur   = 0.0001;
            return 0;
        }
    }
    if (round == 0 && prompt_is_what_is_2_plus_2(prompt)) {
        *out_text     = "4";
        *out_sigma    = 0.03f;
        *out_cost_eur = 0.0;
        return 0;
    }
    return cos_cli_stub_generate(prompt, round, ctx, out_text, out_sigma,
                                 out_cost_eur);
}

int cos_cli_stub_escalate(const char *prompt, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur,
                          uint64_t *out_bytes_sent,
                          uint64_t *out_bytes_recv) {
    (void)prompt; (void)ctx;
    *out_text       = "an escalated answer from the cloud tier.";
    *out_sigma      = 0.08f;
    *out_cost_eur   = 0.012;
    *out_bytes_sent = 1024;
    *out_bytes_recv = 4096;
    return 0;
}
