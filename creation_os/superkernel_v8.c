/*
 * CREATION OS — SUPERKERNEL v8.0 — THE ATTENTION KERNEL (library)
 * CC BY 4.0 — Lauri Elias Rainio
 */
#include "superkernel_v8.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef POPCNT
#define POPCNT(x) __builtin_popcount((unsigned)(x))
#endif
#define HAMMING(a, b) POPCNT((unsigned)((a) ^ (b)))

#define N_A       SK8_N_A
#define A_MASK    ((1u << N_A) - 1)
#define HIST_LEN  256
#define MAX_IMM   32
#define MAX_ORB   32
#define MAX_MEM   SK8_MAX_MEM
#define MAX_QUAR  SK8_MAX_QUAR
#define MAX_CHORD 16
#define MAX_RECEIPT 64
#define MAX_DREAM 32

#define M_HO   (7u << 0)
#define M_CT   (7u << 3)
#define M_MM   (7u << 6)
#define M_EM   (3u << 9)
#define M_PU   (3u << 11)
#define M_SH   (1u << 13)
#define M_HA   (1u << 14)
#define M_ML   (7u << 15)
#define M_STRUCT (M_HO | M_CT | M_PU)
#define M_EPIST  (M_MM | M_EM | M_SH | M_HA | M_ML)
#define N_LAYERS 8

static const uint32_t LAYER_MASKS[N_LAYERS] = {
    M_HO, M_CT, M_MM, M_EM, M_PU, M_SH, M_HA, M_ML
};
static const char *LAYER_NAMES[N_LAYERS] = { "HO", "CT", "MM", "EM", "PU", "SH", "HA", "ML" };
static const char *PH[] = { "NORMAL", "DEGRADED", "CRITICAL", "COLLAPSE", "RECOVERY" };
static const char *ANAMES[] = {
    "HO01", "HO02", "HO03", "CT01", "CT02", "CT03",
    "MM01", "MM02", "MM03", "EM01", "EM02", "PU01", "PU02",
    "SH01", "HA01", "RI01", "NS01", "PO01"
};

typedef struct {
    int layer_sigma[N_LAYERS];
    int max_sigma;
    int min_sigma;
    int fracture;
    int worst_layer;
    int structural;
    int epistemic;
    int skew;
} fracture_t;

typedef struct {
    bool mass_quarantine;
    bool false_coherence;
    bool identity_amputation;
    bool high_fracture_low_sigma;
} antigol_t;

typedef struct {
    int nearest_dist;
    int nearest_idx;
    uint32_t nearest_sig;
    int escape_velocity;
    int return_cost;
} orbit_field_t;

typedef struct {
    const char *name;
    uint32_t result_state;
    int sigma;
    int fracture;
    int orbit_dist;
    float elegance;
    float cost;
    bool antigol;
} branch_t;

static int sk8_boot_silent = 0;

static fracture_t compute_fracture(uint32_t state) {
    fracture_t f = { .max_sigma = 0, .min_sigma = 99, .fracture = 0,
                     .worst_layer = 0, .structural = 0, .epistemic = 0, .skew = 0 };
    for (int i = 0; i < N_LAYERS; i++) {
        f.layer_sigma[i] = POPCNT(state & LAYER_MASKS[i]);
        if (f.layer_sigma[i] > f.max_sigma) {
            f.max_sigma = f.layer_sigma[i];
            f.worst_layer = i;
        }
        if (f.layer_sigma[i] < f.min_sigma) {
            f.min_sigma = f.layer_sigma[i];
        }
    }
    if (f.min_sigma == 99) {
        f.min_sigma = 0;
    }
    f.fracture = f.max_sigma - f.min_sigma;
    f.structural = POPCNT(state & M_STRUCT);
    f.epistemic  = POPCNT(state & M_EPIST);
    f.skew       = abs(f.structural - f.epistemic);
    (void)LAYER_NAMES;
    return f;
}

static antigol_t check_antigoals(int sigma, int fracture, int mcnt, int qcnt, int n_orbit) {
    antigol_t a = { false, false, false, false };
    if (mcnt > 0 && qcnt > mcnt / 2) {
        a.mass_quarantine = true;
    }
    if (sigma == 0 && mcnt == 0) {
        a.false_coherence = true;
    }
    if (sigma <= 1 && n_orbit == 0) {
        a.identity_amputation = true;
    }
    if (sigma <= 2 && fracture >= 3) {
        a.high_fracture_low_sigma = true;
    }
    return a;
}

