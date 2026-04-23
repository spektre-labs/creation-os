/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only */
/*
 * Dual-model speculative σ-decoding: fast draft (e.g. 2B), optional verify
 * (e.g. 9B) when draft σ exceeds τ_fast.  Uses llama-server HTTP only.
 */
#ifndef COS_SPECULATIVE_DECODE_H
#define COS_SPECULATIVE_DECODE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_SPEC_DECODE_RESPONSE_CAP 4096

struct cos_spec_decode_config {
    char draft_model[256];
    char verify_model[256];
    int draft_port;
    int verify_port;
    float tau_fast;
    float tau_escalate;
    int draft_max_tokens;
    int verify_max_tokens;
};

struct cos_spec_decode_result {
    char  response[COS_SPEC_DECODE_RESPONSE_CAP];
    float sigma;
    int   used_draft;
    float draft_sigma;
    float verify_sigma;
    float draft_tok_s;
    float verify_tok_s;
    float total_ms;
    float savings_ms;
};

typedef struct cos_spec_decode_result cos_spec_decode_result_t;

int cos_spec_decode_init(const struct cos_spec_decode_config *config);

int cos_spec_decode_run(const char                     *prompt,
                        const char                     *system_prompt,
                        struct cos_spec_decode_result *result);

void cos_spec_decode_print(const struct cos_spec_decode_result *r);

#ifdef __cplusplus
}
#endif

#endif /* COS_SPECULATIVE_DECODE_H */
