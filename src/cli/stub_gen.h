/* Shared stub generator for the reference CLIs.
 *
 * Creation OS ships the control plane (σ-pipeline orchestrator +
 * 15 primitives).  The reference CLIs (cos_chat, cos_benchmark,
 * cos_cost) exercise that control plane with a deterministic
 * stub that maps prompt-prefix keywords to scripted (text, σ)
 * tuples:
 *
 *   "low:<rest>"       → σ=0.05       (ACCEPT)
 *   "mid:<rest>"       → σ=0.55       (plateau → RETHINK ×3 → ABST)
 *   "high:<rest>"      → σ=0.92       (immediate ABSTAIN)
 *   "improve:<rest>"   → σ=0.55/0.45/0.35   (RETHINK → ACCEPT)
 *   anything else      → σ=0.30       (RETHINK → ACCEPT)
 *
 * Escalation returns σ=0.08 at €0.012 per call.
 *
 * `cos_cli_chat_generate()` is the **chat** entry point: optional
 * `CREATION_OS_BITNET_EXE` subprocess (see `bitnet_spawn.h`), then
 * the canonical arithmetic demo prompt **What is 2+2?** → `"4"`
 * with low σ, then the prefix stub rules above.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_CLI_STUB_GEN_H
#define COS_CLI_STUB_GEN_H

#include "pipeline.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Magic at offset 0 so `generate_ctx` can be either this struct or a
 * legacy bare `cos_sigma_codex_t *` (first field there is a pointer,
 * which never equals this constant on real hosts). */
#define COS_CLI_GENERATE_CTX_MAGIC  0xC053100Du

struct cos_engram_persist_s;
typedef struct cos_engram_persist_s cos_engram_persist_t;

/* Optional AGI-1 hook: build an ICL-augmented prompt from the engram.
 * Returns 0 on success; on failure the generator falls back to the
 * raw user prompt. */
typedef int (*cos_cli_icl_compose_fn)(
    void *icl_ctx,
    uint64_t exclude_prompt_hash,
    float exemplar_max_sigma,
    int k_shots,
    const char *user_prompt,
    char *out, size_t out_cap);

typedef struct {
    uint32_t                   magic;
    uint32_t                   _pad;
    const cos_sigma_codex_t   *codex;
    cos_engram_persist_t      *persist; /* optional SQLite engram   */
    cos_sigma_engram_t        *runtime_engram; /* in-memory σ-engram;
                    * optional degrade path when local HTTP fails      */
    int                        pipeline_local_only; /* 1 = cos --local-only */
    float                      icl_exemplar_max_sigma; /* σ < this    */
    int                        icl_k;      /* 0 = off                  */
    int                        icl_rethink_only; /* 1 = round>0 only    */
    cos_cli_icl_compose_fn     icl_compose;
    void                      *icl_ctx;    /* usually `persist`      */
} cos_cli_generate_ctx_t;

int cos_cli_stub_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur);

int cos_cli_stub_escalate(const char *prompt, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur,
                          uint64_t *out_bytes_sent,
                          uint64_t *out_bytes_recv);

int cos_cli_chat_generate(const char *prompt, int round, void *ctx,
                          const char **out_text, float *out_sigma,
                          double *out_cost_eur);

/** True when COS_BITNET_SERVER=1 or COS_BITNET_SERVER_EXTERNAL=1 (HTTP path). */
int cos_cli_use_bitnet_http(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_CLI_STUB_GEN_H */
