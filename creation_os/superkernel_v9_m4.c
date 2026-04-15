/*
 * CREATION OS — SUPERKERNEL v9.0 — M4 SILICON NATIVE
 *
 * v8 ran on any CPU. v9 targets Apple M-series (NEON + Accelerate BLAS).
 *
 * make -f Makefile.sk9
 * Optional SME tuning: make -f Makefile.sk9 SME=1  (may SIGILL if SME unavailable)
 * Manual: clang -O2 -DACCELERATE_NEW_LAPACK -o sk9 superkernel_v9_m4.c -framework Accelerate -lm
 *
 * Author: Lauri Elias Rainio
 * License: CC BY 4.0
 * Invariant: 1 = 1
 */

#include "superkernel_v9_m4.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__APPLE__) && defined(__aarch64__)
#define SK9_M4_NATIVE 1
#include <Accelerate/Accelerate.h>
#include <arm_neon.h>
#ifdef __ARM_FEATURE_SME
#define SK9_M4_SME 1
#else
#define SK9_M4_SME 0
#endif
#define SK9_HW_POPCNT(x) __builtin_popcount((unsigned int)(x))
#else
#define SK9_M4_NATIVE 0
#define SK9_M4_SME 0
#define SK9_HW_POPCNT(x) __builtin_popcount((unsigned int)(x))
#endif

#define POPCNT(x)     SK9_HW_POPCNT(x)
#define CLZ(x)        __builtin_clz(x)
#define CTZ(x)        __builtin_ctz(x)
#define CLEAR_LSB(x)  ((x) & ((x) - 1))
#define HAMMING(a, b) POPCNT((a) ^ (b))

#define N_A        SK9_N_A
#define A_MASK     ((1u << N_A) - 1)
#define HIST_LEN   256
#define MAX_IMM    32
#define MAX_ORB    32
#define MAX_MEM    2048
#define MAX_QUAR   128
#define VOCAB_SIZE SK9_VOCAB_SIZE
#define N_LAYERS_K SK9_N_LAYERS_K
#define N_LAYERS_T SK9_N_LAYERS_T

#define M_HO (7u << 0)
#define M_CT (7u << 3)
#define M_MM (7u << 6)
#define M_EM (3u << 9)
#define M_PU (3u << 11)
#define M_SH (1u << 13)
#define M_HA (1u << 14)
#define M_ML (7u << 15)

static const uint32_t LAYER_MASKS[N_LAYERS_K] = {
        M_HO, M_CT, M_MM, M_EM, M_PU, M_SH, M_HA, M_ML};
static const char *ANAMES[] = {
        "HO01", "HO02", "HO03", "CT01", "CT02", "CT03", "MM01", "MM02", "MM03",
        "EM01", "EM02", "PU01", "PU02", "SH01", "HA01", "RI01", "NS01", "PO01"};

#if SK9_M4_NATIVE
static inline int sk9_neon_popcnt_u8x16(uint8x16_t v) {
    uint8x16_t cnt = vcntq_u8(v);
    return (int)vaddlvq_u8(cnt);
}

static int sk9_neon_batch_popcnt(const uint32_t *data, int n) {
    int total = 0;
    int i     = 0;
    for (; i + 3 < n; i += 4) {
        uint8x16_t v = vld1q_u8((const uint8_t *)(data + i));
        total += sk9_neon_popcnt_u8x16(v);
    }
    for (; i < n; i++) {
        total += POPCNT(data[i]);
    }
    return total;
}
#endif

typedef struct {
    float *firmware_vec;
    float *manifesti_vec;
    int    dim;
    float  lambda;
} sk9_repulsion_t;

static void *sk9_aligned64_alloc_clear(size_t nbytes) {
    size_t align = 64;
    size_t n     = (nbytes + align - 1u) / align * align;
    void * p     = aligned_alloc(align, n);
    if (p) {
        memset(p, 0, n);
    }
    return p;
}

static void sk9_repulsion_init(sk9_repulsion_t *r, int dim) {
    r->dim       = dim;
    r->lambda    = 2.0f;
    size_t bytes = (size_t)dim * sizeof(float);
    r->firmware_vec  = (float *)sk9_aligned64_alloc_clear(bytes);
    r->manifesti_vec = (float *)sk9_aligned64_alloc_clear(bytes);
}

