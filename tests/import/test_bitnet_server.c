/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 *
 *  tests/import/test_bitnet_server.c
 *  --------------------------------------------------------------------
 *  End-to-end smoke test: spawn llama-server, POST /completion,
 *  parse per-token probs, print (text, σ, tokens).  Manual harness
 *  (not wired into `make check`) because it needs the ~1 GiB GGUF
 *  model file and a working bitnet.cpp build.  Run:
 *
 *    make test_bitnet_server && ./test_bitnet_server "What is 2+2?"
 */

#include "bitnet_server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    const char *prompt =
        argc > 1 ? argv[1]
                 : "Q: What is 2+2?\nA:";

    cos_bitnet_server_params_t p;
    memset(&p, 0, sizeof(p));
    p.n_predict   = 24;
    p.n_probs     = 5;
    /* Leave temperature unset: server default (0.8) exercises the
     * full sampler path.  At T<=0.1 the BitNet-b1.58 fork returns
     * `"prob": null` for every token (pre-sampling shortcut); we
     * want real distributions so σ is measurable. */
    p.temperature = 0.0f;
    p.seed        = -1;

    fprintf(stdout, "→ prompt: %s\n", prompt);
    fprintf(stdout, "  starting llama-server...\n");

    if (cos_bitnet_server_ensure() != 0) {
        fprintf(stderr, "test_bitnet_server: ensure() failed — is "
                        "third_party/bitnet/build/bin/llama-server "
                        "and COS_BITNET_SERVER_MODEL (default: models/qwen3-8b-Q4_K_M.gguf) "
                        "present?\n");
        return 2;
    }

    cos_bitnet_server_result_t r;
    int rc = cos_bitnet_server_complete(prompt, &p, &r);
    if (rc != 0) {
        fprintf(stderr, "complete() failed: rc=%d\n", rc);
        return 3;
    }
    fprintf(stdout, "← text: %s\n", r.text);
    fprintf(stdout, "  σ_max=%.4f  σ_mean=%.4f  tokens=%d  "
                    "stopped=%s  %.1fms  €%.6f\n",
            (double)r.sigma, (double)r.mean_sigma,
            r.token_count,
            r.stopped_eos ? "eos" : (r.stopped_limit ? "limit" : "other"),
            r.elapsed_ms, r.cost_eur);

    /* second call to confirm persistent server */
    cos_bitnet_server_result_t r2;
    rc = cos_bitnet_server_complete("asdfghjkl", &p, &r2);
    if (rc == 0) {
        fprintf(stdout, "  control prompt 'asdfghjkl' → σ=%.4f (expected high)\n",
                (double)r2.sigma);
    }
    return 0;
}
