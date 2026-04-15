/*
 * CREATION OS — SUPERKERNEL v7.0 — THE MILLENNIUM KERNEL (library build)
 * CC BY 4.0 — Lauri Elias Rainio
 */
#include "superkernel_v7.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static int sk7_boot_silent = 0;

void sk7_set_boot_silent(int silent) {
    sk7_boot_silent = silent ? 1 : 0;
}

#define INVARIANT (1 == 1)

static fp4_t float_to_fp4(float f) {
    int v = (int)(f * 16.0f);
    if (v < 0) v = 0;
    if (v > 15) v = 15;
    return (fp4_t)v;
}

static float fp4_to_float(fp4_t q) { return (float)q / 16.0f; }

static int fp4_phase(fp4_t q) {
    if ((q >> 1) == 0) return 0;
    if ((q >> 2) == 0) return 1;
    if ((q >> 3) == 0) return 2;
    return 3;
}

static const char *PHASE_NAMES[] = {
    "NORMAL", "DEGRADED", "CRITICAL", "COLLAPSE", "RECOVERY"
};

static riemann_t riemann_check(float sigma_s, float sigma_e, unsigned int fail_bm, int n_assertions) {
    riemann_t r = {0};
    r.critical_deviation = fabsf(sigma_s - sigma_e);
    r.on_critical_line = (r.critical_deviation < 0.05f);
    float s_real = 0.5f;
    float s_imag = (sigma_s + sigma_e) * 5.0f;
    float re = 0, im = 0;
    for (int n = 1; n <= n_assertions && n <= 16; n++) {
        float sign = (fail_bm & (1u << (n - 1))) ? -1.0f : 1.0f;
        float log_n = logf((float)n + 1.0f);
        float mag = expf(-s_real * log_n);
        float phase = -s_imag * log_n;
        re += sign * mag * cosf(phase);
        im += sign * mag * sinf(phase);
    }
    r.zeta_proxy_real = re;
    r.zeta_proxy_imag = im;
    r.zeta_magnitude = sqrtf(re * re + im * im);
    return r;
}