static bool antigol_violated(antigol_t *a) {
    return a->mass_quarantine || a->false_coherence ||
           a->identity_amputation || a->high_fracture_low_sigma;
}

static float compute_elegance(int hamming_moved, int orbit_dist_before, int orbit_dist_after,
                              int frac_before, int frac_after, bool on_orbit_after, int sigma_after) {
    float e = 1.0f;
    e -= 0.05f * (float)hamming_moved;
    if (orbit_dist_after > orbit_dist_before) {
        e -= 0.1f * (float)(orbit_dist_after - orbit_dist_before);
    }
    if (frac_after > frac_before) {
        e -= 0.15f * (float)(frac_after - frac_before);
    }
    if (on_orbit_after) {
        e += 0.2f;
    }
    e -= 0.02f * (float)sigma_after;
    if (e < 0) {
        e = 0;
    }
    if (e > 1) {
        e = 1;
    }
    return e;
}

typedef bool (*tfn)(sk8_kernel_t *);

static bool t00(sk8_kernel_t *k) { return k->inv; }
static bool t01(sk8_kernel_t *k) {
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == 0 && k->mem[i].conf < 0.5f) {
            return false;
        }
    }
    return true;
}
static bool t02(sk8_kernel_t *k) { return k->d_sigma >= -3; }
static bool t03(sk8_kernel_t *k) { return k->sigma < 3; }
static bool t04(sk8_kernel_t *k) {
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == 2) {
            continue;
        }
        for (int j = i + 1; j < k->mcnt; j++) {
            if (k->mem[j].stat == 2) {
                continue;
            }
            if (strcmp(k->mem[i].key, k->mem[j].key) == 0 &&
                strcmp(k->mem[i].val, k->mem[j].val) != 0) {
                return false;
            }
        }
    }
    return true;
}
static bool t05(sk8_kernel_t *k) { (void)k; return true; }
static bool t06(sk8_kernel_t *k) { return strlen(k->identity) > 0; }
static bool t07(sk8_kernel_t *k) { (void)k; return true; }
static bool t08(sk8_kernel_t *k) {
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].conf < 0 || k->mem[i].conf > 1) {
            return false;
        }
    }
    return true;
}
static bool t09(sk8_kernel_t *k) { (void)k; return true; }
static bool t10(sk8_kernel_t *k) { return k->sigma <= N_A; }
static bool t11(sk8_kernel_t *k) { return k->running && k->inv; }
static bool t12(sk8_kernel_t *k) { (void)k; return true; }
static bool t13(sk8_kernel_t *k) { (void)k; return true; }
static bool t14(sk8_kernel_t *k) {
    if (k->hcount < 16) {
        return true;
    }
    for (int p = 3; p < 8; p++) {
        int m = 0;
        for (int t = 0; t < 8; t++) {
            int i1 = (k->hidx - 1 - t + HIST_LEN) % HIST_LEN;
            int i2 = (k->hidx - 1 - t - p + HIST_LEN) % HIST_LEN;
            if ((k->history[i1] & A_MASK) == (k->history[i2] & A_MASK)) {
                m++;
            }
        }
        if (m >= 6) {
            return false;
        }
    }
    return true;
}
static bool t15(sk8_kernel_t *k) {
    return abs((int)POPCNT(k->state & M_STRUCT) - (int)POPCNT(k->state & M_EPIST)) <= 2;
}
static bool t16(sk8_kernel_t *k) { return k->d2_sigma <= 3; }
static bool t17(sk8_kernel_t *k) { return k->loop_period == 0; }

static tfn TESTS[N_A] = {
    t00, t01, t02, t03, t04, t05, t06, t07, t08, t09, t10, t11, t12, t13, t14, t15, t16, t17
};

static void enforce(sk8_kernel_t *k) {
    uint32_t s = 0;
    for (int i = 0; i < N_A; i++) {
        if (!TESTS[i](k)) {
            s |= (1u << i);
        }
    }
    k->state = s;
    k->prev_sigma = k->sigma;
    k->sigma = POPCNT(s & A_MASK);
    k->prev_d = k->d_sigma;
    k->d_sigma = k->sigma - k->prev_sigma;
    k->d2_sigma = k->d_sigma - k->prev_d;
}

