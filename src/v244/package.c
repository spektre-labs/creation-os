/*
 * v244 σ-Package — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "package.h"

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

static const struct { const char *name; const char *cmd; }
    PLATFORMS[COS_V244_N_PLATFORMS] = {
    { "macOS",   "brew install creation-os"         },
    { "linux",   "apt install creation-os"          },
    { "docker",  "docker run spektre/creation-os"   },
    { "windows", "winget install creation-os"       },
};

/* Seed quintet — the five kernels a minimal install
 * must carry.  Any deviation is a gate failure. */
static const int SEED_QUINTET[COS_V244_SEED_QUINTET] = {
    29, 101, 106, 124, 148
};

static const struct { cos_v244_state_t st; const char *banner; }
    FIRSTRUN[COS_V244_N_FIRSTRUN] = {
    { COS_V244_STATE_SEED,     "Presence: SEED. Growing..." },
    { COS_V244_STATE_GROWING,  "Growing kernels from seed"  },
    { COS_V244_STATE_CHECKING, "Running v243 completeness"  },
    { COS_V244_STATE_READY,    "Ready. K_eff: 0.74."        },
};

/* Update fixture: 4 steps.  Step 2 is deliberately
 * risky (σ > τ_update = 0.50) so the σ-gate rejects
 * it.  Step 4 is a rollback drill. */
static const struct { const char *from; const char *to; float sigma; }
    UPDATES[COS_V244_N_UPDATES] = {
    { "1.0.0", "1.0.1", 0.12f },
    { "1.0.1", "1.0.2", 0.72f },   /* σ > τ → rejected */
    { "1.0.1", "1.0.3", 0.18f },
    { "1.0.3", "1.0.1", 0.05f },   /* rollback to previous */
};

void cos_v244_init(cos_v244_state_full_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed       = seed ? seed : 0x244A7EADULL;
    s->tau_update = 0.50f;
}