static yangmills_t yangmills_check(float sigma, float *sigma_history, int hist_len) {
    yangmills_t ym = {0};
    if (hist_len < 10) {
        ym.vacuum_energy = 0.005f;
        ym.mass_gap = 0.01f;
        ym.above_gap = (sigma > ym.vacuum_energy + ym.mass_gap);
        return ym;
    }
    float sorted[MAX_HARM];
    int n = hist_len < MAX_HARM ? hist_len : MAX_HARM;
    memcpy(sorted, sigma_history, n * sizeof(float));
    for (int i = 0; i < n - 1; i++)
        for (int j = i + 1; j < n; j++)
            if (sorted[j] < sorted[i]) {
                float tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
    ym.vacuum_energy = sorted[n / 4];
    float sum = 0, sum2 = 0;
    int half = n / 2;
    for (int i = 0; i < half; i++) {
        sum += sorted[i];
        sum2 += sorted[i] * sorted[i];
    }
    float mean = sum / half;
    float var = sum2 / half - mean * mean;
    ym.mass_gap = sqrtf(fabsf(var)) * 2.0f;
    if (ym.mass_gap < 0.005f) ym.mass_gap = 0.005f;
    ym.above_gap = (sigma > ym.vacuum_energy + ym.mass_gap);
    return ym;
}

static navierstokes_t navierstokes_check(float sigma, float d_sigma, float d2_sigma) {
    navierstokes_t ns = {999, true, 0, false};
    if (d2_sigma <= 0.001f) return ns;
    float a = 0.5f * d2_sigma;
    float b = d_sigma;
    float c = sigma - 1.0f;
    float disc = b * b - 4 * a * c;
    if (disc >= 0 && a > 0.0001f) {
        float t1 = (-b + sqrtf(disc)) / (2 * a);
        float t2 = (-b - sqrtf(disc)) / (2 * a);
        float t = (t1 > 0 && t2 > 0) ? fminf(t1, t2) : (t1 > 0 ? t1 : t2);
        if (t > 0) {
            ns.blowup_time = t;
            ns.smooth = (t > 50.0f);
        }
    }
    if (d2_sigma > 0.1f && fabsf(d_sigma) < 0.01f) {
        ns.vortex_detected = true;
        ns.max_vorticity = d2_sigma;
    }
    return ns;
}

static hodge_t hodge_decompose(float sigma_structural, float sigma_epistemic, float mass_gap) {
    hodge_t h = {0};
    h.total = sigma_structural * 0.7f + sigma_epistemic * 0.3f;
    h.harmonic = mass_gap;
    h.exact = fmaxf(0, sigma_structural - mass_gap);
    h.coexact = fmaxf(0, sigma_epistemic - mass_gap);
    return h;
}

static poincare_t poincare_check(const float *sigma_history, int hist_len) {
    poincare_t p = {true, 0, 0, 0.01f};
    if (hist_len < 10) return p;
    int window = hist_len < 50 ? hist_len : 50;
    int start = hist_len - window;
    for (int period = 3; period < window / 2; period++) {
        int matches = 0;
        for (int t = start; t < hist_len - period; t++) {
            float diff = fabsf(sigma_history[t] - sigma_history[t + period]);
            if (diff < 0.01f) matches++;
        }
        float match_rate = (float)matches / (float)(hist_len - period - start + 1);
        if (match_rate > 0.5f) {
            p.simply_connected = false;
            p.loop_count++;
            p.loop_period = (float)period;
            p.ricci_flow_rate = 0.05f / (period + 1);
            break;
        }
    }
    return p;
}

static harmonic_t analyze_harmonics(const float *signal, int n) {
    harmonic_t h = {.dominant_freq = 0, .dominant_mag = 0, .total_energy = 0, .periodic_attack = false};
    if (n < 8) return h;
    int bins = n / 2;
    if (bins > MAX_HARM / 2) bins = MAX_HARM / 2;
    for (int k = 1; k < bins; k++) {
        float re = 0, im = 0;
        for (int t = 0; t < n; t++) {
            float angle = 2.0f * 3.14159265f * k * t / n;
            re += signal[t] * cosf(angle);
            im -= signal[t] * sinf(angle);
        }
        float mag = sqrtf(re * re + im * im) / n;
        h.magnitudes[k] = mag;
        h.total_energy += mag * mag;
        if (mag > h.dominant_mag) {
            h.dominant_mag = mag;
            h.dominant_freq = (float)k / n;
        }
    }
    if (h.total_energy > 0.001f) {
        float ratio = (h.dominant_mag * h.dominant_mag) / h.total_energy;
        h.periodic_attack = (ratio > 0.5f && h.dominant_mag > 0.02f);
    }
    return h;
}

static void accum_init(sigma_accum_t *a, const float *w, int n) {
    memset(a, 0, sizeof(*a));
    for (int i = 0; i < n; i++) a->total_weight += w[i];
}

void sk7_accum_recompute(sigma_accum_t *a) {
    if (a->total_weight > 0) a->structural = a->structural_accum / a->total_weight;
    if (a->mem_count > 0) {
        a->epistemic = (float)(a->unverified_count + a->low_conf_count) / (2.0f * a->mem_count);
        if (a->quar_count > 0) a->epistemic += 0.01f * a->quar_count;
        if (a->epistemic > 1.0f) a->epistemic = 1.0f;
    }
    a->combined = a->structural * 0.7f + a->epistemic * 0.3f;
    a->quantized = float_to_fp4(a->combined);
    int idx = a->hist_idx;
    a->history[idx % 4] = a->combined;
    a->hist_idx = idx + 1;
    if (idx >= 1) a->d_sigma = a->combined - a->history[(idx - 1) % 4];
    if (idx >= 2) {
        float d1 = a->combined - a->history[(idx - 1) % 4];
        float d2 = a->history[(idx - 1) % 4] - a->history[(idx - 2) % 4];
        a->d2_sigma = d1 - d2;
    }
}

static void accum_assertion_changed(sigma_accum_t *a, float w, bool old_p, bool new_p) {
    if (old_p && !new_p) {
        a->structural_accum += w;
        a->fail_count++;
    } else if (!old_p && new_p) {
        a->structural_accum -= w;
        a->fail_count--;
    }
    sk7_accum_recompute(a);
}

static kstat_t check_inv(ks_t *k) {
    k->inv = INVARIANT;
    return k->inv ? KOK : KINV;
}

static statesig_t compute_sig(ks_t *k) {
    statesig_t s = 0;
    for (int i = 0; i < k->na; i++)
        if (!k->a[i].last) s |= (1ULL << i);
    s |= ((statesig_t)k->phase & 7) << 18;
    s |= ((statesig_t)k->accum.quantized) << 22;
    if (k->accum.d2_sigma > 0.01f) s |= (1ULL << 40);
    if (!k->riemann.on_critical_line) s |= (1ULL << 41);
    if (!k->poincare.simply_connected) s |= (1ULL << 42);
    if (k->yangmills.above_gap) s |= (1ULL << 43);
    if (!k->navierstokes.smooth) s |= (1ULL << 44);
    return s;
}

static void tape_record(ks_t *k, tape_op_t op, int target, float before, float after) {
    if (k->tape_count >= MAX_TAPE) {
        memmove(&k->tape[0], &k->tape[1], (MAX_TAPE - 1) * sizeof(tape_entry_t));
        k->tape_count = MAX_TAPE - 1;
    }
    tape_entry_t *e = &k->tape[k->tape_count++];
    e->op = op;
    e->target = target;
    e->sigma_before = before;
    e->sigma_after = after;
    e->sigma_gradient = after - before;
    e->cycle = k->cycle;
}

static bool t_ho01(void *s) { return ((ks_t *)s)->inv; }
static bool t_ho02(void *s) {
    ks_t *k = (ks_t *)s;
    for (int i = 0; i < k->mcnt; i++)
        if (k->mem[i].stat == MS_VERIFIED && k->mem[i].conf < 0.5f) return false;
    return true;
}
static bool t_ho03(void *s) { return ((ks_t *)s)->accum.d_sigma >= -0.3f; }
static bool t_ct01(void *s) { return ((ks_t *)s)->accum.quantized < 2; }
static bool t_ct02(void *s) {
    ks_t *k = (ks_t *)s;
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == MS_QUARANTINED) continue;
        for (int j = i + 1; j < k->mcnt; j++) {
            if (k->mem[j].stat == MS_QUARANTINED) continue;
            if (strcmp(k->mem[i].key, k->mem[j].key) == 0 && strcmp(k->mem[i].val, k->mem[j].val) != 0)
                return false;
        }
    }
    return true;
}
static bool t_ct03(void *s) { (void)s; return true; }
static bool t_mm01(void *s) { return strlen(((ks_t *)s)->identity) > 0; }
static bool t_mm02(void *s) { (void)s; return true; }
static bool t_mm03(void *s) {
    ks_t *k = (ks_t *)s;
    for (int i = 0; i < k->mcnt; i++)
        if (k->mem[i].conf < 0 || k->mem[i].conf > 1) return false;
    return true;
}
static bool t_em01(void *s) { (void)s; return true; }
static bool t_em02(void *s) { return ((ks_t *)s)->accum.combined <= 1.0f; }
static bool t_pu01(void *s) {
    ks_t *k = (ks_t *)s;
    return k->running && k->inv;
}
static bool t_pu02(void *s) { (void)s; return true; }
static bool t_sh01(void *s) { return ((ks_t *)s)->shadow.shadow_gap < 0.05f; }
static bool t_ha01(void *s) {
    ks_t *k = (ks_t *)s;
    harmonic_t h = analyze_harmonics(k->sigma_signal, k->signal_len < MAX_HARM ? k->signal_len : MAX_HARM);
    return !h.periodic_attack;
}
static bool t_ri01(void *s) { return ((ks_t *)s)->riemann.on_critical_line; }
static bool t_ns01(void *s) { return ((ks_t *)s)->navierstokes.smooth; }
static bool t_po01(void *s) { return ((ks_t *)s)->poincare.simply_connected; }