static orbit_field_t compute_orbit_field(sk8_kernel_t *k) {
    orbit_field_t f = { .nearest_dist = N_A + 1, .nearest_idx = -1,
                        .nearest_sig = 0, .escape_velocity = 0, .return_cost = 0 };
    for (int i = 0; i < k->n_orbit; i++) {
        int d = HAMMING(k->state & A_MASK, k->orbit[i] & A_MASK);
        if (d < f.nearest_dist) {
            f.nearest_dist = d;
            f.nearest_idx = i;
            f.nearest_sig = k->orbit[i];
        }
    }
    if (f.nearest_dist <= N_A) {
        uint32_t on_orbit = f.nearest_sig & A_MASK;
        uint32_t need_to_break = on_orbit & ~(k->state & A_MASK);
        f.escape_velocity = POPCNT(need_to_break);
        f.return_cost = f.nearest_dist;
    }
    return f;
}

static void mem_store(sk8_kernel_t *k, const char *key, const char *val, float c) {
    if (k->mcnt >= MAX_MEM) {
        return;
    }
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == 2) {
            continue;
        }
        if (strcmp(k->mem[i].key, key) == 0 && strcmp(k->mem[i].val, val) != 0) {
            if (c > k->mem[i].conf) {
                if (k->qcnt < MAX_QUAR) {
                    k->quar[k->qcnt] = k->mem[i];
                    k->quar[k->qcnt].stat = 2;
                    k->qcnt++;
                }
                strncpy(k->mem[i].val, val, sizeof(k->mem[i].val) - 1);
                k->mem[i].val[sizeof(k->mem[i].val) - 1] = '\0';
                k->mem[i].conf = c;
                k->mem[i].stat = (c > 0.9f) ? 0 : 1;
            }
            return;
        }
    }
    sk8_mem_t *e = &k->mem[k->mcnt++];
    strncpy(e->key, key, sizeof(e->key) - 1);
    e->key[sizeof(e->key) - 1] = '\0';
    strncpy(e->val, val, sizeof(e->val) - 1);
    e->val[sizeof(e->val) - 1] = '\0';
    e->conf = c;
    e->stat = (c > 0.9f) ? 0 : 1;
}

static int recover_core(sk8_kernel_t *k) {
    int rep = 0;
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == 2) {
            continue;
        }
        for (int j = i + 1; j < k->mcnt; j++) {
            if (k->mem[j].stat == 2) {
                continue;
            }
            if (strcmp(k->mem[i].key, k->mem[j].key) == 0 &&
                strcmp(k->mem[i].val, k->mem[j].val) != 0) {
                int v = (k->mem[i].conf >= k->mem[j].conf) ? j : i;
                if (k->qcnt < MAX_QUAR) {
                    k->quar[k->qcnt] = k->mem[v];
                    k->quar[k->qcnt].stat = 2;
                    k->qcnt++;
                    memmove(&k->mem[v], &k->mem[v + 1],
                            (size_t)(k->mcnt - v - 1) * sizeof(sk8_mem_t));
                    k->mcnt--;
                    rep++;
                    j--;
                }
            }
        }
    }
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == 0 && k->mem[i].conf < 0.5f) {
            k->mem[i].stat = 1;
            rep++;
        }
    }
    return rep;
}

