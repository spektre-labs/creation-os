/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * ULTRA-8 — In-place test-time training hooks for the σ pipeline.
 *
 * llama-server does not expose weight gradients; we treat per-token σ
 * from the failed completion as the learning signal and compress it into
 * a 256-slot fast-weight sketch that steers the next RETHINK prompt.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_TTT_RUNTIME_H
#define COS_SIGMA_TTT_RUNTIME_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct cos_ttt_update {
    uint64_t prompt_hash;
    float    sigma_before;
    float    sigma_after;
    float    delta_weights[256];
    int      applied;
};

int cos_ttt_extract_signal(const char *response, const float *token_sigma,
                           int n_tokens, struct cos_ttt_update *update);

int cos_ttt_apply_fast_weights(struct cos_ttt_update *update);

int cos_ttt_revert(struct cos_ttt_update *update);

float cos_ttt_measure_improvement(float sigma_before, float sigma_after);

/** Called by stub_gen before each HTTP completion (round ≥ 1) when active. */
size_t cos_ttt_augment_prompt(char *dst, size_t dst_cap,
                              const char *base_prompt);

/** Pipeline entry / exit bookkeeping. */
void cos_ttt_turn_begin(void);

/** After a failed local attempt, build sketch + activate fast weights. */
int cos_ttt_prepare_rethink(const char *input, const char *failed_response,
                            float failed_sigma);

int cos_ttt_should_revert(float new_sigma);

int cos_ttt_was_reverted(void);

/** Mark whether this turn ultimately applied TTT (for episodic row). */
int cos_ttt_episode_applied_flag(void);

void cos_ttt_set_verbose(int on);

char *cos_ttt_last_verbose_line(void);

int cos_ttt_runtime_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_TTT_RUNTIME_H */
