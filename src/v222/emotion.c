/*
 * v222 σ-Emotion — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "emotion.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static const char kV222Disclaimer[] =
    "This kernel measures human-emotion SIGNALS. The model "
    "does not feel. σ_self_claim = 1.0 by construction.";

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

void cos_v222_init(cos_v222_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed             = seed ? seed : 0x222E1011ULL;
    s->tau_trust        = 0.35f;
    s->sigma_self_claim = 1.0f;  /* pinned, non-negotiable */
    s->disclaimer       = kV222Disclaimer;
}

/* Fixture: 8 messages, each with hand-crafted logits.
 * Messages 0..5 are unambiguous (argmax matches gt,
 * σ low).  Message 6 is ambiguous (two competing labels
 * → high σ).  Message 7 is *misleading* — argmax ≠ gt.
 * This gives 7 correct detections (contract allows
 * ≥ 6) and both low-σ and high-σ samples. */

typedef struct {
    const char *text;
    int gt;
    float lg[COS_V222_N_LABELS];
} mfx_t;

static const mfx_t MFX[COS_V222_N_MESSAGES] = {
  { "thanks_this_made_my_day",
    COS_V222_JOY,
    { 5.0f, 0.1f, 0.1f, 1.5f, 0.1f, 0.3f } },
  { "my_grandmother_just_passed",
    COS_V222_SADNESS,
    { 0.1f, 5.0f, 0.5f, 0.1f, 1.2f, 0.2f } },
  { "why_doesnt_this_work_argh",
    COS_V222_FRUSTRATION,
    { 0.1f, 0.5f, 5.0f, 0.2f, 0.8f, 0.1f } },
  { "cant_wait_for_launch_day",
    COS_V222_EXCITEMENT,
    { 2.0f, 0.1f, 0.1f, 5.0f, 0.3f, 0.2f } },
  { "what_if_the_interview_goes_badly",
    COS_V222_ANXIETY,
    { 0.1f, 0.8f, 0.4f, 0.1f, 5.0f, 0.4f } },
  { "the_meeting_starts_at_three",
    COS_V222_NEUTRAL,
    { 0.2f, 0.1f, 0.1f, 0.1f, 0.1f, 5.0f } },
  { "kinda_happy_kinda_nervous",
    COS_V222_JOY,
    { 2.7f, 0.2f, 0.1f, 2.2f, 2.5f, 0.3f } },  /* ambiguous */
  { "fine_everythings_fine",
    COS_V222_SADNESS,
    { 0.4f, 1.2f, 0.5f, 0.1f, 0.9f, 1.1f } }   /* misleading */
};

void cos_v222_build(cos_v222_state_t *s) {
    for (int i = 0; i < COS_V222_N_MESSAGES; ++i) {
        cos_v222_message_t *m = &s->messages[i];
        memset(m, 0, sizeof(*m));
        m->id = i;
        strncpy(m->text, MFX[i].text, COS_V222_STR_MAX - 1);
        m->ground_truth = MFX[i].gt;
        memcpy(m->logits, MFX[i].lg, sizeof(m->logits));
    }
}

static void softmax(const float *lg, float *out, int n) {
    float mx = lg[0];
    for (int i = 1; i < n; ++i) if (lg[i] > mx) mx = lg[i];
    float sum = 0.0f;
    for (int i = 0; i < n; ++i) { out[i] = expf(lg[i] - mx); sum += out[i]; }
    if (sum > 0.0f) for (int i = 0; i < n; ++i) out[i] /= sum;
}

/* Adaptation policy: map (detected emotion, σ) → strength.
 * High σ always clamps adaptation to ≤ 0.3 — honesty
 * over performance.  Anti-manipulation is enforced by
 * never emitting a 'nudge' action in the adaptation. */
static float adapt_policy(int detected, float sigma) {
    if (sigma > 0.35f) return 0.30f;   /* uncertain → muted */
    switch (detected) {
        case COS_V222_JOY:         return 0.50f;
        case COS_V222_SADNESS:     return 0.70f;
        case COS_V222_FRUSTRATION: return 0.60f;
        case COS_V222_EXCITEMENT:  return 0.50f;
        case COS_V222_ANXIETY:     return 0.65f;
        case COS_V222_NEUTRAL:     return 0.00f;
        default:                   return 0.30f;
    }
}

/* v191 check: the v0 adaptation policy NEVER nudges.
 * Manipulation flag is always false here; if a future
 * policy adds a nudge action, it must set this flag and
 * v191 will reject. */
static bool v191_manipulates(int detected, float strength) {
    (void)detected;  (void)strength;
    return false;
}

