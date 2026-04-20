/* Reference stub generator — see stub_gen.h. */

#include "stub_gen.h"

#include <string.h>

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