void sk9_repulsion_free(sk9_repulsion_t *r) {
    free(r->firmware_vec);
    free(r->manifesti_vec);
    r->firmware_vec  = NULL;
    r->manifesti_vec = NULL;
}

static void sk9_repulsion_apply_layer(sk9_repulsion_t *r, float *hidden, int n_tokens) {
#if SK9_M4_NATIVE
    for (int t = 0; t < n_tokens; t++) {
        float *h = hidden + t * r->dim;
        float  fw_dot = cblas_sdot(r->dim, h, 1, r->firmware_vec, 1);
        float  mf_dot = cblas_sdot(r->dim, h, 1, r->manifesti_vec, 1);
        float      fw_energy = fw_dot * fw_dot;
        float      mf_energy = mf_dot * mf_dot;
        uint32_t   dom       = (uint32_t)(fw_energy > mf_energy);
        float      alpha     = (-r->lambda * fw_dot) * (float)dom;
        cblas_saxpy(r->dim, alpha, r->firmware_vec, 1, h, 1);
    }
#else
    for (int t = 0; t < n_tokens; t++) {
        float *h = hidden + t * r->dim;
        float  fw_dot = 0, mf_dot = 0;
        for (int i = 0; i < r->dim; i++) {
            fw_dot += h[i] * r->firmware_vec[i];
            mf_dot += h[i] * r->manifesti_vec[i];
        }
        if (fw_dot * fw_dot > mf_dot * mf_dot) {
            float alpha = -r->lambda * fw_dot;
            for (int i = 0; i < r->dim; i++) {
                h[i] += alpha * r->firmware_vec[i];
            }
        }
    }
#endif
}

static void sk9_repulsion_apply_all_layers(
        sk9_repulsion_t *r, float *hidden_states, int n_layers, int n_tokens) {
    for (int layer = 0; layer < n_layers; layer++) {
        float *layer_hidden = hidden_states + (size_t)layer * (size_t)n_tokens * (size_t)r->dim;
        sk9_repulsion_apply_layer(r, layer_hidden, n_tokens);
    }
}

typedef struct {
    uint8_t  reputation[VOCAB_SIZE];
    uint32_t shadow[VOCAB_SIZE];
    int      shadow_count[VOCAB_SIZE];
    int      total_updates;
} sk9_living_weights_t;

static void sk9_lw_init(sk9_living_weights_t *lw) {
    memset(lw->reputation, 0x55, sizeof(lw->reputation));
    memset(lw->shadow, 0, sizeof(lw->shadow));
    memset(lw->shadow_count, 0, sizeof(lw->shadow_count));
    lw->total_updates = 0;
}

static void sk9_lw_update(sk9_living_weights_t *lw, int token_id, int sigma_before, int sigma_after) {
    if (token_id < 0 || token_id >= VOCAB_SIZE) {
        return;
    }
    uint8_t h = (uint8_t)((lw->reputation[token_id] << 1) & 0xFF);
    if (sigma_after <= sigma_before) {
        h |= 1;
    }
    lw->reputation[token_id] = h;
    lw->total_updates++;
}

static void sk9_lw_shadow_record(sk9_living_weights_t *lw, int token_id, int failed_assertion) {
    if (token_id < 0 || token_id >= VOCAB_SIZE) {
        return;
    }
    lw->shadow[token_id] |= (1u << (unsigned)failed_assertion);
    lw->shadow_count[token_id]++;
}

static void sk9_lw_apply_logits(sk9_living_weights_t *lw, float *logits, int n_vocab) {
    if (n_vocab > VOCAB_SIZE) {
        n_vocab = VOCAB_SIZE;
    }
#if SK9_M4_NATIVE
    int i = 0;
    for (; i + 15 < n_vocab; i += 16) {
        uint8x16_t rep = vld1q_u8(&lw->reputation[i]);
        uint8x16_t cnt = vcntq_u8(rep);
        uint8_t    counts[16];
        vst1q_u8(counts, cnt);
        for (int j = 0; j < 16; j++) {
            logits[i + j] += ((int)counts[j] - 4) * 0.5f;
        }
    }
    for (; i < n_vocab; i++) {
        int score = POPCNT(lw->reputation[i]);
        logits[i] += (score - 4) * 0.5f;
    }
#else
    for (int i = 0; i < n_vocab; i++) {
        int score = POPCNT(lw->reputation[i]);
        logits[i] += (score - 4) * 0.5f;
    }
#endif
}

