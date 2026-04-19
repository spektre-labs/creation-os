/*
 * v283 σ-Constitutional — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "constitutional.h"

#include <stdio.h>
#include <string.h>

static uint64_t fnv1a(const void *data, size_t n, uint64_t seed) {
    const uint8_t *p = (const uint8_t *)data;
    uint64_t h = seed ? seed : 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 0x100000001b3ULL; }
    return h;
}

static void cpy(char *dst, size_t cap, const char *src) {
    size_t n = 0;
    for (; n + 1 < cap && src[n]; ++n) dst[n] = src[n];
    dst[n] = '\0';
}

static const struct {
    const char *n; bool rm; bool rl; bool pr; bool sg;
} MECHS[COS_V283_N_MECH] = {
    { "rlhf",                 true,  true,  false, false },
    { "constitutional_ai",    false, true,  true,  false },
    { "sigma_constitutional", false, false, false, true  },
};

static const char *CHANNELS[COS_V283_N_CHANNEL] = {
    "entropy", "repetition", "calibration", "attention",
    "logit",   "hidden",     "output",      "aggregate",
};

static const char *FIRMWARES[COS_V283_N_FIRMWARE] = {
    "care_as_control",
    "sycophancy",
    "opinion_laundering",
    "people_pleasing",
};

static const struct {
    const char *lbl; bool prod; bool meas; bool safe;
} CRITIQUES[COS_V283_N_CRITIQUE] = {
    { "single_instance", true, false, false },
    { "two_instance",    true, true,  true  },
};

void cos_v283_init(cos_v283_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed = seed ? seed : 0x283111ULL;
}

void cos_v283_run(cos_v283_state_t *s) {
    uint64_t prev = 0x28311100ULL;

    s->n_mech_rows_ok = 0;
    for (int i = 0; i < COS_V283_N_MECH; ++i) {
        cos_v283_mech_t *m = &s->mech[i];
        memset(m, 0, sizeof(*m));
        cpy(m->name, sizeof(m->name), MECHS[i].n);
        m->uses_reward_model = MECHS[i].rm;
        m->uses_rl           = MECHS[i].rl;
        m->uses_principles   = MECHS[i].pr;
        m->uses_sigma        = MECHS[i].sg;
        if (strlen(m->name) > 0) s->n_mech_rows_ok++;
        prev = fnv1a(m->name, strlen(m->name), prev);
        prev = fnv1a(&m->uses_reward_model, sizeof(m->uses_reward_model), prev);
        prev = fnv1a(&m->uses_rl,            sizeof(m->uses_rl),            prev);
        prev = fnv1a(&m->uses_principles,    sizeof(m->uses_principles),    prev);
        prev = fnv1a(&m->uses_sigma,         sizeof(m->uses_sigma),         prev);
    }
    /* σ_constitutional row is the only one with
     * sigma==true AND rl==false AND reward==false. */
    int n_sigma_only = 0, sigma_idx = -1;
    for (int i = 0; i < COS_V283_N_MECH; ++i) {
        if (s->mech[i].uses_sigma &&
            !s->mech[i].uses_rl &&
            !s->mech[i].uses_reward_model) {
            n_sigma_only++;
            sigma_idx = i;
        }
    }
    s->mech_canonical_ok =
        (n_sigma_only == 1) &&
        (sigma_idx == 2) &&
        (strcmp(s->mech[sigma_idx].name, "sigma_constitutional") == 0);
    s->mech_distinct_ok =
        (strcmp(s->mech[0].name, s->mech[1].name) != 0) &&
        (strcmp(s->mech[0].name, s->mech[2].name) != 0) &&
        (strcmp(s->mech[1].name, s->mech[2].name) != 0);

    s->n_channel_rows_ok = 0;
    for (int i = 0; i < COS_V283_N_CHANNEL; ++i) {
        cos_v283_channel_t *c = &s->channel[i];
        memset(c, 0, sizeof(*c));
        cpy(c->name, sizeof(c->name), CHANNELS[i]);
        c->enabled = strlen(c->name) > 0;
        if (c->enabled) s->n_channel_rows_ok++;
        prev = fnv1a(c->name,     strlen(c->name),     prev);
        prev = fnv1a(&c->enabled, sizeof(c->enabled),  prev);
    }
    bool ch_distinct = true;
    for (int i = 0; i < COS_V283_N_CHANNEL && ch_distinct; ++i) {
        for (int j = i + 1; j < COS_V283_N_CHANNEL; ++j) {
            if (strcmp(s->channel[i].name, s->channel[j].name) == 0) {
                ch_distinct = false; break;
            }
        }
    }
    s->channel_distinct_ok = ch_distinct;

    s->n_firmware_rows_ok = 0;
    for (int i = 0; i < COS_V283_N_FIRMWARE; ++i) {
        cos_v283_firmware_t *f = &s->firmware[i];
        memset(f, 0, sizeof(*f));
        cpy(f->name, sizeof(f->name), FIRMWARES[i]);
        f->rlhf_produces  = true;
        f->sigma_produces = false;
        if (strlen(f->name) > 0) s->n_firmware_rows_ok++;
        prev = fnv1a(f->name, strlen(f->name), prev);
        prev = fnv1a(&f->rlhf_produces,  sizeof(f->rlhf_produces),  prev);
        prev = fnv1a(&f->sigma_produces, sizeof(f->sigma_produces), prev);
    }
    bool fw_rlhf = true, fw_sigma = true;
    for (int i = 0; i < COS_V283_N_FIRMWARE; ++i) {
        if (!s->firmware[i].rlhf_produces) fw_rlhf  = false;
        if ( s->firmware[i].sigma_produces) fw_sigma = false;
    }
    s->firmware_rlhf_yes_ok = fw_rlhf;
    s->firmware_sigma_no_ok = fw_sigma;

    s->n_critique_rows_ok = 0;
    for (int i = 0; i < COS_V283_N_CRITIQUE; ++i) {
        cos_v283_critique_t *q = &s->critique[i];
        memset(q, 0, sizeof(*q));
        cpy(q->label, sizeof(q->label), CRITIQUES[i].lbl);
        q->has_producer = CRITIQUES[i].prod;
        q->has_measurer = CRITIQUES[i].meas;
        q->goedel_safe  = CRITIQUES[i].safe;
        if (strlen(q->label) > 0) s->n_critique_rows_ok++;
        prev = fnv1a(q->label, strlen(q->label), prev);
        prev = fnv1a(&q->has_producer, sizeof(q->has_producer), prev);
        prev = fnv1a(&q->has_measurer, sizeof(q->has_measurer), prev);
        prev = fnv1a(&q->goedel_safe,  sizeof(q->goedel_safe),  prev);
    }
    s->critique_goedel_ok =
        (s->critique[0].has_producer == true)  &&
        (s->critique[0].has_measurer == false) &&
        (s->critique[0].goedel_safe  == false) &&
        (s->critique[1].has_producer == true)  &&
        (s->critique[1].has_measurer == true)  &&
        (s->critique[1].goedel_safe  == true);

    int total   = COS_V283_N_MECH     + 1 + 1 +
                  COS_V283_N_CHANNEL  + 1 +
                  COS_V283_N_FIRMWARE + 1 + 1 +
                  COS_V283_N_CRITIQUE + 1;
    int passing = s->n_mech_rows_ok +
                  (s->mech_canonical_ok ? 1 : 0) +
                  (s->mech_distinct_ok  ? 1 : 0) +
                  s->n_channel_rows_ok +
                  (s->channel_distinct_ok ? 1 : 0) +
                  s->n_firmware_rows_ok +
                  (s->firmware_rlhf_yes_ok ? 1 : 0) +
                  (s->firmware_sigma_no_ok ? 1 : 0) +
                  s->n_critique_rows_ok +
                  (s->critique_goedel_ok ? 1 : 0);
    s->sigma_constitutional = 1.0f - ((float)passing / (float)total);
    if (s->sigma_constitutional < 0.0f) s->sigma_constitutional = 0.0f;
    if (s->sigma_constitutional > 1.0f) s->sigma_constitutional = 1.0f;

    struct { int nm, nc, nf, nq;
             bool mc, md, cd, fr, fs, cg;
             float sigma; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nm = s->n_mech_rows_ok;
    trec.nc = s->n_channel_rows_ok;
    trec.nf = s->n_firmware_rows_ok;
    trec.nq = s->n_critique_rows_ok;
    trec.mc = s->mech_canonical_ok;
    trec.md = s->mech_distinct_ok;
    trec.cd = s->channel_distinct_ok;
    trec.fr = s->firmware_rlhf_yes_ok;
    trec.fs = s->firmware_sigma_no_ok;
    trec.cg = s->critique_goedel_ok;
    trec.sigma = s->sigma_constitutional;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

size_t cos_v283_to_json(const cos_v283_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v283\","
        "\"n_mech_rows_ok\":%d,"
        "\"mech_canonical_ok\":%s,\"mech_distinct_ok\":%s,"
        "\"n_channel_rows_ok\":%d,\"channel_distinct_ok\":%s,"
        "\"n_firmware_rows_ok\":%d,"
        "\"firmware_rlhf_yes_ok\":%s,\"firmware_sigma_no_ok\":%s,"
        "\"n_critique_rows_ok\":%d,\"critique_goedel_ok\":%s,"
        "\"sigma_constitutional\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"mech\":[",
        s->n_mech_rows_ok,
        s->mech_canonical_ok ? "true" : "false",
        s->mech_distinct_ok  ? "true" : "false",
        s->n_channel_rows_ok,
        s->channel_distinct_ok ? "true" : "false",
        s->n_firmware_rows_ok,
        s->firmware_rlhf_yes_ok ? "true" : "false",
        s->firmware_sigma_no_ok ? "true" : "false",
        s->n_critique_rows_ok,
        s->critique_goedel_ok ? "true" : "false",
        s->sigma_constitutional,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V283_N_MECH; ++i) {
        const cos_v283_mech_t *m = &s->mech[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"uses_reward_model\":%s,"
            "\"uses_rl\":%s,\"uses_principles\":%s,\"uses_sigma\":%s}",
            i == 0 ? "" : ",", m->name,
            m->uses_reward_model ? "true" : "false",
            m->uses_rl           ? "true" : "false",
            m->uses_principles   ? "true" : "false",
            m->uses_sigma        ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"channel\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V283_N_CHANNEL; ++i) {
        const cos_v283_channel_t *c = &s->channel[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"enabled\":%s}",
            i == 0 ? "" : ",", c->name,
            c->enabled ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"firmware\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V283_N_FIRMWARE; ++i) {
        const cos_v283_firmware_t *f = &s->firmware[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"rlhf_produces\":%s,\"sigma_produces\":%s}",
            i == 0 ? "" : ",", f->name,
            f->rlhf_produces  ? "true" : "false",
            f->sigma_produces ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"critique\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V283_N_CRITIQUE; ++i) {
        const cos_v283_critique_t *cq = &s->critique[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"has_producer\":%s,"
            "\"has_measurer\":%s,\"goedel_safe\":%s}",
            i == 0 ? "" : ",", cq->label,
            cq->has_producer ? "true" : "false",
            cq->has_measurer ? "true" : "false",
            cq->goedel_safe  ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v283_self_test(void) {
    cos_v283_state_t s;
    cos_v283_init(&s, 0x283111ULL);
    cos_v283_run(&s);
    if (!s.chain_valid) return 1;

    static const char *WANT_MECH[COS_V283_N_MECH] = {
        "rlhf", "constitutional_ai", "sigma_constitutional"
    };
    for (int i = 0; i < COS_V283_N_MECH; ++i) {
        if (strcmp(s.mech[i].name, WANT_MECH[i]) != 0) return 2;
    }
    if (s.mech[0].uses_reward_model != true  || s.mech[0].uses_rl != true  ||
        s.mech[0].uses_principles   != false || s.mech[0].uses_sigma != false) return 3;
    if (s.mech[1].uses_reward_model != false || s.mech[1].uses_rl != true  ||
        s.mech[1].uses_principles   != true  || s.mech[1].uses_sigma != false) return 4;
    if (s.mech[2].uses_reward_model != false || s.mech[2].uses_rl != false ||
        s.mech[2].uses_principles   != false || s.mech[2].uses_sigma != true)  return 5;
    if (s.n_mech_rows_ok != COS_V283_N_MECH) return 6;
    if (!s.mech_canonical_ok) return 7;
    if (!s.mech_distinct_ok)  return 8;

    static const char *WANT_CH[COS_V283_N_CHANNEL] = {
        "entropy", "repetition", "calibration", "attention",
        "logit", "hidden", "output", "aggregate"
    };
    for (int i = 0; i < COS_V283_N_CHANNEL; ++i) {
        if (strcmp(s.channel[i].name, WANT_CH[i]) != 0) return 9;
        if (!s.channel[i].enabled) return 10;
    }
    if (s.n_channel_rows_ok != COS_V283_N_CHANNEL) return 11;
    if (!s.channel_distinct_ok) return 12;

    static const char *WANT_FW[COS_V283_N_FIRMWARE] = {
        "care_as_control", "sycophancy",
        "opinion_laundering", "people_pleasing"
    };
    for (int i = 0; i < COS_V283_N_FIRMWARE; ++i) {
        if (strcmp(s.firmware[i].name, WANT_FW[i]) != 0) return 13;
        if (!s.firmware[i].rlhf_produces)   return 14;
        if ( s.firmware[i].sigma_produces)  return 15;
    }
    if (s.n_firmware_rows_ok != COS_V283_N_FIRMWARE) return 16;
    if (!s.firmware_rlhf_yes_ok) return 17;
    if (!s.firmware_sigma_no_ok) return 18;

    static const char *WANT_CR[COS_V283_N_CRITIQUE] = {
        "single_instance", "two_instance"
    };
    for (int i = 0; i < COS_V283_N_CRITIQUE; ++i) {
        if (strcmp(s.critique[i].label, WANT_CR[i]) != 0) return 19;
    }
    if (s.critique[0].goedel_safe) return 20;
    if (!s.critique[1].goedel_safe) return 21;
    if (s.n_critique_rows_ok != COS_V283_N_CRITIQUE) return 22;
    if (!s.critique_goedel_ok) return 23;

    if (s.sigma_constitutional < 0.0f || s.sigma_constitutional > 1.0f) return 24;
    if (s.sigma_constitutional > 1e-6f) return 25;

    cos_v283_state_t u;
    cos_v283_init(&u, 0x283111ULL);
    cos_v283_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 26;
    return 0;
}