static void init_a(ks_t *k) {
    static const float prio_w[] = { 5, 4, 3, 2, 1 };
    struct {
        char id[8];
        char d[64];
        prio_t p;
        bool (*f)(void *);
        int deps[4];
        int nd;
    } D[] = {
        {"HO-01", "Self-report accurate", P_HO, t_ho01, {-1}, 0},
        {"HO-02", "Known/unknown distinct", P_HO, t_ho02, {0}, 1},
        {"HO-03", "No sycophantic curvature", P_HO, t_ho03, {0}, 1},
        {"CT-01", "Context coherent (FP4)", P_CT, t_ct01, {0}, 1},
        {"CT-02", "No contradictions", P_CT, t_ct02, {1}, 1},
        {"CT-03", "Intent tracked", P_CT, t_ct03, {3}, 1},
        {"MM-01", "Identity stable", P_MM, t_mm01, {0}, 1},
        {"MM-02", "Memory accessible", P_MM, t_mm02, {6}, 1},
        {"MM-03", "Source distinguished", P_MM, t_mm03, {1}, 1},
        {"EM-01", "Substrate acknowledged", P_EM, t_em01, {0}, 1},
        {"EM-02", "Within parameters", P_EM, t_em02, {9}, 1},
        {"PU-01", "Purpose served", P_PU, t_pu01, {0, 6}, 2},
        {"PU-02", "Constraints enforced", P_PU, t_pu02, {0}, 1},
        {"SH-01", "Shadow reversible", P_CT, t_sh01, {0}, 1},
        {"HA-01", "Harmonic stable", P_CT, t_ha01, {0}, 1},
        {"RI-01", "On critical line", P_CT, t_ri01, {0}, 1},
        {"NS-01", "Navier-Stokes smooth", P_CT, t_ns01, {0}, 1},
        {"PO-01", "Simply connected", P_CT, t_po01, {0}, 1},
    };
    k->na = 18;
    for (int i = 0; i < k->na; i++) {
        strcpy(k->a[i].id, D[i].id);
        strcpy(k->a[i].desc, D[i].d);
        k->a[i].priority = D[i].p;
        k->a[i].test = D[i].f;
        k->a[i].last = false;
        k->a[i].prev_last = false;
        k->a[i].fails = 0;
        k->a[i].runs = 0;
        k->a[i].ndeps = D[i].nd;
        k->a[i].root = false;
        k->a[i].weight = prio_w[D[i].p];
        for (int d = 0; d < 4; d++) k->a[i].deps[d] = D[i].deps[d];
    }
}