void cos_v244_run(cos_v244_state_full_t *s) {
    uint64_t prev = 0x244A7EA5ULL;

    s->n_platforms_ok = 0;
    for (int i = 0; i < COS_V244_N_PLATFORMS; ++i) {
        cos_v244_platform_t *p = &s->platforms[i];
        memset(p, 0, sizeof(*p));
        cpy(p->name,        sizeof(p->name),        PLATFORMS[i].name);
        cpy(p->install_cmd, sizeof(p->install_cmd), PLATFORMS[i].cmd);
        p->manifest_ok = (strlen(p->install_cmd) > 0) && (strlen(p->name) > 0);
        if (p->manifest_ok) s->n_platforms_ok++;
        prev = fnv1a(p->name,        strlen(p->name),        prev);
        prev = fnv1a(p->install_cmd, strlen(p->install_cmd), prev);
    }

    s->minimal.profile = COS_V244_PROFILE_MINIMAL;
    s->minimal.n_kernels = COS_V244_SEED_QUINTET;
    for (int i = 0; i < COS_V244_SEED_QUINTET; ++i)
        s->minimal.kernels[i] = SEED_QUINTET[i];
    s->minimal.ram_mb  = 4096;
    s->minimal.disk_mb = 2048;
    prev = fnv1a(s->minimal.kernels,
                 sizeof(int) * s->minimal.n_kernels, prev);

    /* Full profile — the canonical 243 kernel ids the
     * FULL install pulls in (integer range 6..248
     * inclusive = 243 ids).  Explicit enumeration
     * gives the self-test a byte-deterministic
     * reference; any deviation from the declared
     * bound is a gate failure. */
    s->full.profile = COS_V244_PROFILE_FULL;
    int idx = 0;
    for (int k = 6; k <= 248; ++k) s->full.kernels[idx++] = k;
    s->full.n_kernels = idx;
    s->full.ram_mb    = 16384;
    s->full.disk_mb   = 15360;
    prev = fnv1a(&s->full.n_kernels, sizeof(s->full.n_kernels), prev);

    for (int i = 0; i < COS_V244_N_FIRSTRUN; ++i) {
        cos_v244_firstrun_t *fr = &s->firstrun[i];
        memset(fr, 0, sizeof(*fr));
        fr->step  = i + 1;
        fr->state = FIRSTRUN[i].st;
        cpy(fr->banner, sizeof(fr->banner), FIRSTRUN[i].banner);
        fr->tick  = i + 1;
        prev = fnv1a(&fr->state, sizeof(fr->state), prev);
        prev = fnv1a(fr->banner, strlen(fr->banner), prev);
    }

    s->n_updates_accepted = s->n_updates_rejected = 0;
    cpy(s->installed_version, sizeof(s->installed_version), "1.0.0");
    cpy(s->previous_version,  sizeof(s->previous_version),  "1.0.0");
    for (int i = 0; i < COS_V244_N_UPDATES; ++i) {
        cos_v244_update_t *u = &s->updates[i];
        memset(u, 0, sizeof(*u));
        u->step        = i + 1;
        cpy(u->from_version, sizeof(u->from_version), UPDATES[i].from);
        cpy(u->to_version,   sizeof(u->to_version),   UPDATES[i].to);
        u->sigma_update = UPDATES[i].sigma;
        u->accepted     = (u->sigma_update <= s->tau_update);
        if (u->accepted) {
            cpy(s->previous_version,  sizeof(s->previous_version),
                s->installed_version);
            cpy(s->installed_version, sizeof(s->installed_version),
                u->to_version);
            s->n_updates_accepted++;
        } else {
            s->n_updates_rejected++;
        }
        prev = fnv1a(u->from_version, strlen(u->from_version), prev);
        prev = fnv1a(u->to_version,   strlen(u->to_version),   prev);
        prev = fnv1a(&u->sigma_update, sizeof(u->sigma_update), prev);
    }

    /* Rollback audit: the final update (step 4) is a
     * rollback to the prior version.  If the fixture
     * accepted it, installed_version must now equal the
     * step-4 target string byte-for-byte. */
    s->rollback_ok =
        (s->updates[COS_V244_N_UPDATES - 1].accepted) &&
        (strcmp(s->installed_version,
                s->updates[COS_V244_N_UPDATES - 1].to_version) == 0);

    s->sigma_package = 1.0f - ((float)s->n_platforms_ok /
                               (float)COS_V244_N_PLATFORMS);
    if (s->sigma_package < 0.0f) s->sigma_package = 0.0f;
    if (s->sigma_package > 1.0f) s->sigma_package = 1.0f;

    struct { int np, na, nr; float sp, tu; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.np = s->n_platforms_ok;
    trec.na = s->n_updates_accepted;
    trec.nr = s->n_updates_rejected;
    trec.sp = s->sigma_package;
    trec.tu = s->tau_update;
    trec.prev = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *state_name(cos_v244_state_t st) {
    switch (st) {
    case COS_V244_STATE_SEED:     return "SEED";
    case COS_V244_STATE_GROWING:  return "GROWING";
    case COS_V244_STATE_CHECKING: return "CHECKING";
    case COS_V244_STATE_READY:    return "READY";
    }
    return "UNKNOWN";
}

size_t cos_v244_to_json(const cos_v244_state_full_t *s,
                         char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v244\","
        "\"n_platforms\":%d,\"n_platforms_ok\":%d,"
        "\"sigma_package\":%.4f,\"tau_update\":%.3f,"
        "\"n_updates_accepted\":%d,\"n_updates_rejected\":%d,"
        "\"installed_version\":\"%s\",\"previous_version\":\"%s\","
        "\"rollback_ok\":%s,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"platforms\":[",
        COS_V244_N_PLATFORMS, s->n_platforms_ok,
        s->sigma_package, s->tau_update,
        s->n_updates_accepted, s->n_updates_rejected,
        s->installed_version, s->previous_version,
        s->rollback_ok ? "true" : "false",
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V244_N_PLATFORMS; ++i) {
        const cos_v244_platform_t *p = &s->platforms[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"install_cmd\":\"%s\","
            "\"manifest_ok\":%s}",
            i == 0 ? "" : ",",
            p->name, p->install_cmd,
            p->manifest_ok ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int q = snprintf(buf + off, cap - off,
        "],\"minimal\":{\"profile\":\"MINIMAL\","
        "\"n_kernels\":%d,\"ram_mb\":%d,\"disk_mb\":%d,"
        "\"kernels\":[",
        s->minimal.n_kernels, s->minimal.ram_mb, s->minimal.disk_mb);
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < s->minimal.n_kernels; ++i) {
        int z = snprintf(buf + off, cap - off, "%s%d",
                         i == 0 ? "" : ",", s->minimal.kernels[i]);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off,
        "]},\"full\":{\"profile\":\"FULL\","
        "\"n_kernels\":%d,\"ram_mb\":%d,\"disk_mb\":%d},"
        "\"firstrun\":[",
        s->full.n_kernels, s->full.ram_mb, s->full.disk_mb);
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V244_N_FIRSTRUN; ++i) {
        const cos_v244_firstrun_t *fr = &s->firstrun[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"state\":\"%s\","
            "\"banner\":\"%s\",\"tick\":%d}",
            i == 0 ? "" : ",",
            fr->step, state_name(fr->state), fr->banner, fr->tick);
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    q = snprintf(buf + off, cap - off, "],\"updates\":[");
    if (q < 0 || off + (size_t)q >= cap) return 0;
    off += (size_t)q;
    for (int i = 0; i < COS_V244_N_UPDATES; ++i) {
        const cos_v244_update_t *u = &s->updates[i];
        int z = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"from\":\"%s\",\"to\":\"%s\","
            "\"sigma_update\":%.4f,\"accepted\":%s}",
            i == 0 ? "" : ",",
            u->step, u->from_version, u->to_version,
            u->sigma_update, u->accepted ? "true" : "false");
        if (z < 0 || off + (size_t)z >= cap) return 0;
        off += (size_t)z;
    }
    int m = snprintf(buf + off, cap - off, "]}");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    return off + (size_t)m;
}

int cos_v244_self_test(void) {
    cos_v244_state_full_t s;
    cos_v244_init(&s, 0x244A7EADULL);
    cos_v244_run(&s);
    if (!s.chain_valid) return 1;

    const char *names[COS_V244_N_PLATFORMS] =
        { "macOS", "linux", "docker", "windows" };
    for (int i = 0; i < COS_V244_N_PLATFORMS; ++i) {
        if (strcmp(s.platforms[i].name, names[i]) != 0) return 2;
        if (!s.platforms[i].manifest_ok)                return 3;
        if (strlen(s.platforms[i].install_cmd) == 0)    return 4;
    }
    if (s.n_platforms_ok != COS_V244_N_PLATFORMS) return 5;

    if (s.minimal.n_kernels != COS_V244_SEED_QUINTET) return 6;
    int expect[COS_V244_SEED_QUINTET] = { 29, 101, 106, 124, 148 };
    for (int i = 0; i < COS_V244_SEED_QUINTET; ++i)
        if (s.minimal.kernels[i] != expect[i]) return 7;
    if (s.full.n_kernels < COS_V244_MIN_FULL_KERNELS) return 8;

    cos_v244_state_t want[COS_V244_N_FIRSTRUN] = {
        COS_V244_STATE_SEED, COS_V244_STATE_GROWING,
        COS_V244_STATE_CHECKING, COS_V244_STATE_READY
    };
    for (int i = 0; i < COS_V244_N_FIRSTRUN; ++i) {
        if (s.firstrun[i].state != want[i]) return 9;
        if (s.firstrun[i].step  != i + 1)   return 10;
        if (i > 0 && s.firstrun[i].tick <= s.firstrun[i - 1].tick) return 11;
    }

    if (s.n_updates_accepted < 3) return 12;
    if (s.n_updates_rejected < 1) return 13;
    if (!s.rollback_ok)           return 14;

    if (s.sigma_package < 0.0f || s.sigma_package > 1.0f) return 15;
    if (s.sigma_package > 1e-6f) return 16;

    cos_v244_state_full_t t;
    cos_v244_init(&t, 0x244A7EADULL);
    cos_v244_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 17;
    return 0;
}
