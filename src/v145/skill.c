/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v145 σ-Skill — implementation.
 */
#include "skill.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* SplitMix64 PRNG — same one used by v139/v143 for byte-identical
 * reproducibility across the stack. */
static uint64_t splitmix(uint64_t *s) {
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}
static float urand(uint64_t *s) {
    uint64_t x = splitmix(s);
    return (float)((x >> 40) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    for (; i + 1 < cap && src[i]; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

void cos_v145_library_init(cos_v145_library_t *lib) {
    if (!lib) return;
    memset(lib, 0, sizeof *lib);
}

int cos_v145_acquire(cos_v145_library_t *lib,
                     const char *name, const char *target_topic,
                     float difficulty, uint64_t seed,
                     float tau_install,
                     cos_v145_skill_t *out) {
    if (!lib || !name || !target_topic) return -1;
    if (lib->n_skills >= COS_V145_MAX_SKILLS)  return -2;
    if (difficulty < 0.0f) difficulty = 0.0f;
    if (difficulty > 1.0f) difficulty = 1.0f;
    if (tau_install <= 0.0f) tau_install = COS_V145_TAU_INSTALL;

    /* Synthesise σ_avg deterministically.  The mean is a linear
     * function of difficulty — on the default seed this maps the
     * 0.0→1.0 difficulty axis onto ~0.12→0.68 σ_avg, so medium
     * difficulties install and very-hard ones bounce off τ.  A
     * per-seed jitter of ±0.07 keeps the skills distinct. */
    uint64_t st = seed ^ 0xC05C05C05C05C05DULL;
    float jitter = (urand(&st) - 0.5f) * 0.14f;
    float sigma  = 0.12f + 0.56f * difficulty + jitter;
    if (sigma < 0.02f) sigma = 0.02f;
    if (sigma > 0.99f) sigma = 0.99f;

    /* Self-test pass-rate tracks σ inversely: low σ ⇒ high pass. */
    int total = 10;
    int passed = (int)(total * (1.0f - sigma) + 0.5f);
    if (passed < 0)     passed = 0;
    if (passed > total) passed = total;

    cos_v145_skill_t s;
    memset(&s, 0, sizeof s);
    safe_copy(s.name,         sizeof s.name,         name);
    safe_copy(s.target_topic, sizeof s.target_topic, target_topic);
    s.rank         = COS_V145_RANK_DEFAULT;
    s.size_kb      = 200;                 /* symbolic */
    s.sigma_avg    = sigma;
    s.tests_passed = passed;
    s.tests_total  = total;
    s.is_shareable = 0;
    s.created_seed = seed;
    s.generation   = 1;

    if (out) *out = s;

    /* Contract S1: σ-gate on install. */
    if (sigma >= tau_install) {
        lib->n_rejected++;
        return 1;
    }

    lib->skills[lib->n_skills++] = s;
    return 0;
}

int cos_v145_route(const cos_v145_library_t *lib, const char *topic) {
    if (!lib || !topic) return -1;
    int   best = -1;
    float best_sigma = 2.0f;
    for (int i = 0; i < lib->n_skills; ++i) {
        if (strcmp(lib->skills[i].target_topic, topic) != 0) continue;
        if (lib->skills[i].sigma_avg < best_sigma) {
            best_sigma = lib->skills[i].sigma_avg;
            best       = i;
        }
    }
    return best;
}

int cos_v145_stack(const cos_v145_library_t *lib,
                   const int *indices, int n, float *out_sigma) {
    if (!lib || !indices || n <= 0 || !out_sigma) return -1;
    float min_sigma = 2.0f;
    for (int i = 0; i < n; ++i) {
        int k = indices[i];
        if (k < 0 || k >= lib->n_skills) return -2;
        if (lib->skills[k].sigma_avg < min_sigma)
            min_sigma = lib->skills[k].sigma_avg;
    }
    /* LoRA-merge σ-aggregation.  For n stacked skills whose
     * answers correlate only weakly, the σ on the joint answer
     * is ≤ σ_min.  We use the conservative 1/√n shrinkage.    */
    float denom = sqrtf((float)n);
    if (denom < 1.0f) denom = 1.0f;
    *out_sigma = min_sigma / denom;
    return 0;
}

int cos_v145_share(cos_v145_library_t *lib, float tau) {
    if (!lib) return -1;
    int count = 0;
    for (int i = 0; i < lib->n_skills; ++i) {
        int sh = lib->skills[i].sigma_avg < tau ? 1 : 0;
        lib->skills[i].is_shareable = sh;
        count += sh;
    }
    return count;
}

int cos_v145_evolve(cos_v145_library_t *lib, int idx, uint64_t seed) {
    if (!lib || idx < 0 || idx >= lib->n_skills) return -1;
    cos_v145_skill_t *cur = &lib->skills[idx];

    /* Deterministic CMA-ES-style perturbation in σ-space.  We
     * draw two candidates and keep the better one; if neither
     * improves on the current skill we reject (S5 no-regression). */
    uint64_t st = seed ^ 0xE501E501ULL;
    float best_delta = 0.0f;
    for (int k = 0; k < 2; ++k) {
        /* Centered in [−0.15, +0.10] — slight improvement bias. */
        float delta = (urand(&st) - 0.6f) * 0.25f;
        if (delta < best_delta) best_delta = delta;
    }
    float sigma_new = cur->sigma_avg + best_delta;
    if (sigma_new < 0.01f) sigma_new = 0.01f;
    if (sigma_new >= cur->sigma_avg) return 0;   /* no improvement */

    cur->sigma_avg    = sigma_new;
    cur->generation  += 1;
    int total = cur->tests_total > 0 ? cur->tests_total : 10;
    int passed = (int)(total * (1.0f - sigma_new) + 0.5f);
    if (passed < 0)     passed = 0;
    if (passed > total) passed = total;
    cur->tests_passed = passed;
    return 1;
}

int cos_v145_seed_default(cos_v145_library_t *lib, uint64_t seed) {
    if (!lib) return -1;
    cos_v145_library_init(lib);
    /* Difficulties chosen so that on seed 0xC05 all five install
     * cleanly (σ < 0.60) — exercised in the self-test. */
    struct { const char *n, *t; float d; } roster[5] = {
        { "algebra_step_by_step", "math",     0.15f },
        { "c_macro_safety",       "code",     0.05f },
        { "world_war_chronology", "history",  0.40f },
        { "nl_glue_words",        "language", 0.22f },
        { "propositional_proof",  "logic",    0.10f },
    };
    uint64_t s = seed;
    for (int i = 0; i < 5; ++i) {
        int rc = cos_v145_acquire(lib, roster[i].n, roster[i].t,
                                  roster[i].d, s + (uint64_t)i * 101ull,
                                  COS_V145_TAU_INSTALL, NULL);
        (void)rc;    /* counted in n_skills / n_rejected              */
    }
    return lib->n_skills;
}

int cos_v145_skill_to_json(const cos_v145_skill_t *s,
                           char *buf, size_t cap) {
    if (!s || !buf || cap == 0) return -1;
    int rc = snprintf(buf, cap,
        "{\"name\":\"%s\",\"topic\":\"%s\",\"rank\":%d,"
        "\"size_kb\":%d,\"sigma_avg\":%.6f,"
        "\"tests_passed\":%d,\"tests_total\":%d,"
        "\"is_shareable\":%s,\"generation\":%d,"
        "\"created_seed\":%llu}",
        s->name, s->target_topic, s->rank, s->size_kb,
        (double)s->sigma_avg,
        s->tests_passed, s->tests_total,
        s->is_shareable ? "true" : "false",
        s->generation,
        (unsigned long long)s->created_seed);
    if (rc < 0 || (size_t)rc >= cap) return -1;
    return rc;
}

int cos_v145_library_to_json(const cos_v145_library_t *lib,
                             char *buf, size_t cap) {
    if (!lib || !buf || cap == 0) return -1;
    size_t off = 0;
    int rc = snprintf(buf + off, cap - off,
        "{\"v145_library\":true,\"n_skills\":%d,\"n_rejected\":%d,"
        "\"skills\":[",
        lib->n_skills, lib->n_rejected);
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    for (int i = 0; i < lib->n_skills; ++i) {
        if (i) {
            if (off + 1 >= cap) return -1;
            buf[off++] = ',';
            buf[off]   = '\0';
        }
        int w = cos_v145_skill_to_json(&lib->skills[i],
                                       buf + off, cap - off);
        if (w < 0) return -1;
        off += (size_t)w;
    }
    rc = snprintf(buf + off, cap - off, "]}");
    if (rc < 0 || (size_t)rc >= cap - off) return -1;
    off += (size_t)rc;
    return (int)off;
}

/* ================================================================
 * Self-test
 * ============================================================= */

#define _CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "v145 self-test FAIL: %s (line %d)\n", \
                (msg), __LINE__); return 1; \
    } \
} while (0)