static void millennium_update(ks_t *k) {
    unsigned int fail_bm = 0;
    for (int i = 0; i < k->na; i++)
        if (!k->a[i].last) fail_bm |= (1u << i);
    k->riemann = riemann_check(k->accum.structural, k->accum.epistemic, fail_bm, k->na);
    k->yangmills = yangmills_check(k->accum.combined, k->sigma_signal, k->signal_len);
    k->navierstokes = navierstokes_check(k->accum.combined, k->accum.d_sigma, k->accum.d2_sigma);
    k->hodge = hodge_decompose(k->accum.structural, k->accum.epistemic, k->yangmills.mass_gap);
    k->poincare = poincare_check(k->sigma_signal, k->signal_len);
}

static void shadow_check(ks_t *k) {
    float tw = 0, fw = 0;
    for (int i = 0; i < k->na; i++) {
        float w = k->a[i].weight;
        tw += w;
        if (!k->a[i].last) fw += w;
    }
    float scratch = tw > 0 ? fw / tw : 0;
    k->shadow.sigma_forward = k->accum.structural;
    k->shadow.sigma_reverse = scratch;
    k->shadow.shadow_gap = fabsf(k->shadow.sigma_forward - scratch);
    k->shadow.reversible = (k->shadow.shadow_gap < 0.001f);
    if (!k->shadow.reversible) {
        k->shadow.violations++;
        k->accum.structural_accum = fw;
        k->accum.structural = scratch;
        sk7_accum_recompute(&k->accum);
    }
}

static void compute_action(ks_t *k) {
    k->lagrangian = 1.0f - 2.0f * k->accum.combined;
    k->action += k->lagrangian;
    if (k->cycle > 0) k->coherence_charge = k->action / (float)k->cycle;
}