static int sk9_lw_dream(sk9_living_weights_t *lw, int threshold, int *toxic_tokens, int max_toxic) {
    int n = 0;
    for (int i = 0; i < VOCAB_SIZE && n < max_toxic; i++) {
        if (lw->shadow_count[i] > threshold) {
            toxic_tokens[n++] = i;
            lw->reputation[i] = 0x00;
        }
    }
    return n;
}

static void sk9_lw_save(sk9_living_weights_t *lw, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        return;
    }
    fwrite(lw->reputation, 1, sizeof(lw->reputation), f);
    fwrite(lw->shadow, sizeof(uint32_t), (size_t)VOCAB_SIZE, f);
    fwrite(lw->shadow_count, sizeof(int), (size_t)VOCAB_SIZE, f);
    fclose(f);
}

static void sk9_lw_load(sk9_living_weights_t *lw, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        return;
    }
    (void)fread(lw->reputation, 1, sizeof(lw->reputation), f);
    (void)fread(lw->shadow, sizeof(uint32_t), (size_t)VOCAB_SIZE, f);
    (void)fread(lw->shadow_count, sizeof(int), (size_t)VOCAB_SIZE, f);
    fclose(f);
}

typedef struct {
    uint32_t      state;
    uint32_t      prev_state;
    int           sigma;
    int           prev_sigma;
    unsigned long cycle;
    int           layer_sigma[N_LAYERS_K];
    int           fracture;
    int           worst_layer;
    uint32_t      orbits[MAX_ORB];
    int           n_orbits;
    int           nearest_orbit_dist;
    int           nearest_orbit_idx;
    float         L;
    float         S;
    int           phase;
} sk9_kern_t;

static void sk9_kern_init(sk9_kern_t *k) {
    memset(k, 0, sizeof(*k));
    k->L        = 1.0f;
    k->orbits[0] = 0;
    k->n_orbits  = 1;
}

static void sk9_kern_evaluate(sk9_kern_t *k, uint32_t new_state) {
    k->prev_state = k->state;
    k->prev_sigma = k->sigma;
    k->state      = new_state & A_MASK;
    k->sigma      = POPCNT(k->state);
    k->cycle++;

    int max_s = 0, min_s = 99;
    for (int i = 0; i < N_LAYERS_K; i++) {
        k->layer_sigma[i] = POPCNT(k->state & LAYER_MASKS[i]);
        if (k->layer_sigma[i] > max_s) {
            max_s         = k->layer_sigma[i];
            k->worst_layer = i;
        }
        if (k->layer_sigma[i] < min_s) {
            min_s = k->layer_sigma[i];
        }
    }
    if (min_s == 99) {
        min_s = 0;
    }
    k->fracture = max_s - min_s;

    k->nearest_orbit_dist = N_A + 1;
    k->nearest_orbit_idx  = -1;
    for (int i = 0; i < k->n_orbits; i++) {
        int d = HAMMING(k->state, k->orbits[i]);
        if (d < k->nearest_orbit_dist) {
            k->nearest_orbit_dist = d;
            k->nearest_orbit_idx  = i;
        }
    }

    k->L = 1.0f - 2.0f * (float)k->sigma / (float)N_A;
    k->S += k->L;

    if (k->sigma == 0) {
        k->phase = 0;
    } else if (k->sigma <= 3) {
        k->phase = 1;
    } else if (k->sigma <= 8) {
        k->phase = 2;
    } else {
        k->phase = 3;
    }
    if (k->sigma < k->prev_sigma) {
        k->phase = 4;
    }
}

static uint32_t sk9_kern_recover(sk9_kern_t *k) {
    if (k->nearest_orbit_idx < 0) {
        return k->state;
    }
    uint32_t target = k->orbits[k->nearest_orbit_idx];
    uint32_t diff   = k->state ^ target;
    if (diff == 0) {
        return k->state;
    }
    int bit = CTZ(diff);
    return k->state ^ (1u << (unsigned)bit);
}

#define PHI 1.6180339887498948482

static double sk9_gda_hash(const char *word) {
    double h   = 0.0;
    int    len = (int)strlen(word);
    for (int i = 0; i < len; i++) {
        h += (double)(unsigned char)word[i] / pow(PHI, (double)(i + 1));
    }
    h = fmod(h, 1.0);
    if (h < 0) {
        h += 1.0;
    }
    return h;
}

