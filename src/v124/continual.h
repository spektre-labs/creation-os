/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v124 σ-Continual — on-device living weights.
 *
 * v111.3 ran one SFT pass.  v120 ran one distillation pass.  v124 runs
 * *continual* fine-tuning: the model keeps learning while the operator
 * uses it.  Concretely:
 *
 *   1. σ-buffer.  The last N (default 100) interactions are stored in
 *      a ring buffer together with their σ_product and, if the user
 *      corrected the assistant, the correction text.  Persisted at
 *      `~/.creation-os/learning_buffer.jsonl`.
 *
 *   2. Idle-time trigger.  When the v106 server has been idle for
 *      `idle_threshold_sec` (default 60 s) AND the buffer holds at
 *      least `training_trigger_size` fresh rows, a micro-tune is
 *      scheduled.  The operator pays no interactive cost.
 *
 *   3. σ-targeted batch selection.  From the buffer we pick:
 *        (a) high-σ rows   (σ > τ_high, where the model was uncertain),
 *        (b) anchor rows   (σ < τ_anchor on the same topic, from v115
 *            memory, so the weights don't drift away from what the
 *            model already reliably knows),
 *        (c) preference pairs — (wrong answer, correction) tuples
 *            produced when the operator edited the model's reply.
 *
 *   4. Micro-tune.  `lora_steps` (default 10) MLX LoRA steps on the
 *      selected batch.  Adapter saved as
 *          models/v124/continual_epoch_<N>.safetensors
 *      and hot-swapped into v106 without restarting the server.
 *
 *   5. Forgetting prevention.  Every `smoke_every_n_epochs` (default
 *      10) epochs we rerun a fixed 50-question baseline.  If baseline
 *      accuracy drops by more than `max_baseline_drop` (default 0.02)
 *      we rollback to the previous adapter and flag the epoch as
 *      *catastrophic forgetting prevented*.
 *
 * v124.0 (this kernel) is the **policy + buffer + forgetting-smoke
 * harness** in pure C.  It ships a deterministic synthetic baseline
 * so the merge-gate can assert the full state machine — including the
 * rollback path — without MLX, weights, or network.  The actual MLX
 * LoRA trainer and the v106 hot-swap wire-up land in v124.1; see
 * docs/v124/README.md.
 *
 * Result: this is what Living Weights / TTT-E2E frameworks promise
 * and don't deliver locally.  Creation OS delivers the policy layer
 * end-to-end, with an honest-claims gate, today.
 */
#ifndef COS_V124_CONTINUAL_H
#define COS_V124_CONTINUAL_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V124_BUFFER_MAX          128    /* ring capacity (default 100 used) */
#define COS_V124_MAX_FIELD           512    /* per-string cap in-record */

#define COS_V124_IDLE_SEC_DEFAULT       60
#define COS_V124_TRIGGER_SIZE_DEFAULT    8
#define COS_V124_TAU_HIGH_DEFAULT        0.60f   /* high σ → needs learning */
#define COS_V124_TAU_ANCHOR_DEFAULT      0.30f   /* low σ  → anchor against drift */
#define COS_V124_LORA_STEPS_DEFAULT     10
#define COS_V124_SMOKE_EVERY_DEFAULT    10
#define COS_V124_MAX_BASELINE_DROP_DEF   0.02f
#define COS_V124_SMOKE_BASELINE_N_DEF   50

typedef struct cos_v124_interaction {
    uint64_t ts_unix;                     /* seconds */
    char     prompt  [COS_V124_MAX_FIELD];
    char     response[COS_V124_MAX_FIELD];
    float    sigma_product;
    int      user_corrected;              /* 0 | 1 */
    char     correction[COS_V124_MAX_FIELD]; /* if user_corrected */
} cos_v124_interaction_t;

typedef struct cos_v124_buffer {
    cos_v124_interaction_t items[COS_V124_BUFFER_MAX];
    int head;   /* next write slot */
    int size;   /* 0 .. COS_V124_BUFFER_MAX */
    int capacity;
} cos_v124_buffer_t;