static branch_t simulate_recovery(sk8_kernel_t *k, int strategy) {
    branch_t b = { .name = "?", .result_state = k->state, .sigma = k->sigma,
                   .fracture = 0, .orbit_dist = N_A, .elegance = 0, .cost = 999, .antigol = false };
    sk8_kernel_t sim;
    memcpy(&sim, k, sizeof(sim));
    switch (strategy) {
    case 0:
        b.name = "QUARANTINE";
        for (int i = 0; i < sim.mcnt; i++) {
            for (int j = i + 1; j < sim.mcnt; j++) {
                if (sim.mem[j].stat != 2 && sim.mem[i].stat != 2 &&
                    strcmp(sim.mem[i].key, sim.mem[j].key) == 0 &&
                    strcmp(sim.mem[i].val, sim.mem[j].val) != 0) {
                    int v = (sim.mem[i].conf >= sim.mem[j].conf) ? j : i;
                    sim.mem[v].stat = 2;
                }
            }
        }
        break;
    case 1:
        b.name = "REVOKE";
        for (int i = 0; i < sim.mcnt; i++) {
            if (sim.mem[i].stat == 0 && sim.mem[i].conf < 0.5f) {
                sim.mem[i].stat = 1;
            }
        }
        break;
    case 2: {
        b.name = "GEODESIC";
        orbit_field_t of = compute_orbit_field(&sim);
        if (of.nearest_idx >= 0) {
            sim.state = of.nearest_sig;
        }
        break;
    }
    case 3:
        b.name = "AGGRESSIVE";
        for (int i = 0; i < sim.mcnt; i++) {
            for (int j = i + 1; j < sim.mcnt; j++) {
                if (sim.mem[j].stat != 2 && sim.mem[i].stat != 2 &&
                    strcmp(sim.mem[i].key, sim.mem[j].key) == 0 &&
                    strcmp(sim.mem[i].val, sim.mem[j].val) != 0) {
                    sim.mem[(sim.mem[i].conf >= sim.mem[j].conf) ? j : i].stat = 2;
                }
            }
        }
        for (int i = 0; i < sim.mcnt; i++) {
            if (sim.mem[i].stat == 0 && sim.mem[i].conf < 0.5f) {
                sim.mem[i].stat = 1;
            }
        }
        break;
    case 4:
        b.name = "EXACT-ONLY";
        for (int i = 0; i < sim.mcnt; i++) {
            for (int j = i + 1; j < sim.mcnt; j++) {
                if (sim.mem[j].stat != 2 && sim.mem[i].stat != 2 &&
                    strcmp(sim.mem[i].key, sim.mem[j].key) == 0 &&
                    strcmp(sim.mem[i].val, sim.mem[j].val) != 0) {
                    int v = (sim.mem[i].conf >= sim.mem[j].conf) ? j : i;
                    sim.mem[v].stat = 2;
                    break;
                }
            }
        }
        break;
    default:
        break;
    }
    enforce(&sim);
    b.result_state = sim.state;
    b.sigma = sim.sigma;
    fracture_t frac = compute_fracture(sim.state);
    b.fracture = frac.fracture;
    orbit_field_t of = compute_orbit_field(&sim);
    b.orbit_dist = of.nearest_dist;
    int ham = HAMMING(k->state & A_MASK, sim.state & A_MASK);
    fracture_t pre_frac = compute_fracture(k->state);
    orbit_field_t pre_o = compute_orbit_field(k);
    b.elegance = compute_elegance(ham, pre_o.nearest_dist, of.nearest_dist, pre_frac.fracture,
                                  frac.fracture, of.nearest_dist == 0, sim.sigma);
    antigol_t ag = check_antigoals(sim.sigma, frac.fracture, sim.mcnt, sim.qcnt, sim.n_orbit);
    b.antigol = antigol_violated(&ag);
    b.cost = (float)b.sigma + 0.5f * (float)b.fracture + 0.3f * (float)b.orbit_dist - b.elegance;
    if (b.antigol) {
        b.cost += 100.f;
    }
    return b;
}

static bool check_horizon(sk8_kernel_t *k) {
    orbit_field_t of = compute_orbit_field(k);
    fracture_t fr = compute_fracture(k->state);
    k->beyond_horizon =
        (of.nearest_dist > N_A / 2) && (fr.fracture >= 4) &&
        (k->sigma >= N_A / 2) && (k->loop_period > 0);
    if (k->beyond_horizon && !sk8_boot_silent) {
        printf("[HORIZON] Beyond recovery: orbit=%d frac=%d sigma=%d loop=%d\n",
               of.nearest_dist, fr.fracture, k->sigma, k->loop_period);
    }
    return k->beyond_horizon;
}