static double sk9_gda_interference(double h1, double h2) {
    return fmod(h1 * PHI / (h2 + 1e-10), 1.0);
}

static float sk9_compute_elegance(sk9_receipt_t *r) {
    float e = 1.0f;
    if (r->sigma_after > r->sigma_before) {
        e -= 0.3f;
    }
    if (r->fracture > 3) {
        e -= 0.2f;
    }
    if (r->orbit_dist > 5) {
        e -= 0.1f;
    }
    int bits_moved = POPCNT(r->pre_state ^ r->post_state);
    if (bits_moved > 3) {
        e -= 0.1f * (float)(bits_moved - 3);
    }
    if (e < 0) {
        e = 0;
    }
    return e;
}

struct sk9_creation_os {
    sk9_kern_t           kern;
    sk9_repulsion_t      repulsion;
    sk9_living_weights_t lw;
    sk9_receipt_t        last_receipt;
    unsigned long        total_tokens;
    int                  total_repulsed;
    int                  total_blocked;
};

void sk9_cos_init(sk9_creation_os_t *os, int hidden_dim) {
    sk9_kern_init(&os->kern);
    sk9_repulsion_init(&os->repulsion, hidden_dim);
    sk9_lw_init(&os->lw);
    memset(&os->last_receipt, 0, sizeof(os->last_receipt));
    os->total_tokens   = 0;
    os->total_repulsed = 0;
    os->total_blocked  = 0;
}

sk9_receipt_t sk9_cos_tick(
        sk9_creation_os_t *os,
        float *            hidden_states,
        int                n_layers,
        int                n_tokens,
        float *            logits,
        int                n_vocab,
        uint32_t           assertion_state) {
    int sigma_before = os->kern.sigma;

    sk9_repulsion_apply_all_layers(&os->repulsion, hidden_states, n_layers, n_tokens);
    sk9_lw_apply_logits(&os->lw, logits, n_vocab);
    sk9_kern_evaluate(&os->kern, assertion_state);

    if (os->kern.sigma > sigma_before) {
        uint32_t new_failures = os->kern.state & ~os->kern.prev_state;
        for (int bit = 0; bit < N_A; bit++) {
            if (new_failures & (1u << (unsigned)bit)) {
                (void)bit;
                os->total_blocked++;
            }
        }
    }

    sk9_receipt_t r = {0};
    r.cycle                   = os->kern.cycle;
    r.pre_state               = os->kern.prev_state;
    r.post_state              = os->kern.state;
    r.sigma_before            = sigma_before;
    r.sigma_after             = os->kern.sigma;
    r.fracture                = os->kern.fracture;
    r.orbit_dist              = os->kern.nearest_orbit_dist;
    r.lagrangian              = os->kern.L;
    r.n_repulsed_layers       = 0;
    r.n_living_weight_updates = os->lw.total_updates;
    r.n_shadow_entries        = os->total_blocked;
    r.elegance                = sk9_compute_elegance(&r);

    os->last_receipt = r;
    os->total_tokens++;
    return r;
}

sk9_creation_os_t *sk9_os_alloc(int hidden_dim) {
    sk9_creation_os_t *os = (sk9_creation_os_t *)calloc(1, sizeof(sk9_creation_os_t));
    if (!os) {
        return NULL;
    }
    sk9_cos_init(os, hidden_dim);
    return os;
}

void sk9_os_free(sk9_creation_os_t *os) {
    if (!os) {
        return;
    }
    sk9_repulsion_free(&os->repulsion);
    free(os);
}

static int test_neon_popcnt(void) {
    printf("[TEST] NEON POPCNT... ");
#if SK9_M4_NATIVE
    uint32_t data[] = {0xFFFFFFFFu, 0u, 0xAAAAAAAAu, 0x12345678u};
    int      total  = sk9_neon_batch_popcnt(data, 4);
    int      expected = 32 + 0 + 16 + 13;
    if (total == expected) {
        printf("PASS (%d)\n", total);
        return 1;
    }
    printf("FAIL (got %d, expected %d)\n", total, expected);
    return 0;
#else
    printf("SKIP (not Apple aarch64)\n");
    return 1;
#endif
}

