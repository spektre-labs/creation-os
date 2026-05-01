/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* σ-Stream demo + pinned JSON receipt.
 *
 * Emulates the canonical "quantum entanglement" stream from the
 * spec.  Every token's σ is fed through the driver; two σ-spikes
 * trigger RETHINK markers; the final trace is reported as JSON.
 */

#include "stream.h"

#include <stdio.h>
#include <string.h>

static const char *const DEMO_TOKENS[] = {
    "Quantum ",     "entanglement ", "is ",
    "a ",           "phenomenon ",
    "where ",       "particles ",    "become ",
    "correlated.",
};
static const float DEMO_SIGMAS[] = {
    0.05f, 0.08f, 0.03f,
    0.02f, 0.55f,
    0.04f, 0.60f, 0.06f,
    0.09f,
};
static const int DEMO_N =
    (int)(sizeof DEMO_TOKENS / sizeof DEMO_TOKENS[0]);

typedef struct { int idx; } gen_t;

static int gen_fn(void *s, const char **out_tok, float *out_sig, int *done) {
    gen_t *g = (gen_t *)s;
    if (g->idx >= DEMO_N) { *done = 1; return 0; }
    *out_tok = DEMO_TOKENS[g->idx];
    *out_sig = DEMO_SIGMAS[g->idx];
    *done    = 0;
    g->idx++;
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int rc = cos_sigma_stream_self_test();

    gen_t g = {0};
    cos_sigma_stream_config_t cfg = { .tau_rethink = 0.50f };
    cos_sigma_stream_trace_t  tr; cos_sigma_stream_trace_init(&tr);
    cos_sigma_stream_run(&cfg, gen_fn, &g, NULL, NULL, &tr);

    printf("{\"kernel\":\"sigma_stream\","
           "\"self_test_rc\":%d,"
           "\"tau_rethink\":%.4f,"
           "\"n_tokens\":%d,"
           "\"n_rethink\":%d,"
           "\"sigma_sum\":%.4f,"
           "\"sigma_mean\":%.4f,"
           "\"sigma_max\":%.4f,"
           "\"sigma_std\":%.4f,"
           "\"truncated_tokens\":%s,"
           "\"truncated_text\":%s,"
           "\"text_len\":%zu,"
           "\"sigma_per_token\":[",
           rc,
           (double)cfg.tau_rethink,
           tr.n_tokens, tr.n_rethink,
           (double)tr.sigma_sum, (double)tr.sigma_mean,
           (double)tr.sigma_max, (double)tr.sigma_std,
           tr.truncated_tokens ? "true" : "false",
           tr.truncated_text   ? "true" : "false",
           tr.text_len);
    for (int i = 0; i < tr.n_tokens; i++)
        printf("%s%.4f", i ? "," : "", (double)tr.sigma_per_token[i]);
    printf("],\"rethink_at_idx\":[");
    int first = 1;
    for (int i = 0; i < tr.n_tokens; i++) {
        if (tr.sigma_per_token[i] >= cfg.tau_rethink) {
            printf("%s%d", first ? "" : ",", i);
            first = 0;
        }
    }
    printf("],\"pass\":%s}\n", rc == 0 ? "true" : "false");

    cos_sigma_stream_trace_free(&tr);
    return rc == 0 ? 0 : 1;
}