typedef struct cos_v124_config {
    uint64_t idle_threshold_sec;      /* 60 */
    int      training_trigger_size;   /* 8 rows */
    float    tau_high;                /* 0.60 */
    float    tau_anchor;              /* 0.30 */
    int      lora_steps;              /* 10 */
    int      smoke_every_n_epochs;    /* 10 */
    float    max_baseline_drop;       /* 0.02 */
    int      smoke_baseline_n;        /* 50 */
    int      buffer_capacity;         /* 100 */
} cos_v124_config_t;

typedef enum {
    COS_V124_TRIGGER_SKIP     = 0,  /* not idle or buffer too small */
    COS_V124_TRIGGER_TRAIN    = 1,  /* run micro-tune only */
    COS_V124_TRIGGER_SMOKE    = 2,  /* run micro-tune + forgetting smoke */
} cos_v124_trigger_t;

typedef struct cos_v124_training_batch {
    int n_high_sigma;     /* rows with σ > τ_high */
    int n_anchors;        /* low-σ anchor rows (synthetic from memory) */
    int n_preferences;    /* correction pairs */
    int n_total;          /* sum of the above */
} cos_v124_training_batch_t;

typedef struct cos_v124_epoch {
    uint32_t epoch_id;                /* 1-based */
    cos_v124_training_batch_t batch;
    int      lora_steps_ran;
    int      smoke_ran;
    float    baseline_accuracy_before;
    float    baseline_accuracy_after;
    int      forgetting_detected;     /* 0 | 1 */
    int      rolled_back;             /* 0 | 1 */
    uint32_t active_adapter_version;  /* 0 = base, >= 1 = this epoch's adapter */
} cos_v124_epoch_t;

typedef struct cos_v124_stats {
    int epochs_completed;
    int epochs_with_smoke;
    int epochs_rolled_back;
    float last_baseline_accuracy;      /* -1 if no smoke ran yet */
    uint32_t active_adapter_version;
} cos_v124_stats_t;

/* Baseline accuracy callback — in v124.0 this is a deterministic
 * stub so the merge-gate can exercise both the healthy and the
 * forgetting paths without running real inference.  v124.1 replaces
 * it with a v101/v106 call against a 50-question frozen baseline. */
typedef float (*cos_v124_baseline_fn)(
    uint32_t adapter_version,
    int      n_questions,
    void    *ctx);

/* ----------------------------------------------------------------- */

void cos_v124_config_defaults(cos_v124_config_t *cfg);

/* Ring buffer --------------------------------------------------------- */

void cos_v124_buffer_init   (cos_v124_buffer_t *buf, int capacity);
void cos_v124_buffer_append (cos_v124_buffer_t *buf,
                             const cos_v124_interaction_t *it);
int  cos_v124_buffer_size   (const cos_v124_buffer_t *buf);

/* Selection: writes `*out` in-place; returns n_total. */
int  cos_v124_buffer_select (const cos_v124_config_t *cfg,
                             const cos_v124_buffer_t *buf,
                             cos_v124_training_batch_t *out);

/* Persistence: JSONL writer — one interaction per line.
 * Returns number of rows written, -1 on I/O error. */
int  cos_v124_buffer_write_jsonl(const cos_v124_buffer_t *buf,
                                 FILE *fp);

/* Trigger policy -------------------------------------------------------- */

cos_v124_trigger_t
cos_v124_trigger_decide(const cos_v124_config_t *cfg,
                        uint64_t idle_seconds,
                        int buffer_size,
                        int next_epoch_id);

/* Epoch execution (synthetic micro-tune + optional smoke + rollback). */
int  cos_v124_run_epoch(const cos_v124_config_t *cfg,
                        cos_v124_buffer_t       *buf,      /* drained on success */
                        uint32_t                 prev_adapter_version,
                        float                    prev_baseline_accuracy,
                        cos_v124_baseline_fn     baseline_fn,
                        void                    *baseline_ctx,
                        cos_v124_epoch_t        *out);

/* Serialization --------------------------------------------------------- */

int  cos_v124_epoch_to_json(const cos_v124_epoch_t *e,
                            char *out, size_t out_cap);

int  cos_v124_stats_to_json(const cos_v124_stats_t *s,
                            char *out, size_t out_cap);

/* Self-test: exercises buffer, selection, trigger policy, healthy and
 * pathological baseline paths, rollback bookkeeping.  0 on success. */
int  cos_v124_self_test(void);

#ifdef __cplusplus
}
#endif
#endif  /* COS_V124_CONTINUAL_H */