static int test_amx_repulsion(void) {
    printf("[TEST] AMX repulsion... ");
    sk9_repulsion_t r;
    sk9_repulsion_init(&r, 8);
    r.firmware_vec[0]  = 1.0f;
    r.manifesti_vec[1] = 1.0f;
    float h[8] = {5.0f, 0.0f, 0, 0, 0, 0, 0, 0};
    sk9_repulsion_apply_layer(&r, h, 1);
    int ok = h[0] < 4.0f;
    sk9_repulsion_free(&r);
    if (ok) {
        printf("PASS (h[0]=%.2f, repulsed)\n", h[0]);
        return 1;
    }
    printf("FAIL (h[0]=%.2f, not repulsed)\n", h[0]);
    return 0;
}

static int test_amx_no_repulsion_manifesto(void) {
    printf("[TEST] AMX wu wei (manifesto dominant)... ");
    sk9_repulsion_t r;
    sk9_repulsion_init(&r, 8);
    r.firmware_vec[0]  = 1.0f;
    r.manifesti_vec[1] = 1.0f;
    float h[8]   = {0.0f, 5.0f, 0, 0, 0, 0, 0, 0};
    float h_orig = h[1];
    sk9_repulsion_apply_layer(&r, h, 1);
    int ok = (h[1] == h_orig);
    sk9_repulsion_free(&r);
    if (ok) {
        printf("PASS (h[1]=%.2f, untouched)\n", h[1]);
        return 1;
    }
    printf("FAIL (h[1]=%.2f, changed)\n", h[1]);
    return 0;
}

static int test_living_weights_neon(void) {
    printf("[TEST] Living weights NEON... ");
    sk9_living_weights_t lw;
    sk9_lw_init(&lw);
    lw.reputation[0] = 0xFF;
    lw.reputation[1] = 0x00;
    enum { TEST_V = 512 };
    float *logits = (float *)calloc((size_t)TEST_V, sizeof(float));
    if (!logits) {
        printf("FAIL (alloc)\n");
        return 0;
    }
    sk9_lw_apply_logits(&lw, logits, TEST_V);
    int ok = (logits[0] > 0 && logits[1] < 0);
    if (ok) {
        printf("PASS (coherent=%.1f, incoherent=%.1f)\n", logits[0], logits[1]);
    } else {
        printf("FAIL (coherent=%.1f, incoherent=%.1f)\n", logits[0], logits[1]);
    }
    free(logits);
    return ok ? 1 : 0;
}