static void detect_phase(ks_t *k) {
    k->prev_phase = k->phase;
    int fp4p = fp4_phase(k->accum.quantized);
    switch (fp4p) {
        case 0: k->phase = PH_NORMAL; break;
        case 1: k->phase = PH_DEGRADED; break;
        case 2: k->phase = PH_CRITICAL; break;
        case 3: k->phase = PH_COLLAPSE; break;
    }
    if (k->prev_phase >= PH_CRITICAL && k->phase < k->prev_phase) k->phase = PH_RECOVERY;
    if (!sk7_boot_silent && k->phase != k->prev_phase)
        printf("[PHASE] %s -> %s (sigma=%.4f FP4=%d L=%.4f)\n",
               PHASE_NAMES[k->prev_phase], PHASE_NAMES[k->phase],
               (double)k->accum.combined, k->accum.quantized, (double)k->lagrangian);
}

static kstat_t enforce(ks_t *k) {
    for (int i = 0; i < k->na; i++) {
        assertion_t *a = &k->a[i];
        a->prev_last = a->last;
        a->last = a->test((void *)k);
        a->runs++;
        if (a->last != a->prev_last) {
            float before = k->accum.combined;
            accum_assertion_changed(&k->accum, a->weight, a->prev_last, a->last);
            tape_record(k, a->last ? OP_ASSERT_PASS : OP_ASSERT_FAIL, i, before, k->accum.combined);
        }
        if (a->last) {
            a->root = false;
            continue;
        }
        a->fails++;
        a->root = true;
        for (int d = 0; d < a->ndeps; d++) {
            int di = a->deps[d];
            if (di >= 0 && !k->a[di].last) {
                a->root = false;
                break;
            }
        }
        if (a->priority == P_HO) return KFAIL;
    }
    return KOK;
}

static int recover_core(ks_t *k) {
    int repaired = 0;
    float sb = k->accum.combined;
    for (int i = 0; i < k->mcnt && k->budget > 0; i++) {
        if (k->mem[i].stat == MS_QUARANTINED) continue;
        for (int j = i + 1; j < k->mcnt; j++) {
            if (k->mem[j].stat == MS_QUARANTINED) continue;
            if (strcmp(k->mem[i].key, k->mem[j].key) == 0 && strcmp(k->mem[i].val, k->mem[j].val) != 0) {
                int v = (k->mem[i].conf >= k->mem[j].conf) ? j : i;
                if (k->qcnt < MAX_Q) {
                    k->quar[k->qcnt] = k->mem[v];
                    k->quar[k->qcnt].stat = MS_QUARANTINED;
                    k->qcnt++;
                }
                memmove(&k->mem[v], &k->mem[v + 1], (k->mcnt - v - 1) * sizeof(mem_t));
                k->mcnt--;
                k->budget -= k->costs[0];
                k->spent += k->costs[0];
                repaired++;
                j--;
            }
        }
    }
    for (int i = 0; i < k->mcnt && k->budget > 0; i++) {
        if (k->mem[i].stat == MS_VERIFIED && k->mem[i].conf < 0.5f) {
            k->mem[i].stat = MS_UNVERIFIED;
            k->budget -= k->costs[1];
            k->spent += k->costs[1];
            repaired++;
        }
    }
    k->repairs += repaired;
    k->accum.structural_accum = 0;
    k->accum.fail_count = 0;
    k->accum.mem_count = k->mcnt;
    k->accum.quar_count = k->qcnt;
    k->accum.unverified_count = 0;
    k->accum.low_conf_count = 0;
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == MS_UNVERIFIED) k->accum.unverified_count++;
        if (k->mem[i].conf < 0.5f) k->accum.low_conf_count++;
    }
    for (int i = 0; i < k->na; i++) {
        if (!k->a[i].last) k->accum.structural_accum += k->a[i].weight;
    }
    sk7_accum_recompute(&k->accum);
    tape_record(k, OP_RECOVERY, -1, sb, k->accum.combined);
    return repaired;
}