int cos_v145_self_test(void) {
    cos_v145_library_t lib;

    /* --- A. Empty library --------------------------------------- */
    cos_v145_library_init(&lib);
    _CHECK(lib.n_skills == 0, "empty library starts at 0");
    _CHECK(cos_v145_route(&lib, "math") == -1,
           "route on empty lib returns -1");

    /* --- B. Seed default 5 skills ------------------------------ */
    int n = cos_v145_seed_default(&lib, 0xC05ULL);
    fprintf(stderr, "check-v145: seeded %d skills (rejected %d)\n",
            n, lib.n_rejected);
    _CHECK(n == 5, "exactly 5 skills installed on default seed");

    /* --- C. Contract S1: σ-gate reject ------------------------- */
    /* A very hard acquisition attempt (difficulty 0.95) must
     * produce σ ≥ τ_install and be rejected. */
    cos_v145_skill_t rej;
    int rc = cos_v145_acquire(&lib, "impossible", "metaphysics",
                              0.95f, 0xBADBADULL,
                              COS_V145_TAU_INSTALL, &rej);
    fprintf(stderr, "check-v145: hard acquire σ=%.3f rc=%d\n",
            (double)rej.sigma_avg, rc);
    _CHECK(rc == 1, "σ-gate rejected hard acquisition");
    _CHECK(lib.n_skills == 5, "rejected skill did NOT install");
    _CHECK(rej.sigma_avg >= COS_V145_TAU_INSTALL,
           "rejected skill's σ ≥ τ_install");

    /* --- D. Contract S2: route argmin-σ by topic --------------- */
    int mi = cos_v145_route(&lib, "math");
    _CHECK(mi >= 0, "math route hit");
    _CHECK(strcmp(lib.skills[mi].target_topic, "math") == 0,
           "routed skill has target_topic=math");
    int ci = cos_v145_route(&lib, "code");
    _CHECK(ci >= 0 && ci != mi, "code route distinct from math");
    _CHECK(cos_v145_route(&lib, "nonexistent") == -1,
           "route for unknown topic returns -1");

    /* --- E. Contract S3: stack σ ≤ min/√n --------------------- */
    int pair[2] = { mi, ci };
    float sig_stacked = -1.0f;
    _CHECK(cos_v145_stack(&lib, pair, 2, &sig_stacked) == 0, "stack ok");
    float sig_min = lib.skills[mi].sigma_avg < lib.skills[ci].sigma_avg
                    ? lib.skills[mi].sigma_avg : lib.skills[ci].sigma_avg;
    fprintf(stderr, "check-v145: stack σ=%.4f (min=%.4f, √2-scaled=%.4f)\n",
            (double)sig_stacked, (double)sig_min,
            (double)(sig_min / sqrtf(2.0f)));
    _CHECK(sig_stacked <= sig_min + 1e-6f,
           "stacked σ ≤ min individual σ");
    _CHECK(fabsf(sig_stacked - sig_min / sqrtf(2.0f)) < 1e-4f,
           "stacked σ exactly min/√n");

    /* --- F. Contract S4: share at τ_share --------------------- */
    int shared = cos_v145_share(&lib, COS_V145_TAU_SHARE);
    fprintf(stderr, "check-v145: %d skills shareable at τ=%.2f\n",
            shared, (double)COS_V145_TAU_SHARE);
    _CHECK(shared >= 1, "at least one shareable skill at default τ");
    for (int i = 0; i < lib.n_skills; ++i) {
        int should = lib.skills[i].sigma_avg < COS_V145_TAU_SHARE;
        _CHECK(lib.skills[i].is_shareable == should,
               "is_shareable matches σ_avg < τ exactly");
    }

    /* --- G. Contract S5: evolve is monotone ------------------- */
    float sigma_before = lib.skills[mi].sigma_avg;
    int   gen_before   = lib.skills[mi].generation;
    /* Try a few seeds until we get an improvement.  The contract
     * is that *when* we install, σ strictly decreases and
     * generation bumps; when no seed yields improvement the
     * library is untouched.  On the default roster we reliably
     * find an improving seed within the first few tries. */
    int installed = 0;
    for (uint64_t s = 1; s <= 8 && !installed; ++s) {
        int r = cos_v145_evolve(&lib, mi, s);
        _CHECK(r == 0 || r == 1,
               "evolve returns 0 (no-op) or 1 (installed)");
        if (r == 1) installed = 1;
    }
    _CHECK(installed == 1,
           "at least one of 8 seeds improved the math skill");
    _CHECK(lib.skills[mi].sigma_avg < sigma_before,
           "σ strictly decreases on installed evolution");
    _CHECK(lib.skills[mi].generation == gen_before + 1,
           "generation bumped exactly once");

    /* --- H. JSON shape ---------------------------------------- */
    char buf[4096];
    int jw = cos_v145_library_to_json(&lib, buf, sizeof buf);
    _CHECK(jw > 0, "library json emit");
    _CHECK(strstr(buf, "\"v145_library\":true") != NULL, "tag");
    _CHECK(strstr(buf, "\"n_skills\":5") != NULL, "n_skills in json");

    fprintf(stderr,
        "check-v145: OK (5 skills + σ-gate + route + stack + share + evolve)\n");
    return 0;
}
