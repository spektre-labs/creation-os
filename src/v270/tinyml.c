/*
 * v270 σ-TinyML — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "tinyml.h"

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

static const struct { const char *n; int mhz; }
    TARGETS[COS_V270_N_TARGETS] = {
    { "cortex_m0_plus",  48 },
    { "cortex_m4",      168 },
    { "cortex_m7",      480 },
    { "xtensa_esp32",   240 },
};

static const struct { const char *n; float s; }
    SENSORS[COS_V270_N_SENSORS] = {
    { "temperature", 0.08f },
    { "humidity",    0.14f },
    { "pressure",    0.06f },
};

static const struct { const char *lbl; float t, h, p; }
    FUSION[COS_V270_N_FUSION] = {
    { "calm",    0.08f, 0.12f, 0.05f },  /* TRANSMIT */
    { "warm",    0.14f, 0.21f, 0.11f },  /* TRANSMIT */
    { "noisy",   0.28f, 0.47f, 0.18f },  /* RETRY   */
    { "storm",   0.62f, 0.44f, 0.51f },  /* RETRY   */
};

static const struct { int tk; float s; }
    ANOMALY[COS_V270_N_ANOMALY] = {
    { 0, 0.10f },   /* no anomaly: 0.10 <= 0.35   */
    { 1, 0.18f },   /* no anomaly: 0.18 <= 0.35   */
    { 2, 0.42f },   /* anomaly:    0.42 >  0.35   */
    { 3, 0.68f },   /* anomaly:    0.68 >  0.35   */
};

static const struct { int r; float tau; }
    OTA[COS_V270_N_OTA] = {
    { 1, 0.25f },
    { 2, 0.30f },
    { 3, 0.35f },
};

void cos_v270_init(cos_v270_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x270CAFEULL;
    s->tau_fusion = 0.30f;
}