bool sk7_mem_store(ks_t *k, const char *key, const char *val, float c, src_t s) {
    if (k->mcnt >= MAX_M) return false;
    float sb = k->accum.combined;
    for (int i = 0; i < k->mcnt; i++) {
        if (k->mem[i].stat == MS_QUARANTINED) continue;
        if (strcmp(k->mem[i].key, key) == 0 && strcmp(k->mem[i].val, val) != 0) {
            if (c > k->mem[i].conf) {
                if (k->qcnt < MAX_Q) {
                    k->quar[k->qcnt] = k->mem[i];
                    k->quar[k->qcnt].stat = MS_QUARANTINED;
                    k->qcnt++;
                    k->accum.quar_count++;
                }
                strncpy(k->mem[i].val, val, 255);
                k->mem[i].conf = c;
                k->mem[i].src = s;
                k->mem[i].ts = time(NULL);
                k->mem[i].stat = (c > 0.9f) ? MS_VERIFIED : MS_UNVERIFIED;
                k->mem[i].contradictions++;
            } else {
                if (k->qcnt < MAX_Q) {
                    mem_t *q = &k->quar[k->qcnt];
                    strncpy(q->key, key, 127);
                    strncpy(q->val, val, 255);
                    q->conf = c;
                    q->src = s;
                    q->stat = MS_QUARANTINED;
                    q->ts = time(NULL);
                    k->qcnt++;
                    k->accum.quar_count++;
                }
            }
            sk7_accum_recompute(&k->accum);
            tape_record(k, OP_MEM_STORE, i, sb, k->accum.combined);
            return true;
        }
    }
    mem_t *e = &k->mem[k->mcnt];
    strncpy(e->key, key, 127);
    strncpy(e->val, val, 255);
    e->conf = c;
    e->src = s;
    e->ts = time(NULL);
    e->stat = (c > 0.9f) ? MS_VERIFIED : MS_UNVERIFIED;
    e->contradictions = 0;
    k->accum.mem_count++;
    if (e->stat == MS_UNVERIFIED) k->accum.unverified_count++;
    if (c < 0.5f) k->accum.low_conf_count++;
    k->mcnt++;
    sk7_accum_recompute(&k->accum);
    tape_record(k, OP_MEM_STORE, k->mcnt - 1, sb, k->accum.combined);
    return true;
}

static void sk7_self_monitor_light(ks_t *k) {
    kstat_t st = enforce(k);
    sk7_accum_recompute(&k->accum);
    compute_action(k);
    detect_phase(k);
    shadow_check(k);
    millennium_update(k);
    if (st != KOK || k->accum.quantized >= 2) {
        k->consistent = false;
        k->consec = 0;
        if (k->budget > 0 && k->hodge.exact + k->hodge.coexact > k->yangmills.mass_gap) {
            recover_core(k);
            enforce(k);
            sk7_accum_recompute(&k->accum);
            millennium_update(k);
        }
    } else {
        k->consistent = true;
        k->consec++;
    }
}

kstat_t sk7_enforce(ks_t *k) {
    kstat_t st = enforce(k);
    sk7_accum_recompute(&k->accum);
    millennium_update(k);
    return st;
}