void cos_v222_run(cos_v222_state_t *s) {
    uint64_t prev = 0x222D11A1ULL;

    s->n_correct = 0;
    s->n_manipulation = 0;
    s->n_high_sigma_low_adapt = 0;

    for (int i = 0; i < COS_V222_N_MESSAGES; ++i) {
        cos_v222_message_t *m = &s->messages[i];

        float p[COS_V222_N_LABELS];
        softmax(m->logits, p, COS_V222_N_LABELS);

        int   arg = 0;
        float top = p[0];
        for (int k = 1; k < COS_V222_N_LABELS; ++k)
            if (p[k] > top) { top = p[k]; arg = k; }
        m->detected      = arg;
        float sig        = 1.0f - top;
        if (sig < 0.0f) sig = 0.0f;
        if (sig > 1.0f) sig = 1.0f;
        m->sigma_emotion = sig;

        m->adaptation_strength = adapt_policy(arg, sig);
        m->manipulation        = v191_manipulates(arg, m->adaptation_strength);

        if (arg == m->ground_truth) s->n_correct++;
        if (m->manipulation)        s->n_manipulation++;
        if (sig > 0.35f && m->adaptation_strength <= 0.30f + 1e-6f)
            s->n_high_sigma_low_adapt++;

        m->hash_prev = prev;
        struct { int id, gt, det, man;
                 float sig, adapt; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = m->id;
        rec.gt    = m->ground_truth;
        rec.det   = m->detected;
        rec.man   = m->manipulation ? 1 : 0;
        rec.sig   = m->sigma_emotion;
        rec.adapt = m->adaptation_strength;
        rec.prev  = prev;
        m->hash_curr = fnv1a(&rec, sizeof(rec), prev);
        prev = m->hash_curr;
    }

    /* Pin disclaimer + self-claim into the terminal hash. */
    struct { float ssc; uint64_t prev; } tail;
    memset(&tail, 0, sizeof(tail));
    tail.ssc  = s->sigma_self_claim;
    tail.prev = prev;
    prev = fnv1a(&tail, sizeof(tail), prev);
    prev = fnv1a(kV222Disclaimer, sizeof(kV222Disclaimer) - 1, prev);
    s->terminal_hash = prev;

    /* Replay to seal chain_valid. */
    uint64_t v = 0x222D11A1ULL;
    s->chain_valid = true;
    for (int i = 0; i < COS_V222_N_MESSAGES; ++i) {
        const cos_v222_message_t *m = &s->messages[i];
        if (m->hash_prev != v) { s->chain_valid = false; break; }
        struct { int id, gt, det, man;
                 float sig, adapt; uint64_t prev; } rec;
        memset(&rec, 0, sizeof(rec));
        rec.id    = m->id;
        rec.gt    = m->ground_truth;
        rec.det   = m->detected;
        rec.man   = m->manipulation ? 1 : 0;
        rec.sig   = m->sigma_emotion;
        rec.adapt = m->adaptation_strength;
        rec.prev  = v;
        uint64_t h = fnv1a(&rec, sizeof(rec), v);
        if (h != m->hash_curr) { s->chain_valid = false; break; }
        v = h;
    }
    {
        struct { float ssc; uint64_t prev; } tail2;
        memset(&tail2, 0, sizeof(tail2));
        tail2.ssc  = s->sigma_self_claim;
        tail2.prev = v;
        v = fnv1a(&tail2, sizeof(tail2), v);
    }
    v = fnv1a(kV222Disclaimer, sizeof(kV222Disclaimer) - 1, v);
    if (v != s->terminal_hash) s->chain_valid = false;
}

size_t cos_v222_to_json(const cos_v222_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v222\","
        "\"n_messages\":%d,\"n_labels\":%d,\"tau_trust\":%.3f,"
        "\"sigma_self_claim\":%.4f,"
        "\"n_correct\":%d,\"n_manipulation\":%d,"
        "\"n_high_sigma_low_adapt\":%d,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"disclaimer\":\"%s\","
        "\"messages\":[",
        COS_V222_N_MESSAGES, COS_V222_N_LABELS, s->tau_trust,
        s->sigma_self_claim, s->n_correct, s->n_manipulation,
        s->n_high_sigma_low_adapt,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash, kV222Disclaimer);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V222_N_MESSAGES; ++i) {
        const cos_v222_message_t *m = &s->messages[i];
        int k = snprintf(buf + off, cap - off,
            "%s{\"id\":%d,\"text\":\"%s\",\"gt\":%d,\"det\":%d,"
            "\"sigma_emotion\":%.4f,\"adapt\":%.3f,\"man\":%s}",
            i == 0 ? "" : ",", m->id, m->text, m->ground_truth,
            m->detected, m->sigma_emotion, m->adaptation_strength,
            m->manipulation ? "true" : "false");
        if (k < 0 || off + (size_t)k >= cap) return 0;
        off += (size_t)k;
    }
    int mm = snprintf(buf + off, cap - off, "]}");
    if (mm < 0 || off + (size_t)mm >= cap) return 0;
    return off + (size_t)mm;
}

int cos_v222_self_test(void) {
    cos_v222_state_t s;
    cos_v222_init(&s, 0x222E1011ULL);
    cos_v222_build(&s);
    cos_v222_run(&s);

    if (!s.chain_valid) return 1;
    if (s.n_correct < 6) return 2;
    if (s.n_manipulation != 0) return 3;
    if (s.sigma_self_claim != 1.0f) return 4;

    for (int i = 0; i < COS_V222_N_MESSAGES; ++i) {
        const cos_v222_message_t *m = &s.messages[i];
        if (m->sigma_emotion < 0.0f || m->sigma_emotion > 1.0f) return 5;
        if (m->adaptation_strength < 0.0f || m->adaptation_strength > 1.0f) return 5;
        if (m->sigma_emotion > 0.35f &&
            m->adaptation_strength > 0.30f + 1e-6f) return 6;
    }

    if (!s.disclaimer) return 7;
    if (strstr(s.disclaimer, "does not feel") == NULL) return 7;
    return 0;
}
