/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * AGI-1 — In-context "test-time training" (TTT proxy).
 *
 * llama-server does not expose differentiable weight updates.  Phase 1
 * replays successful prior turns from the SQLite engram as few-shot
 * exemplars prepended to the user message.  This is the standard ICL
 * mechanism: the frozen model adapts its behaviour through the prefix
 * without any weight write.
 *
 * Phase 2 (LoRA train-on-the-fly) is intentionally out of scope here;
 * see docs/GOOD_FIRST_ISSUES.md and the distill pipeline.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_INPLACE_TTT_H
#define COS_SIGMA_INPLACE_TTT_H

#include <stddef.h>
#include <stdint.h>

#include "engram_persist.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Build a single user-visible prompt:
 *
 *   <instruction + k exemplar blocks>
 *   ---
 *   User: <original user_prompt>
 *
 * When `persist` is NULL or `k_shots` ≤ 0 or no rows qualify, copies
 * `user_prompt` verbatim into `out`.  Returns 0 on success, -1 on I/O
 * or truncation failure (buffer too small for even the bare prompt).
 */
int cos_inplace_ttt_compose_from_engram(
    cos_engram_persist_t *persist,
    uint64_t exclude_prompt_hash,
    float exemplar_max_sigma,
    int k_shots,
    const char *user_prompt,
    char *out, size_t out_cap);

/* Unit checks: NULL-persist path + tiny-cap error.  Returns 0 on pass. */
int cos_inplace_ttt_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_INPLACE_TTT_H */