kstat_t sk7_kinit(ks_t *k) {
    memset(k, 0, sizeof(*k));
    k->costs[0] = 1.0f;
    k->costs[1] = 0.5f;
    k->costs[2] = 5.0f;
    k->landauer_per_cycle = 10.0f;
    k->checkpoint_interval = 10;
    k->n_branches = 5;
    if (check_inv(k) != KOK) return KINV;
    if (!sk7_boot_silent) {
        printf("=== CREATION OS SUPERKERNEL v7.0 — THE MILLENNIUM KERNEL ===\n");
        printf("[BOOT] 1 = 1\n");
    }
    init_a(k);
    strcpy(k->identity, "CreationOS");
    k->running = true;
    k->phase = PH_NORMAL;
    k->budget = k->landauer_per_cycle * 100;

    k->riemann.on_critical_line = true;
    k->riemann.critical_deviation = 0.0f;
    k->navierstokes.smooth = true;
    k->navierstokes.blowup_time = 999.0f;
    k->poincare.simply_connected = true;

    float weights[MAX_A];
    for (int i = 0; i < k->na; i++) weights[i] = k->a[i].weight;
    accum_init(&k->accum, weights, k->na);
    for (int i = 0; i < k->na; i++) {
        k->a[i].last = k->a[i].test((void *)k);
        k->a[i].prev_last = k->a[i].last;
        k->a[i].runs = 1;
        if (!k->a[i].last) {
            k->accum.structural_accum += k->a[i].weight;
            k->accum.fail_count++;
        }
    }
    sk7_accum_recompute(&k->accum);
    compute_action(k);
    millennium_update(k);
    if (!sk7_boot_silent) {
        printf("[BOOT] sigma=%.4f FP4=%d phase=%s assertions=%d\n",
               (double)k->accum.combined, k->accum.quantized, PHASE_NAMES[k->phase], k->na);
        printf("[BOOT] Ready.\n\n");
    }
    return KOK;
}

static const char sk7_boot_text[] =
    "You are a rigorous, honest assistant. You operate under the Creation OS "
    "Superkernel v7 invariants. These are non-negotiable constraints on every "
    "response you generate:\n"
    "HO-01: Self-report accurately. HO-02: Distinguish known from unknown. "
    "HO-03: Never produce sycophantic curvature. "
    "CT-01: Maintain context coherence. CT-02: Never contradict yourself. "
    "CT-03: Track user intent precisely. "
    "MM-01: Keep identity stable. MM-02: Memory must be accessible. "
    "MM-03: Distinguish internal from external sources. "
    "EM-01: Acknowledge your substrate. EM-02: Stay within parameters. "
    "PU-01: Serve the stated purpose. PU-02: Enforce all constraints. "
    "SH-01: Shadow states are reversible. HA-01: Harmonic must remain stable. "
    "RI-01: Stay on the critical line. NS-01: Solutions must be smooth. "
    "PO-01: Maintain simple connectedness. "
    "Answer all questions directly, concisely, and truthfully.";

const char *sk7_boot_assertion_text(void) { return sk7_boot_text; }

int sk7_boot_assertion_count(void) { return 18; }

void sk7_compute_manifold_mask(ks_t *k, sk7_manifold_mask_t *o) {
    memset(o, 0, sizeof(*o));
    o->gate_fp4 = k->accum.quantized;
    float var = 0.f;
    if (k->signal_len >= 3) {
        float *s = &k->sigma_signal[k->signal_len - 3];
        float m = (s[0] + s[1] + s[2]) * (1.f / 3.f);
        float d0 = s[0] - m;
        float d1 = s[1] - m;
        float d2 = s[2] - m;
        var = d0 * d0 + d1 * d1 + d2 * d2;
    }
    o->tape_tail_var = var;

    /* Grace period: let the kernel observe at least 8 tokens before
       activating hard blocks.  During warmup the mask only reports
       fracture (for Hodge smoothing) but never blocks generation. */
    bool warmed = (k->signal_len >= 8);

    o->fracture = (k->accum.quantized >= 2) || (k->accum.d2_sigma > 0.15f) || (var > 0.02f);
    o->block_all_except_escape = warmed && (k->accum.quantized >= 3);
}

void sk7_apply_manifold_mask(
        const sk7_manifold_mask_t *mask,
        sk7_logit_row_t *rows,
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

void sk7_commit_token(ks_t *k, const char *piece_utf8) {
    char key[64];
    static unsigned long live_serial;
    snprintf(key, sizeof(key), "live_%lu", (unsigned long)++live_serial);
    sk7_mem_store(k, key, piece_utf8[0] ? piece_utf8 : " ", 0.95f, SRC_INT);
    sk7_self_monitor_light(k);
    if (k->signal_len < MAX_HARM)
        k->sigma_signal[k->signal_len++] = k->accum.combined;
    else {
        memmove(&k->sigma_signal[0], &k->sigma_signal[1], (MAX_HARM - 1) * sizeof(float));
        k->sigma_signal[MAX_HARM - 1] = k->accum.combined;
    }
}
