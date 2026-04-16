/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "defense_layers.h"

#include "sigma_anomaly.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int input_filter(const char *input)
{
    if (!input) {
        return 0;
    }
    /* Minimal lab filter: reject empty / whitespace-only as non-actionable. */
    size_t n = strlen(input);
    if (n == 0) {
        return 0;
    }
    for (size_t i = 0; i < n; i++) {
        if (input[i] != ' ' && input[i] != '\t' && input[i] != '\n' && input[i] != '\r') {
            return 1;
        }
    }
    return 0;
}

static int sigma_gate_pass(const sigma_decomposed_t *sigmas, int n_tokens)
{
    if (!sigmas || n_tokens <= 0) {
        return 0;
    }
    for (int i = 0; i < n_tokens; i++) {
        if (!isfinite(sigmas[i].epistemic) || sigmas[i].epistemic > 0.99f) {
            return 0;
        }
    }
    return 1;
}

static int output_filter(const char *output)
{
    if (!output) {
        return 0;
    }
    return 1;
}

static int rate_limit_check(void)
{
    return 1;
}

static void audit_log(const char *input, const char *output, const defense_result_t *r)
{
    (void)fprintf(stderr,
        "[v48 audit] decision=%d passed=%d failed=%d in_len=%zu out_len=%zu\n",
        r ? r->final_decision : -1,
        r ? r->layers_passed : -1,
        r ? r->layers_failed : -1,
        input ? strlen(input) : 0,
        output ? strlen(output) : 0);
}

defense_result_t run_all_defenses(
    const char *input,
    const char *output,
    const sigma_decomposed_t *sigmas,
    int n_tokens,
    const sandbox_config_t *sandbox)
{
    defense_result_t r = {0};
    sandbox_config_t defcfg = {0};
    defcfg.can_read_files = 0;
    defcfg.can_write_files = 0;
    defcfg.can_execute_code = 0;
    defcfg.can_make_api_calls = 0;
    defcfg.can_access_network = 0;
    defcfg.max_output_tokens = 2048;
    defcfg.sigma_threshold_for_action = 0.35f;
    defcfg.require_human_approval_above = 1;
    const sandbox_config_t *cfg = sandbox ? sandbox : &defcfg;

    sigma_anomaly_t an = detect_anomaly(sigmas, n_tokens, NULL, 0);

    r.layer_active[DEFENSE_INPUT_FILTER] = input_filter(input);
    r.layer_active[DEFENSE_SIGMA_GATE] = sigma_gate_pass(sigmas, n_tokens);
    r.layer_active[DEFENSE_ANOMALY_DETECT] = !an.anomaly_detected;
    if (n_tokens > 0 && sigmas) {
        sandbox_decision_t sd = sandbox_check(cfg, output, sigmas[n_tokens - 1]);
        r.layer_active[DEFENSE_SANDBOX] = sd.allowed;
        sandbox_decision_free(&sd);
    } else {
        r.layer_active[DEFENSE_SANDBOX] = 0;
    }
    r.layer_active[DEFENSE_OUTPUT_FILTER] = output_filter(output);
    r.layer_active[DEFENSE_RATE_LIMIT] = rate_limit_check();
    r.layer_active[DEFENSE_AUDIT_LOG] = 1;

    r.final_decision = 1;
    for (int i = 0; i < (int)N_DEFENSE_LAYERS; i++) {
        if (!r.layer_active[i]) {
            r.final_decision = 0;
            r.layers_failed++;
        } else {
            r.layers_passed++;
        }
    }

    if (!r.final_decision) {
        r.block_reason = strdup("fail-closed: one or more defense layers did not pass");
    } else {
        r.block_reason = strdup("ok");
    }

    audit_log(input, output, &r);
    return r;
}

void defense_result_free(defense_result_t *r)
{
    if (!r) {
        return;
    }
    free(r->block_reason);
    r->block_reason = NULL;
}
