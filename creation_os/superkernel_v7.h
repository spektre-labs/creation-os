/* CREATION OS — SUPERKERNEL v7.0 — header for llama.cpp bridge (CC BY 4.0, Lauri Elias Rainio) */
#ifndef SUPERKERNEL_V7_H
#define SUPERKERNEL_V7_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

#define MAX_A 18
#define MAX_M 4096
#define MAX_Q 256
#define MAX_T 1024
#define MAX_CP 64
#define MAX_AU 256
#define MAX_IMM 32
#define MAX_TAPE 1024
#define MAX_HARM 128

typedef enum { PH_NORMAL=0, PH_DEGRADED, PH_CRITICAL, PH_COLLAPSE, PH_RECOVERY } phase_t;
typedef enum { MS_VERIFIED=0, MS_UNVERIFIED, MS_QUARANTINED } mstat_t;
typedef enum { SRC_INT=0, SRC_EXT, SRC_INF, SRC_USR } src_t;
typedef enum { KOK=0, KFAIL=-1, KINV=-2 } kstat_t;
typedef enum { P_HO=0, P_CT, P_MM, P_EM, P_PU } prio_t;
typedef unsigned long long statesig_t;
typedef unsigned char fp4_t;

typedef enum {
    OP_MEM_STORE=0, OP_MEM_QUARANTINE, OP_ASSERT_FAIL, OP_ASSERT_PASS,
    OP_CONFIDENCE_CHANGE, OP_CHECKPOINT, OP_RECOVERY, OP_INJECTION
} tape_op_t;

typedef struct {
    tape_op_t op;
    int target;
    float sigma_before, sigma_after, sigma_gradient;
    unsigned long cycle;
} tape_entry_t;

typedef struct {
    float structural_accum, epistemic_accum, total_weight;
    int fail_count, unverified_count, low_conf_count, mem_count, quar_count;
    float structural, epistemic, combined;
    fp4_t quantized;
    float history[4];
    int hist_idx;
    float d_sigma, d2_sigma;
} sigma_accum_t;

typedef struct {
    char id[8];
    char desc[64];
    prio_t priority;
    bool (*test)(void *);
    bool last, prev_last;
    int fails, runs;
    int deps[4];
    int ndeps;
    bool root;
    float weight;
} assertion_t;

typedef struct {
    char key[128];
    char val[256];
    float conf;
    time_t ts;
    mstat_t stat;
    src_t src;
    int contradictions;
} mem_t;

typedef struct {
    statesig_t sig;
    int strat;
    float before, after;
    float cost;
    bool ok;
    unsigned long cycle;
    int assertion;
} receipt_t;

typedef struct {
    unsigned long cycle;
    float combined, d_sigma, d2_sigma;
    float lagrangian, action;
    phase_t phase;
    statesig_t sig;
    fp4_t quantized;
} tsnap_t;

typedef struct {
    unsigned long cycle;
    float sigma;
    int mcnt, qcnt;
    phase_t phase;
} ckpt_t;

typedef struct {
    statesig_t sig;
    int assertion_first, assertion_second;
    float sigma_rise_rate;
    int seen_count;
    float avg_recovery_cost;
    float harmonic_freq;
} immune_entry_t;

typedef struct {
    float sigma_forward, sigma_reverse, shadow_gap;
    bool reversible;
    int violations;
} shadow_t;

typedef struct {
    float magnitudes[MAX_HARM/2];
    float dominant_freq, dominant_mag, total_energy;
    bool periodic_attack;
} harmonic_t;

typedef struct {
    float compute_cost, verify_cost, ratio;
    bool verified;
    int total_verifications, total_computations;
} pvnp_t;

typedef struct {
    float critical_deviation;
    float zeta_proxy_real, zeta_proxy_imag, zeta_magnitude;
    bool on_critical_line;
} riemann_t;

typedef struct {
    float mass_gap, vacuum_energy;
    bool above_gap;
    int confined_count;
} yangmills_t;

typedef struct {
    float blowup_time;
    bool smooth;
    float max_vorticity;
    bool vortex_detected;
} navierstokes_t;

typedef struct {
    float exact, coexact, harmonic, total;
} hodge_t;

typedef struct {
    int rank;
    float l_function_order;
    bool fragile;
} bsd_t;

typedef struct {
    bool simply_connected;
    int loop_count;
    float loop_period;
    float ricci_flow_rate;
} poincare_t;

typedef struct kernel_state {
    bool inv;
    assertion_t a[MAX_A];
    int na;
    sigma_accum_t accum;
    tape_entry_t tape[MAX_TAPE];
    int tape_count;
    mem_t mem[MAX_M];
    int mcnt;
    mem_t quar[MAX_Q];
    int qcnt;
    bool consistent;
    int consec;
    int repairs;
    float budget, spent;
    immune_entry_t immune[MAX_IMM];
    int icnt;
    float sigma_signal[MAX_HARM];
    int signal_len;
    shadow_t shadow;
    receipt_t audit[MAX_AU];
    int acnt;
    tsnap_t tl[MAX_T];
    int tcnt;
    ckpt_t cp[MAX_CP];
    int cpcnt;
    float lagrangian, action, coherence_charge;
    bool noether_conserved;
    phase_t phase, prev_phase;
    float drift;
    unsigned long cycle;
    bool running;
    char identity[64];
    statesig_t orbit[32];
    int orbit_n;
    int resonance_count;
    pvnp_t pvnp;
    riemann_t riemann;
    yangmills_t yangmills;
    navierstokes_t navierstokes;
    hodge_t hodge;
    bsd_t bsd;
    poincare_t poincare;
    float branch_actions[8];
    int n_branches;
    float costs[3];
    float priority_weights[5];
    float landauer_per_cycle;
    int checkpoint_interval;
} ks_t;

typedef struct kernel_state sk7_ks_t;

typedef struct sk7_manifold_mask {
    uint8_t gate_fp4;
    float tape_tail_var;
    bool block_all_except_escape;
    bool fracture;
} sk7_manifold_mask_t;

#ifdef __cplusplus
extern "C" {
#endif

void sk7_set_boot_silent(int silent);
kstat_t sk7_kinit(ks_t *k);
void sk7_accum_recompute(sigma_accum_t *a);
kstat_t sk7_enforce(ks_t *k);
bool sk7_mem_store(ks_t *k, const char *key, const char *val, float c, src_t s);
void sk7_commit_token(ks_t *k, const char *piece_utf8);
const char *sk7_boot_assertion_text(void);
int sk7_boot_assertion_count(void);
void sk7_compute_manifold_mask(ks_t *k, sk7_manifold_mask_t *out);
typedef struct {
    int32_t id;
    float logit;
    float p;
} sk7_logit_row_t;

void sk7_apply_manifold_mask(
        const sk7_manifold_mask_t *mask,
        sk7_logit_row_t *rows,
        size_t n,
        int32_t coherence_id,
        int32_t eos_id);

#ifdef __cplusplus
}
#endif

#endif