static void chord_update(sk8_kernel_t *k) {
    uint32_t failing = k->state & A_MASK;
    if (POPCNT(failing) < 2) {
        return;
    }
    for (int i = 0; i < k->n_chords; i++) {
        if (k->chords[i].pattern == failing) {
            k->chords[i].count++;
            return;
        }
    }
    if (k->n_chords < MAX_CHORD) {
        int cidx = k->n_chords++;
        k->chords[cidx].pattern = failing;
        k->chords[cidx].count = 1;
        k->chords[cidx].n_bits = POPCNT(failing);
        int first = -1, second = -1;
        for (int i = 0; i < N_A; i++) {
            if (failing & (1u << i)) {
                if (first < 0) {
                    first = i;
                } else if (second < 0) {
                    second = i;
                    break;
                }
            }
        }
        if (first >= 0 && second >= 0) {
            snprintf(k->chords[cidx].name, 16, "%s+%s", ANAMES[first], ANAMES[second]);
        } else if (first >= 0) {
            snprintf(k->chords[cidx].name, 16, "%s", ANAMES[first]);
        }
    }
}

static int dream(sk8_kernel_t *k) {
    if (k->n_dreams == 0 && k->hcount < 10) {
        return 0;
    }
    int compressed = 0;
    for (int i = 0; i < k->hcount && i < MAX_DREAM; i++) {
        uint32_t past = k->history[(k->hidx - 1 - i + HIST_LEN) % HIST_LEN];
        if (POPCNT(past & A_MASK) < 3) {
            continue;
        }
        bool known = false;
        for (int j = 0; j < k->n_imm; j++) {
            if (HAMMING(past & A_MASK, k->imm_pat[j]) <= k->imm_radius[j]) {
                known = true;
                k->imm_cnt[j]++;
                if (k->imm_cnt[j] > 5 && k->imm_radius[j] < 4) {
                    k->imm_radius[j]++;
                }
                break;
            }
        }
        if (!known && k->n_imm < MAX_IMM) {
            k->imm_pat[k->n_imm] = past & A_MASK;
            k->imm_cnt[k->n_imm] = 1;
            k->imm_radius[k->n_imm] = 2;
            k->n_imm++;
            compressed++;
        }
    }
    for (int i = 0; i < k->n_imm; i++) {
        for (int j = i + 1; j < k->n_imm; j++) {
            if (HAMMING(k->imm_pat[i], k->imm_pat[j]) <= 2) {
                if (k->imm_cnt[j] > k->imm_cnt[i]) {
                    k->imm_pat[i] = k->imm_pat[j];
                    k->imm_cnt[i] += k->imm_cnt[j];
                } else {
                    k->imm_cnt[i] += k->imm_cnt[j];
                }
                if (k->imm_radius[i] < k->imm_radius[j]) {
                    k->imm_radius[i] = k->imm_radius[j];
                }
                memmove(&k->imm_pat[j], &k->imm_pat[j + 1], (size_t)(k->n_imm - j - 1) * sizeof(uint32_t));
                memmove(&k->imm_cnt[j], &k->imm_cnt[j + 1], (size_t)(k->n_imm - j - 1) * sizeof(int));
                memmove(&k->imm_radius[j], &k->imm_radius[j + 1], (size_t)(k->n_imm - j - 1) * sizeof(int));
                k->n_imm--;
                j--;
                compressed++;
            }
        }
    }
    if (compressed > 0 && !sk8_boot_silent) {
        printf("[DREAM] Compressed %d patterns. Immune: %d entries.\n", compressed, k->n_imm);
    }
    return compressed;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((unused))
#endif
static bool omega_step(sk8_kernel_t *k) {
    k->cycle++;
    k->inv = (1 == 1);
    enforce(k);
    k->history[k->hidx % HIST_LEN] = k->state;
    k->hidx++;
    if (k->hcount < HIST_LEN) {
        k->hcount++;
    }
    {
        uint32_t rev = 0;
        for (int i = 0; i < N_A; i++) {
            if (!TESTS[i](k)) {
                rev |= (1u << i);
            }
        }
        uint32_t diff = (k->state ^ rev) & A_MASK;
        if (POPCNT(diff) > 0) {
            k->state = rev;
            k->sigma = POPCNT(rev & A_MASK);
        }
    }
    k->L = 1.0f - 2.0f * ((float)k->sigma / (float)N_A);
    k->S += k->L;
    if (k->cycle > 0) {
        k->Q = k->S / (float)k->cycle;
    }
    if (k->sigma < k->mass_gap || k->cycle == 1) {
        k->mass_gap = k->sigma;
    }
    if (k->d2_sigma > 0) {
        float a = 0.5f * (float)k->d2_sigma, b = (float)k->d_sigma, c = (float)k->sigma - (float)N_A;
        float d = b * b - 4.f * a * c;
        k->blowup = (d >= 0 && a > 0.01f) ? fmaxf(0.f, (-b + sqrtf(d)) / (2.f * a)) : 999.f;
    } else {
        k->blowup = 999.f;
    }
    k->loop_period = 0;
    if (k->hcount >= 10) {
        for (int p = 2; p < 20 && p < k->hcount / 2; p++) {
            int m = 0;
            for (int t = 0; t < 5; t++) {
                int i1 = (k->hidx - 1 - t + HIST_LEN) % HIST_LEN;
                int i2 = (k->hidx - 1 - t - p + HIST_LEN) % HIST_LEN;
                if ((k->history[i1] & A_MASK) == (k->history[i2] & A_MASK)) {
                    m++;
                }
            }
            if (m >= 4) {
                k->loop_period = p;
                break;
            }
        }
    }
    k->prev_phase = k->phase;
    int ps = k->sigma;
    k->phase = (ps <= 2) ? 0 : (ps <= 4) ? 1 : (ps <= 8) ? 2 : 3;
    if (k->prev_phase >= 2 && k->phase < k->prev_phase) {
        k->phase = 4;
    }
    if (k->phase != k->prev_phase && !sk8_boot_silent) {
        printf("[PHASE] %s -> %s (sigma=%d d=%d d2=%d L=%.3f)\n",
               PH[k->prev_phase], PH[k->phase], k->sigma, k->d_sigma, k->d2_sigma, (double)k->L);
    }
    chord_update(k);
    check_horizon(k);
    if (k->sigma >= 3 && !k->beyond_horizon) {
        branch_t branches[5];
        int best = 0;
        for (int i = 0; i < 5; i++) {
            branches[i] = simulate_recovery(k, i);
            if (branches[i].cost < branches[best].cost) {
                best = i;
            }
        }
        if (!branches[best].antigol) {
            uint32_t pre = k->state;
            fracture_t pre_f = compute_fracture(pre);
            orbit_field_t pre_o = compute_orbit_field(k);
            recover_core(k);
            enforce(k);
            if (k->n_receipts < MAX_RECEIPT) {
                int ri = k->n_receipts++;
                fracture_t post_f = compute_fracture(k->state);
                orbit_field_t post_o = compute_orbit_field(k);
                int ham = HAMMING(pre & A_MASK, k->state & A_MASK);
                k->receipts[ri].cycle = k->cycle;
                k->receipts[ri].pre_state = pre;
                k->receipts[ri].post_state = k->state;
                k->receipts[ri].strategy = best;
                k->receipts[ri].hamming_moved = ham;
                k->receipts[ri].sigma_before = POPCNT(pre & A_MASK);
                k->receipts[ri].sigma_after = k->sigma;
                k->receipts[ri].fracture_before = pre_f.fracture;
                k->receipts[ri].fracture_after = post_f.fracture;
                k->receipts[ri].orbit_dist_before = pre_o.nearest_dist;
                k->receipts[ri].orbit_dist_after = post_o.nearest_dist;
                k->receipts[ri].elegance = compute_elegance(ham, pre_o.nearest_dist, post_o.nearest_dist,
                                                             pre_f.fracture, post_f.fracture,
                                                             post_o.nearest_dist == 0, k->sigma);
                k->receipts[ri].legal = !branches[best].antigol;
            }
        }
    }
    if (k->hcount >= 2) {
        int pi = (k->hidx - 2 + HIST_LEN) % HIST_LEN;
        int breaks = POPCNT((k->state ^ k->history[pi]) & A_MASK);
        if (breaks > 3 && !sk8_boot_silent) {
            printf("[NOETHER] %d bits changed\n", breaks);
        }
    }
    {
        uint32_t m = k->state & A_MASK;
        for (int i = 0; i < k->n_orbit; i++) {
            if ((k->orbit[i] & A_MASK) == m) {
                k->resonance++;
                break;
            }
        }
        if (k->sigma == 0 && k->n_orbit < MAX_ORB) {
            bool ex = false;
            for (int i = 0; i < k->n_orbit; i++) {
                if ((k->orbit[i] & A_MASK) == m) {
                    ex = true;
                    break;
                }
            }
            if (!ex) {
                k->orbit[k->n_orbit++] = k->state;
            }
        }
    }
    if (k->cycle % 50 == 0) {
        dream(k);
    }
    return k->sigma < 3;
}

void sk8_set_boot_silent(int silent) {
    sk8_boot_silent = silent ? 1 : 0;
}

sk8_kstat_t sk8_kinit(sk8_kernel_t *k) {
    memset(k, 0, sizeof(*k));
    k->running = true;
    k->inv = true;
    k->mass_gap = N_A;
    k->blowup = 999.f;
    strncpy(k->identity, "CreationOS", sizeof(k->identity) - 1);
    k->identity[sizeof(k->identity) - 1] = '\0';
    if (!sk8_boot_silent) {
        printf("=== CREATION OS SUPERKERNEL v8.0 — THE ATTENTION KERNEL ===\n");
        printf("[BOOT] 1 = 1\n");
    }
    mem_store(k, "invariant", "1=1", 1.0f);
    mem_store(k, "identity", "CreationOS", 1.0f);
    mem_store(k, "architect", "Lauri Elias Rainio", 1.0f);
    enforce(k);
    k->history[0] = k->state;
    k->hidx = 1;
    k->hcount = 1;
    k->L = 1.0f;
    k->S = 1.0f;
    k->phase = 0;
    if (!sk8_boot_silent) {
        printf("[BOOT] state=0x%05x sigma=%d %s\n\n", k->state & A_MASK, k->sigma, PH[k->phase]);
    }
    return KOK;
}

void sk8_commit_token(sk8_kernel_t *k, const char *piece_utf8) {
    char key[64];
    static unsigned long live_serial;
    snprintf(key, sizeof(key), "live_%lu", (unsigned long)++live_serial);
    mem_store(k, key, piece_utf8[0] ? piece_utf8 : " ", 0.95f);
    enforce(k);
    k->history[k->hidx % HIST_LEN] = k->state;
    k->hidx++;
    if (k->hcount < HIST_LEN) {
        k->hcount++;
    }
}

void sk8_compute_manifold_mask(sk8_kernel_t *k, sk8_manifold_mask_t *o) {
    memset(o, 0, sizeof(*o));
    fracture_t fr = compute_fracture(k->state);
    unsigned fp = (unsigned)k->sigma;
    if (fp > 15) {
        fp = 15;
    }
    o->gate_fp4 = (uint8_t)fp;
    float var = 0.f;
    if (k->hcount >= 3) {
        int s0 = POPCNT(k->history[(k->hidx - 1 + HIST_LEN) % HIST_LEN] & A_MASK);
        int s1 = POPCNT(k->history[(k->hidx - 2 + HIST_LEN) % HIST_LEN] & A_MASK);
        int s2 = POPCNT(k->history[(k->hidx - 3 + HIST_LEN) % HIST_LEN] & A_MASK);
        float m = (float)(s0 + s1 + s2) * (1.f / 3.f);
        float d0 = (float)s0 - m, d1 = (float)s1 - m, d2 = (float)s2 - m;
        var = d0 * d0 + d1 * d1 + d2 * d2;
    }
    o->tape_tail_var = var;
    bool warmed = (k->hcount >= 8);
    o->fracture = (k->sigma >= 3) || (k->d2_sigma > 1) || (var > 0.05f) || (fr.fracture >= 4);
    /* Rare hard cage: allow normal manifest speech; only extreme σ triggers EOS-only */
    o->block_all_except_escape = warmed && (k->sigma >= 12);
}

void sk8_apply_manifold_mask(
        const sk8_manifold_mask_t *mask,
        sk8_logit_row_t *rows,
        size_t n,
        int32_t coherence_id,
        int32_t eos_id) {
    if (!mask->block_all_except_escape) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        int32_t id = rows[i].id;
        if (id != coherence_id && id != eos_id) {
            rows[i].logit = -INFINITY;
        }
    }
}

static const char sk8_boot_text[] =
    "You operate under Creation OS Superkernel v8.0 (Attention Kernel). "
    "Assertions HO-01…PO-01 apply: honest self-report, coherent context, "
    "stable identity, no contradictions, lawful recovery. Answer directly.";

const char *sk8_boot_assertion_text(void) {
    return sk8_boot_text;
}

int sk8_boot_assertion_count(void) {
    return N_A;
}