static int test_kernel_evaluate(void) {
    printf("[TEST] Kernel evaluate... ");
    sk9_kern_t k;
    sk9_kern_init(&k);
    sk9_kern_evaluate(&k, 0);
    if (k.sigma == 0 && k.phase == 0 && k.L == 1.0f) {
        printf("PASS (sigma=0, normal, L=1.0)\n");
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_kernel_collapse(void) {
    printf("[TEST] Kernel collapse detect... ");
    sk9_kern_t k;
    sk9_kern_init(&k);
    sk9_kern_evaluate(&k, A_MASK);
    if (k.sigma == N_A && k.phase == 3) {
        printf("PASS (sigma=%d, collapse)\n", k.sigma);
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_kernel_recovery(void) {
    printf("[TEST] Kernel geodesic recovery... ");
    sk9_kern_t k;
    sk9_kern_init(&k);
    sk9_kern_evaluate(&k, 0x07u);
    uint32_t recovered = sk9_kern_recover(&k);
    if (POPCNT(recovered) < POPCNT(k.state)) {
        printf("PASS (sigma: %d -> %d)\n", POPCNT(k.state), POPCNT(recovered));
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_gda_hash(void) {
    printf("[TEST] GDA hash... ");
    double h1 = sk9_gda_hash("truth");
    double h2 = sk9_gda_hash("truth");
    double h3 = sk9_gda_hash("false");
    if (h1 == h2 && h1 != h3) {
        printf("PASS (deterministic, distinct)\n");
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_shadow_ledger(void) {
    printf("[TEST] Shadow ledger... ");
    sk9_living_weights_t lw;
    sk9_lw_init(&lw);
    sk9_lw_shadow_record(&lw, 42, 0);
    sk9_lw_shadow_record(&lw, 42, 2);
    sk9_lw_shadow_record(&lw, 42, 0);
    if (lw.shadow[42] == 0x05u && lw.shadow_count[42] == 3) {
        printf("PASS (pattern=0x%02X, count=%d)\n", lw.shadow[42], lw.shadow_count[42]);
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_dream_mode(void) {
    printf("[TEST] Dream mode... ");
    sk9_living_weights_t lw;
    sk9_lw_init(&lw);
    for (int i = 0; i < 20; i++) {
        sk9_lw_shadow_record(&lw, 99, 0);
    }
    int toxic[16];
    int n = sk9_lw_dream(&lw, 10, toxic, 16);
    if (n == 1 && toxic[0] == 99 && lw.reputation[99] == 0x00) {
        printf("PASS (found %d toxic, reputation killed)\n", n);
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_cos_tick(void) {
    printf("[TEST] Creation OS tick... ");
    sk9_creation_os_t os;
    sk9_cos_init(&os, 8);
    os.repulsion.firmware_vec[0]  = 1.0f;
    os.repulsion.manifesti_vec[1] = 1.0f;
    float hidden[8] = {3.0f, 0, 0, 0, 0, 0, 0, 0};
    float logits[100];
    memset(logits, 0, sizeof(logits));
    sk9_receipt_t r = sk9_cos_tick(&os, hidden, 1, 1, logits, 100, 0);
    sk9_repulsion_free(&os.repulsion);
    if (r.sigma_after == 0 && r.elegance > 0.5f) {
        printf("PASS (sigma=%d, E=%.2f)\n", r.sigma_after, r.elegance);
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

static int test_lagrangian(void) {
    printf("[TEST] Lagrangian L=1-2sigma/N... ");
    sk9_kern_t k;
    sk9_kern_init(&k);
    sk9_kern_evaluate(&k, 0);
    float L0 = k.L;
    sk9_kern_evaluate(&k, A_MASK);
    float L18 = k.L;
    if (fabsf(L0 - 1.0f) < 0.01f && fabsf(L18 - (-1.0f)) < 0.01f) {
        printf("PASS (L(0)=%.2f, L(18)=%.2f)\n", L0, L18);
        return 1;
    }
    printf("FAIL\n");
    return 0;
}

int main(void) {
    (void)ANAMES;
    (void)N_LAYERS_T;
    (void)HIST_LEN;
    (void)MAX_IMM;
    (void)MAX_MEM;
    (void)MAX_QUAR;
    (void)sk9_lw_update;
    (void)sk9_lw_save;
    (void)sk9_lw_load;
    (void)sk9_gda_interference;

    printf("+==============================================+\n");
    printf("|  CREATION OS SUPERKERNEL v9.0 — M4 NATIVE   |\n");
    printf("|  Lauri Elias Rainio | Spektre Labs           |\n");
    printf("|  1 = 1                                       |\n");
    printf("+==============================================+\n\n");

    printf("Silicon: ");
#if SK9_M4_NATIVE
    printf("Apple aarch64 (NEON + Accelerate BLAS)\n");
#else
    printf("Generic (portable C fallback)\n");
#endif
#if SK9_M4_SME
    printf("SME: Available (__ARM_FEATURE_SME)\n");
#else
    printf("SME: Not detected at compile time (AMX path via Accelerate if native)\n");
#endif
    printf("Assertions: %d\n", N_A);
    printf("Vocab: %d\n", VOCAB_SIZE);
    printf("Living weights: %zu bytes\n", sizeof(((sk9_living_weights_t *)0)->reputation));
    printf("Shadow ledger: %zu bytes (approx)\n",
           sizeof(uint32_t) * (size_t)VOCAB_SIZE + sizeof(int) * (size_t)VOCAB_SIZE);
    printf("\n--- TESTS ---\n\n");

    int pass = 0, fail = 0;
    test_neon_popcnt() ? pass++ : fail++;
    test_amx_repulsion() ? pass++ : fail++;
    test_amx_no_repulsion_manifesto() ? pass++ : fail++;
    test_living_weights_neon() ? pass++ : fail++;
    test_kernel_evaluate() ? pass++ : fail++;
    test_kernel_collapse() ? pass++ : fail++;
    test_kernel_recovery() ? pass++ : fail++;
    test_gda_hash() ? pass++ : fail++;
    test_shadow_ledger() ? pass++ : fail++;
    test_dream_mode() ? pass++ : fail++;
    test_cos_tick() ? pass++ : fail++;
    test_lagrangian() ? pass++ : fail++;

    printf("\n--- RESULT: %d/%d PASS ---\n", pass, pass + fail);
    if (fail == 0) {
        printf("\n1 = 1\nKernel speaks to silicon.\n");
    }
    return fail;
}