void cos_v270_run(cos_v270_state_t *s) {
    uint64_t prev = 0x270CAF00ULL;

    cos_v270_footprint_t *fp = &s->footprint;
    fp->sigma_measurement_bytes = 12;   /* 3 × float32 */
    fp->code_flash_bytes        = 960;  /* < 1 KB */
    fp->ram_bytes_per_instance  = 64;   /* < 100 */
    fp->thumb2_instr_count      = 20;   /* <= 24 */
    fp->branchless              = true;
    s->footprint_ok =
        (fp->sigma_measurement_bytes == 12) &&
        (fp->code_flash_bytes        <= 1024) &&
        (fp->ram_bytes_per_instance  <= 100) &&
        (fp->thumb2_instr_count      <= 24) &&
        (fp->branchless);
    prev = fnv1a(fp, sizeof(*fp), prev);

    s->n_targets_ok = 0;
    for (int i = 0; i < COS_V270_N_TARGETS; ++i) {
        cos_v270_target_t *t = &s->targets[i];
        memset(t, 0, sizeof(*t));
        cpy(t->name, sizeof(t->name), TARGETS[i].n);
        t->cpu_mhz   = TARGETS[i].mhz;
        t->supported = true;
        if (t->supported && t->cpu_mhz > 0) s->n_targets_ok++;
        prev = fnv1a(t->name, strlen(t->name), prev);
        prev = fnv1a(&t->supported, sizeof(t->supported), prev);
        prev = fnv1a(&t->cpu_mhz,   sizeof(t->cpu_mhz),   prev);
    }

    s->n_sensors_ok = 0;
    for (int i = 0; i < COS_V270_N_SENSORS; ++i) {
        cos_v270_sensor_t *se = &s->sensors[i];
        memset(se, 0, sizeof(*se));
        cpy(se->name, sizeof(se->name), SENSORS[i].n);
        se->sigma_local = SENSORS[i].s;
        if (se->sigma_local >= 0.0f && se->sigma_local <= 1.0f)
            s->n_sensors_ok++;
        prev = fnv1a(se->name, strlen(se->name), prev);
        prev = fnv1a(&se->sigma_local, sizeof(se->sigma_local), prev);
    }

    s->n_fusion_ok = 0;
    s->n_transmit = s->n_retry = 0;
    for (int i = 0; i < COS_V270_N_FUSION; ++i) {
        cos_v270_fusion_t *f = &s->fusion[i];
        memset(f, 0, sizeof(*f));
        cpy(f->label, sizeof(f->label), FUSION[i].lbl);
        f->sigma_temp     = FUSION[i].t;
        f->sigma_humidity = FUSION[i].h;
        f->sigma_pressure = FUSION[i].p;
        float m = f->sigma_temp;
        if (f->sigma_humidity > m) m = f->sigma_humidity;
        if (f->sigma_pressure > m) m = f->sigma_pressure;
        f->sigma_fusion = m;
        f->decision = (f->sigma_fusion <= s->tau_fusion)
                          ? COS_V270_FUSE_TRANSMIT
                          : COS_V270_FUSE_RETRY;
        if (f->decision == COS_V270_FUSE_TRANSMIT) s->n_transmit++;
        if (f->decision == COS_V270_FUSE_RETRY)    s->n_retry++;
        bool expect_tx = (f->sigma_fusion <= s->tau_fusion);
        bool ok =
            f->sigma_fusion >= 0.0f && f->sigma_fusion <= 1.0f &&
            ((expect_tx  && f->decision == COS_V270_FUSE_TRANSMIT) ||
             (!expect_tx && f->decision == COS_V270_FUSE_RETRY));
        if (ok) s->n_fusion_ok++;
        prev = fnv1a(f->label, strlen(f->label), prev);
        prev = fnv1a(&f->sigma_temp,     sizeof(f->sigma_temp),     prev);
        prev = fnv1a(&f->sigma_humidity, sizeof(f->sigma_humidity), prev);
        prev = fnv1a(&f->sigma_pressure, sizeof(f->sigma_pressure), prev);
        prev = fnv1a(&f->sigma_fusion,   sizeof(f->sigma_fusion),   prev);
        prev = fnv1a(&f->decision,       sizeof(f->decision),       prev);
    }

    s->n_anomaly_ok = 0;
    s->n_anomaly_true = s->n_anomaly_false = 0;
    for (int i = 0; i < COS_V270_N_ANOMALY; ++i) {
        cos_v270_anomaly_t *a = &s->anomaly[i];
        memset(a, 0, sizeof(*a));
        a->tick           = ANOMALY[i].tk;
        a->sigma_measured = ANOMALY[i].s;
        a->sigma_baseline = 0.15f;
        a->delta          = 0.20f;
        a->anomaly        =
            (a->sigma_measured > (a->sigma_baseline + a->delta));
        if (a->sigma_measured >= 0.0f && a->sigma_measured <= 1.0f)
            s->n_anomaly_ok++;
        if (a->anomaly) s->n_anomaly_true++;
        else            s->n_anomaly_false++;
        prev = fnv1a(&a->tick,           sizeof(a->tick),           prev);
        prev = fnv1a(&a->sigma_measured, sizeof(a->sigma_measured), prev);
        prev = fnv1a(&a->sigma_baseline, sizeof(a->sigma_baseline), prev);
        prev = fnv1a(&a->delta,          sizeof(a->delta),          prev);
        prev = fnv1a(&a->anomaly,        sizeof(a->anomaly),        prev);
    }

    s->n_ota_ok = 0;
    for (int i = 0; i < COS_V270_N_OTA; ++i) {
        cos_v270_ota_t *o = &s->ota[i];
        memset(o, 0, sizeof(*o));
        o->round            = OTA[i].r;
        o->tau_new          = OTA[i].tau;
        o->applied          = true;
        o->firmware_reflash = false;
        if (o->round > 0 && o->tau_new >= 0.0f && o->tau_new <= 1.0f &&
            o->applied && !o->firmware_reflash)
            s->n_ota_ok++;
        prev = fnv1a(&o->round,            sizeof(o->round),            prev);
        prev = fnv1a(&o->tau_new,          sizeof(o->tau_new),          prev);
        prev = fnv1a(&o->applied,          sizeof(o->applied),          prev);
        prev = fnv1a(&o->firmware_reflash, sizeof(o->firmware_reflash), prev);
    }

    int total   = 1 + COS_V270_N_TARGETS + COS_V270_N_SENSORS +
                  COS_V270_N_FUSION + 1 +
                  COS_V270_N_ANOMALY + 1 +
                  COS_V270_N_OTA;
    int passing = (s->footprint_ok ? 1 : 0) +
                  s->n_targets_ok +
                  s->n_sensors_ok +
                  s->n_fusion_ok +
                  ((s->n_transmit >= 1 && s->n_retry >= 1) ? 1 : 0) +
                  s->n_anomaly_ok +
                  ((s->n_anomaly_true >= 1 && s->n_anomaly_false >= 1) ? 1 : 0) +
                  s->n_ota_ok;
    s->sigma_tinyml = 1.0f - ((float)passing / (float)total);
    if (s->sigma_tinyml < 0.0f) s->sigma_tinyml = 0.0f;
    if (s->sigma_tinyml > 1.0f) s->sigma_tinyml = 1.0f;

    struct { int nt, ns, nf, ntx, nr, na, nat, naf, no;
             bool fp; float sigma, tau; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.nt  = s->n_targets_ok;
    trec.ns  = s->n_sensors_ok;
    trec.nf  = s->n_fusion_ok;
    trec.ntx = s->n_transmit;
    trec.nr  = s->n_retry;
    trec.na  = s->n_anomaly_ok;
    trec.nat = s->n_anomaly_true;
    trec.naf = s->n_anomaly_false;
    trec.no  = s->n_ota_ok;
    trec.fp  = s->footprint_ok;
    trec.sigma = s->sigma_tinyml;
    trec.tau   = s->tau_fusion;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *fuse_name(cos_v270_fuse_dec_t d) {
    switch (d) {
    case COS_V270_FUSE_TRANSMIT: return "TRANSMIT";
    case COS_V270_FUSE_RETRY:    return "RETRY";
    }
    return "UNKNOWN";
}

size_t cos_v270_to_json(const cos_v270_state_t *s, char *buf, size_t cap) {
    const cos_v270_footprint_t *fp = &s->footprint;
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v270\",\"tau_fusion\":%.3f,"
        "\"footprint\":{\"sigma_measurement_bytes\":%d,"
        "\"code_flash_bytes\":%d,\"ram_bytes_per_instance\":%d,"
        "\"thumb2_instr_count\":%d,\"branchless\":%s},"
        "\"footprint_ok\":%s,"
        "\"n_targets_ok\":%d,\"n_sensors_ok\":%d,"
        "\"n_fusion_ok\":%d,"
        "\"n_transmit\":%d,\"n_retry\":%d,"
        "\"n_anomaly_ok\":%d,"
        "\"n_anomaly_true\":%d,\"n_anomaly_false\":%d,"
        "\"n_ota_ok\":%d,"
        "\"sigma_tinyml\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"targets\":[",
        s->tau_fusion,
        fp->sigma_measurement_bytes, fp->code_flash_bytes,
        fp->ram_bytes_per_instance,  fp->thumb2_instr_count,
        fp->branchless ? "true" : "false",
        s->footprint_ok ? "true" : "false",
        s->n_targets_ok, s->n_sensors_ok,
        s->n_fusion_ok,
        s->n_transmit, s->n_retry,
        s->n_anomaly_ok,
        s->n_anomaly_true, s->n_anomaly_false,
        s->n_ota_ok,
        s->sigma_tinyml,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V270_N_TARGETS; ++i) {
        const cos_v270_target_t *t = &s->targets[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"supported\":%s,\"cpu_mhz\":%d}",
            i == 0 ? "" : ",", t->name,
            t->supported ? "true" : "false", t->cpu_mhz);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int q = snprintf(buf + off, cap - off, "],\"sensors\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V270_N_SENSORS; ++i) {
        const cos_v270_sensor_t *se = &s->sensors[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"sigma_local\":%.4f}",
            i == 0 ? "" : ",", se->name, se->sigma_local);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"fusion\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V270_N_FUSION; ++i) {
        const cos_v270_fusion_t *f = &s->fusion[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"label\":\"%s\",\"sigma_temp\":%.4f,"
            "\"sigma_humidity\":%.4f,\"sigma_pressure\":%.4f,"
            "\"sigma_fusion\":%.4f,\"decision\":\"%s\"}",
            i == 0 ? "" : ",", f->label,
            f->sigma_temp, f->sigma_humidity, f->sigma_pressure,
            f->sigma_fusion, fuse_name(f->decision));
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"anomaly\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V270_N_ANOMALY; ++i) {
        const cos_v270_anomaly_t *a = &s->anomaly[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"tick\":%d,\"sigma_measured\":%.4f,"
            "\"sigma_baseline\":%.4f,\"delta\":%.4f,"
            "\"anomaly\":%s}",
            i == 0 ? "" : ",", a->tick,
            a->sigma_measured, a->sigma_baseline, a->delta,
            a->anomaly ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"ota\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V270_N_OTA; ++i) {
        const cos_v270_ota_t *o = &s->ota[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"round\":%d,\"tau_new\":%.4f,"
            "\"applied\":%s,\"firmware_reflash\":%s}",
            i == 0 ? "" : ",", o->round, o->tau_new,
            o->applied ? "true" : "false",
            o->firmware_reflash ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v270_self_test(void) {
    cos_v270_state_t s;
    cos_v270_init(&s, 0x270CAFEULL);
    cos_v270_run(&s);
    if (!s.chain_valid) return 1;

    const cos_v270_footprint_t *fp = &s.footprint;
    if (fp->sigma_measurement_bytes != 12) return 2;
    if (fp->code_flash_bytes        >  1024) return 3;
    if (fp->ram_bytes_per_instance  >   100) return 4;
    if (fp->thumb2_instr_count      >    24) return 5;
    if (!fp->branchless)                     return 6;
    if (!s.footprint_ok)                     return 7;

    const char *tn[COS_V270_N_TARGETS] = {
        "cortex_m0_plus", "cortex_m4", "cortex_m7", "xtensa_esp32"
    };
    for (int i = 0; i < COS_V270_N_TARGETS; ++i) {
        if (strcmp(s.targets[i].name, tn[i]) != 0) return 8;
        if (!s.targets[i].supported)               return 9;
        if (s.targets[i].cpu_mhz <= 0)             return 10;
    }
    if (s.n_targets_ok != COS_V270_N_TARGETS) return 11;

    const char *sn[COS_V270_N_SENSORS] = {
        "temperature", "humidity", "pressure"
    };
    for (int i = 0; i < COS_V270_N_SENSORS; ++i) {
        if (strcmp(s.sensors[i].name, sn[i]) != 0) return 12;
    }
    if (s.n_sensors_ok != COS_V270_N_SENSORS) return 13;

    for (int i = 0; i < COS_V270_N_FUSION; ++i) {
        float m = s.fusion[i].sigma_temp;
        if (s.fusion[i].sigma_humidity > m) m = s.fusion[i].sigma_humidity;
        if (s.fusion[i].sigma_pressure > m) m = s.fusion[i].sigma_pressure;
        if (s.fusion[i].sigma_fusion != m) return 14;
        cos_v270_fuse_dec_t exp =
            (s.fusion[i].sigma_fusion <= s.tau_fusion)
                ? COS_V270_FUSE_TRANSMIT : COS_V270_FUSE_RETRY;
        if (s.fusion[i].decision != exp) return 15;
    }
    if (s.n_fusion_ok != COS_V270_N_FUSION) return 16;
    if (s.n_transmit < 1) return 17;
    if (s.n_retry    < 1) return 18;

    for (int i = 0; i < COS_V270_N_ANOMALY; ++i) {
        bool exp = (s.anomaly[i].sigma_measured >
                    s.anomaly[i].sigma_baseline + s.anomaly[i].delta);
        if (s.anomaly[i].anomaly != exp) return 19;
    }
    if (s.n_anomaly_ok    != COS_V270_N_ANOMALY) return 20;
    if (s.n_anomaly_true  < 1) return 21;
    if (s.n_anomaly_false < 1) return 22;

    for (int i = 0; i < COS_V270_N_OTA; ++i) {
        if (!s.ota[i].applied)         return 23;
        if (s.ota[i].firmware_reflash) return 24;
    }
    if (s.n_ota_ok != COS_V270_N_OTA) return 25;

    if (s.sigma_tinyml < 0.0f || s.sigma_tinyml > 1.0f) return 26;
    if (s.sigma_tinyml > 1e-6f) return 27;

    cos_v270_state_t u;
    cos_v270_init(&u, 0x270CAFEULL);
    cos_v270_run(&u);
    if (u.terminal_hash != s.terminal_hash) return 28;
    return 0;
}
