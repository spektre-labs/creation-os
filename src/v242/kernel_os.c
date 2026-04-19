/*
 * v242 σ-Kernel-OS — reference implementation.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "kernel_os.h"

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

typedef struct { int kid; const char *name; float sigma; } cos_v242_fxp_t;

static const cos_v242_fxp_t PROCS[COS_V242_N_PROCESSES] = {
    {  29, "v29_sigma_core",        0.08f },
    { 101, "v101_bridge",           0.10f },
    { 106, "v106_server",           0.12f },
    { 111, "v111_reason",           0.22f },
    { 115, "v115_memory",           0.15f },
    { 135, "v135_symbolic",         0.20f },
    { 147, "v147_reflect",          0.28f },
    { 150, "v150_debate",           0.32f },
    { 162, "v162_compose",          0.18f },
    { 189, "v189_ttc",              0.24f },
    { 234, "v234_presence",         0.14f },
    { 239, "v239_runtime",          0.16f },
};

typedef struct { const char *name; const char *backing; } cos_v242_fxipc_t;

static const cos_v242_fxipc_t IPCS[COS_V242_N_IPC] = {
    { "MESSAGE_PASSING", "typed envelopes between procs" },
    { "SHARED_MEMORY",   "v115 SQLite shared tape"       },
    { "SIGNALS",         "v134 sigma-spike signal"       },
};

typedef struct { const char *name; } cos_v242_fxdir_t;

static const cos_v242_fxdir_t DIRS[COS_V242_N_FS_DIRS] = {
    { "models" }, { "memory" }, { "config" }, { "audit" }, { "skills" },
};

typedef struct { int kid; const char *name; } cos_v242_fxstep_t;

static const cos_v242_fxstep_t BOOT[COS_V242_N_BOOT_STEPS] = {
    {  29, "v29_sigma_core"  },
    { 101, "v101_bridge"     },
    { 106, "v106_server"     },
    { 234, "v234_presence"   },
    { 162, "v162_compose"    },
    { 239, "v239_runtime"    },
};

static const cos_v242_fxstep_t SHUT[COS_V242_N_SHUTDOWN] = {
    { 231, "v231_snapshot"   },
    { 181, "v181_audit"      },
    { 233, "v233_legacy"     },
};

void cos_v242_init(cos_v242_state_t *s, uint64_t seed) {
    memset(s, 0, sizeof(*s));
    s->seed  = seed ? seed : 0x2420B007ULL;
    s->state = COS_V242_STATE_COLD;
}

static int cmp_sigma_asc(const cos_v242_fxp_t *a, const cos_v242_fxp_t *b) {
    if (a->sigma < b->sigma) return -1;
    if (a->sigma > b->sigma) return  1;
    if (a->kid   < b->kid)   return -1;
    if (a->kid   > b->kid)   return  1;
    return 0;
}

void cos_v242_run(cos_v242_state_t *s) {
    uint64_t prev = 0x2420B008ULL;
    s->state = COS_V242_STATE_BOOTING;
    s->boot_step_fail_count = 0;

    /* Deterministic sort: stable insertion sort by (σ,
     * kernel_id) so test harnesses don't rely on
     * qsort ordering. */
    cos_v242_fxp_t order[COS_V242_N_PROCESSES];
    for (int i = 0; i < COS_V242_N_PROCESSES; ++i) order[i] = PROCS[i];
    for (int i = 1; i < COS_V242_N_PROCESSES; ++i) {
        cos_v242_fxp_t key = order[i];
        int j = i - 1;
        while (j >= 0 && cmp_sigma_asc(&order[j], &key) > 0) {
            order[j + 1] = order[j];
            --j;
        }
        order[j + 1] = key;
    }
    for (int i = 0; i < COS_V242_N_PROCESSES; ++i) {
        cos_v242_proc_t *p = &s->procs[i];
        memset(p, 0, sizeof(*p));
        p->kernel_id = order[i].kid;
        cpy(p->name, sizeof(p->name), order[i].name);
        p->sigma    = order[i].sigma;
        p->priority = i;
        p->ready    = false;
        prev = fnv1a(p->name, strlen(p->name), prev);
        prev = fnv1a(&p->kernel_id, sizeof(p->kernel_id), prev);
        prev = fnv1a(&p->priority,  sizeof(p->priority),  prev);
    }

    for (int i = 0; i < COS_V242_N_IPC; ++i) {
        cos_v242_ipc_t *c = &s->ipc[i];
        memset(c, 0, sizeof(*c));
        cpy(c->name,    sizeof(c->name),    IPCS[i].name);
        cpy(c->backing, sizeof(c->backing), IPCS[i].backing);
        prev = fnv1a(c->name, strlen(c->name), prev);
    }

    for (int i = 0; i < COS_V242_N_FS_DIRS; ++i) {
        cos_v242_fsdir_t *d = &s->fsdirs[i];
        memset(d, 0, sizeof(*d));
        cpy(d->name, sizeof(d->name), DIRS[i].name);
        snprintf(d->path, sizeof(d->path), "~/.creation-os/%s/", DIRS[i].name);
        prev = fnv1a(d->path, strlen(d->path), prev);
    }

    /* Boot sequence — mark the participating procs
     * ready.  A deviation from BOOT[] order would flip
     * `ok=false` for the failing step; the v0 fixture
     * always succeeds. */
    for (int i = 0; i < COS_V242_N_BOOT_STEPS; ++i) {
        cos_v242_bootstep_t *b = &s->boot[i];
        memset(b, 0, sizeof(*b));
        b->step      = i + 1;
        b->kernel_id = BOOT[i].kid;
        cpy(b->name, sizeof(b->name), BOOT[i].name);
        b->ok        = true;
        if (!b->ok) s->boot_step_fail_count++;
        for (int j = 0; j < COS_V242_N_PROCESSES; ++j) {
            if (s->procs[j].kernel_id == BOOT[i].kid) {
                s->procs[j].ready = true;
                break;
            }
        }
        prev = fnv1a(b->name, strlen(b->name), prev);
        prev = fnv1a(&b->step, sizeof(b->step), prev);
    }

    s->n_ready_kernels = 0;
    for (int j = 0; j < COS_V242_N_PROCESSES; ++j)
        if (s->procs[j].ready) s->n_ready_kernels++;

    s->sigma_os = (float)s->boot_step_fail_count /
                  (float)COS_V242_N_BOOT_STEPS;
    if (s->sigma_os < 0.0f) s->sigma_os = 0.0f;
    if (s->sigma_os > 1.0f) s->sigma_os = 1.0f;

    if (s->boot_step_fail_count == 0 &&
        s->n_ready_kernels >= COS_V242_MIN_READY_KERNELS)
        s->state = COS_V242_STATE_READY;

    for (int i = 0; i < COS_V242_N_SHUTDOWN; ++i) {
        cos_v242_bootstep_t *b = &s->shutdown[i];
        memset(b, 0, sizeof(*b));
        b->step      = i + 1;
        b->kernel_id = SHUT[i].kid;
        cpy(b->name, sizeof(b->name), SHUT[i].name);
        b->ok        = true;
        prev = fnv1a(b->name, strlen(b->name), prev);
        prev = fnv1a(&b->step, sizeof(b->step), prev);
    }
    s->state = COS_V242_STATE_STOPPED;

    struct { int state, nr, fc; float so; uint64_t prev; } trec;
    memset(&trec, 0, sizeof(trec));
    trec.state = (int)s->state;
    trec.nr    = s->n_ready_kernels;
    trec.fc    = s->boot_step_fail_count;
    trec.so    = s->sigma_os;
    trec.prev  = prev;
    prev = fnv1a(&trec, sizeof(trec), prev);

    s->terminal_hash = prev;
    s->chain_valid   = true;
}

