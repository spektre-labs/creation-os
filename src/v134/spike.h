/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v134 σ-Spike — event-driven σ signalling.
 *
 * The current σ stack recomputes σ_product on every token.  v134
 * turns it into a neuromorphic-style *event stream*: a σ-spike
 * fires only when σ_product moves by more than δ from the last
 * observation.  Stable tokens ⇒ no spike ⇒ zero downstream work.
 *
 * Three primitives:
 *
 *   1. Delta-threshold encoder.  Given σ_now and the last reported
 *      σ, emit a spike iff |σ_now − σ_last| ≥ δ.  Default δ = 0.10.
 *
 *   2. Burst detector.  A ring buffer over the last W tokens; if
 *      it contains ≥ K spikes the verdict is BURST → abstain.  This
 *      is O(1) per token (count updated on ring insert/evict).
 *
 *   3. Lava-compatible JSON export.  A frozen schema compatible
 *      with Intel Lava's spike-stream format (process / port /
 *      events).  v134.0 emits it as text; v134.1 hands it to a
 *      Loihi 2 dev-kit.  On non-neuromorphic hosts the simulation
 *      runs natively with identical semantics.
 *
 * σ-innovation: burst-over-spike is the LLM analogue of a
 * biological *reactive spike train* — one spike is information,
 * a sustained burst is instability.  v134 is the first σ-aware
 * layer that makes that distinction explicit in the gate.
 */
#ifndef COS_V134_SPIKE_H
#define COS_V134_SPIKE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V134_HISTORY_CAP              32
#define COS_V134_DELTA_DEFAULT         0.10f
#define COS_V134_BURST_WINDOW_DEFAULT     5
#define COS_V134_BURST_COUNT_DEFAULT      3

typedef struct cos_v134_cfg {
    float delta;             /* spike threshold in σ units           */
    int   burst_window;      /* ring-buffer length (≤ HISTORY_CAP)   */
    int   burst_count;       /* K spikes in W window → BURST         */
} cos_v134_cfg_t;

typedef enum {
    COS_V134_STABLE = 0,     /* no spike: emit normally (no σ work)  */
    COS_V134_SPIKE  = 1,     /* one spike: emit but mark             */
    COS_V134_BURST  = 2,     /* ≥ K spikes in W tokens: abstain      */
} cos_v134_verdict_t;

typedef struct cos_v134_state {
    float  last_sigma;
    int    bootstrapped;

    /* Ring buffer of spike flags (0/1) over the last W tokens. */
    int8_t ring[COS_V134_HISTORY_CAP];
    int    ring_head;
    int    ring_fill;
    int    ring_sum;         /* Σ of active window — updated in O(1) */

    /* Lifetime counters. */
    uint64_t total_tokens;
    uint64_t total_spikes;
    uint64_t total_bursts;
} cos_v134_state_t;

void  cos_v134_cfg_defaults(cos_v134_cfg_t *cfg);
void  cos_v134_init        (cos_v134_state_t *st);

/* Feed one σ observation.  Returns the verdict for this token. */
cos_v134_verdict_t
      cos_v134_feed        (cos_v134_state_t *st,
                            const cos_v134_cfg_t *cfg,
                            float sigma_now);

/* Fraction of tokens that were STABLE (no spike) — the "energy
 * saving" proxy.  Returns 0.0 when no tokens have been fed. */
float cos_v134_stable_ratio(const cos_v134_state_t *st);

/* JSON summary (for /v1/meta/spike and x-cos-sigma-spikes headers). */
int   cos_v134_to_json    (const cos_v134_state_t *st,
                           char *out, size_t cap);

/* Lava-compatible spike-stream export (text, frozen schema).
 * Emits one line per observed spike with {ts, port, verdict}. */
int   cos_v134_export_lava(const cos_v134_state_t *st,
                           const cos_v134_cfg_t   *cfg,
                           const int8_t *token_spikes, int n,
                           char *out, size_t cap);

int   cos_v134_self_test  (void);

#ifdef __cplusplus
}
#endif
#endif