static const char *state_name(cos_v242_state_e st) {
    switch (st) {
    case COS_V242_STATE_COLD:    return "COLD";
    case COS_V242_STATE_BOOTING: return "BOOTING";
    case COS_V242_STATE_READY:   return "READY";
    case COS_V242_STATE_STOPPED: return "STOPPED";
    }
    return "UNKNOWN";
}

size_t cos_v242_to_json(const cos_v242_state_t *s, char *buf, size_t cap) {
    int n = snprintf(buf, cap,
        "{\"kernel\":\"v242\","
        "\"n_processes\":%d,\"n_ipc\":%d,\"n_fs_dirs\":%d,"
        "\"n_boot_steps\":%d,\"n_shutdown_steps\":%d,"
        "\"n_ready_kernels\":%d,\"state\":\"%s\","
        "\"boot_step_fail_count\":%d,\"sigma_os\":%.4f,"
        "\"chain_valid\":%s,\"terminal_hash\":\"%016llx\","
        "\"processes\":[",
        COS_V242_N_PROCESSES, COS_V242_N_IPC, COS_V242_N_FS_DIRS,
        COS_V242_N_BOOT_STEPS, COS_V242_N_SHUTDOWN,
        s->n_ready_kernels, state_name(s->state),
        s->boot_step_fail_count, s->sigma_os,
        s->chain_valid ? "true" : "false",
        (unsigned long long)s->terminal_hash);
    if (n < 0 || (size_t)n >= cap) return 0;
    size_t off = (size_t)n;
    for (int i = 0; i < COS_V242_N_PROCESSES; ++i) {
        const cos_v242_proc_t *p = &s->procs[i];
        int q = snprintf(buf + off, cap - off,
            "%s{\"kernel_id\":%d,\"name\":\"%s\","
            "\"sigma\":%.4f,\"priority\":%d,\"ready\":%s}",
            i == 0 ? "" : ",",
            p->kernel_id, p->name, p->sigma, p->priority,
            p->ready ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int m = snprintf(buf + off, cap - off, "],\"ipc\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < COS_V242_N_IPC; ++i) {
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"backing\":\"%s\"}",
            i == 0 ? "" : ",",
            s->ipc[i].name, s->ipc[i].backing);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    m = snprintf(buf + off, cap - off, "],\"fs_dirs\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < COS_V242_N_FS_DIRS; ++i) {
        int q = snprintf(buf + off, cap - off,
            "%s{\"name\":\"%s\",\"path\":\"%s\"}",
            i == 0 ? "" : ",",
            s->fsdirs[i].name, s->fsdirs[i].path);
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    m = snprintf(buf + off, cap - off, "],\"boot\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < COS_V242_N_BOOT_STEPS; ++i) {
        int q = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"kernel_id\":%d,"
            "\"name\":\"%s\",\"ok\":%s}",
            i == 0 ? "" : ",",
            s->boot[i].step, s->boot[i].kernel_id,
            s->boot[i].name, s->boot[i].ok ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    m = snprintf(buf + off, cap - off, "],\"shutdown\":[");
    if (m < 0 || off + (size_t)m >= cap) return 0;
    off += (size_t)m;
    for (int i = 0; i < COS_V242_N_SHUTDOWN; ++i) {
        int q = snprintf(buf + off, cap - off,
            "%s{\"step\":%d,\"kernel_id\":%d,"
            "\"name\":\"%s\",\"ok\":%s}",
            i == 0 ? "" : ",",
            s->shutdown[i].step, s->shutdown[i].kernel_id,
            s->shutdown[i].name,
            s->shutdown[i].ok ? "true" : "false");
        if (q < 0 || off + (size_t)q >= cap) return 0;
        off += (size_t)q;
    }
    int z = snprintf(buf + off, cap - off, "]}");
    if (z < 0 || off + (size_t)z >= cap) return 0;
    return off + (size_t)z;
}

int cos_v242_self_test(void) {
    cos_v242_state_t s;
    cos_v242_init(&s, 0x2420B007ULL);
    cos_v242_run(&s);
    if (!s.chain_valid) return 1;

    /* Priority == dense [0, N) AND == argsort σ asc. */
    int seen[COS_V242_N_PROCESSES] = {0};
    for (int i = 0; i < COS_V242_N_PROCESSES; ++i) {
        int pr = s.procs[i].priority;
        if (pr < 0 || pr >= COS_V242_N_PROCESSES) return 2;
        if (seen[pr]) return 3;
        seen[pr] = 1;
        if (s.procs[i].sigma < 0.0f || s.procs[i].sigma > 1.0f) return 4;
    }
    for (int i = 1; i < COS_V242_N_PROCESSES; ++i) {
        if (s.procs[i].sigma < s.procs[i - 1].sigma - 1e-6f) return 5;
    }

    const char *ipc_names[COS_V242_N_IPC] = {
        "MESSAGE_PASSING", "SHARED_MEMORY", "SIGNALS"
    };
    for (int i = 0; i < COS_V242_N_IPC; ++i) {
        if (strcmp(s.ipc[i].name, ipc_names[i]) != 0) return 6;
    }

    const char *dir_names[COS_V242_N_FS_DIRS] = {
        "models", "memory", "config", "audit", "skills"
    };
    for (int i = 0; i < COS_V242_N_FS_DIRS; ++i) {
        if (strcmp(s.fsdirs[i].name, dir_names[i]) != 0) return 7;
        if (strncmp(s.fsdirs[i].path, "~/.creation-os/", 15) != 0) return 8;
    }

    int boot_order[COS_V242_N_BOOT_STEPS] = { 29, 101, 106, 234, 162, 239 };
    for (int i = 0; i < COS_V242_N_BOOT_STEPS; ++i) {
        if (s.boot[i].step != i + 1) return 9;
        if (s.boot[i].kernel_id != boot_order[i]) return 10;
        if (!s.boot[i].ok) return 11;
    }
    if (s.n_ready_kernels < COS_V242_MIN_READY_KERNELS) return 12;
    int need[5] = { 29, 101, 106, 234, 162 };
    for (int k = 0; k < 5; ++k) {
        bool found = false;
        for (int j = 0; j < COS_V242_N_PROCESSES; ++j) {
            if (s.procs[j].kernel_id == need[k] && s.procs[j].ready) {
                found = true; break;
            }
        }
        if (!found) return 13;
    }

    int shut_order[COS_V242_N_SHUTDOWN] = { 231, 181, 233 };
    for (int i = 0; i < COS_V242_N_SHUTDOWN; ++i) {
        if (s.shutdown[i].step != i + 1) return 14;
        if (s.shutdown[i].kernel_id != shut_order[i]) return 15;
        if (!s.shutdown[i].ok) return 16;
    }

    if (s.sigma_os < 0.0f || s.sigma_os > 1.0f) return 17;
    if (s.sigma_os > 1e-6f) return 18;
    if (s.state != COS_V242_STATE_STOPPED) return 19;

    cos_v242_state_t t;
    cos_v242_init(&t, 0x2420B007ULL);
    cos_v242_run(&t);
    if (t.terminal_hash != s.terminal_hash) return 20;
    return 0;
}
