/* CREATION OS — SUPERKERNEL v8.0 — llama.cpp bridge header (CC BY 4.0, Lauri Elias Rainio) */
#ifndef SUPERKERNEL_V8_H
#define SUPERKERNEL_V8_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { KOK = 0, KFAIL = -1, KINV = -2 } sk8_kstat_t;

#define SK8_N_A 18
#define SK8_MAX_MEM 2048
#define SK8_MAX_QUAR 128

typedef struct sk8_mem {
    char  key[64];
    char  val[128];
    float conf;
    int   stat; /* 0=verified 1=unverified 2=quarantined */
} sk8_mem_t;

typedef struct sk8_kernel {
    uint32_t state;
    uint32_t history[256];
    int      hidx, hcount;
    int      sigma, prev_sigma, d_sigma, d2_sigma, prev_d;
    int      phase, prev_phase;
    uint32_t imm_pat[32];
    int      imm_cnt[32];
    int      imm_radius[32];
    int      n_imm;
    uint32_t orbit[32];
    int      n_orbit;
    int      resonance;
    struct {
        uint32_t pattern;
        int      count;
        int      n_bits;
        char     name[16];
    } chords[16];
    int n_chords;
    sk8_mem_t mem[SK8_MAX_MEM];
    int       mcnt;
    sk8_mem_t quar[SK8_MAX_QUAR];
    int       qcnt;
    struct {
        unsigned long cycle;
        uint32_t      pre_state;
        uint32_t      post_state;
        int           strategy;
        int           hamming_moved;
        int           sigma_before;
        int           sigma_after;
        int           fracture_before;
        int           fracture_after;
        int           orbit_dist_before;
        int           orbit_dist_after;
        float         elegance;
        bool          legal;
    } receipts[64];
    int n_receipts;
    float L, S, Q;
    int   mass_gap;
    float blowup;
    int   loop_period;
    bool  beyond_horizon;
    uint32_t dream_traces[32];
    int      n_dreams;
    unsigned long cycle;
    bool          running, inv;
    char          identity[64];
} sk8_kernel_t;

typedef struct sk8_manifold_mask {
    uint8_t gate_fp4;
    float   tape_tail_var;
    bool    block_all_except_escape;
    bool    fracture;
} sk8_manifold_mask_t;

typedef struct {
    int32_t id;
    float   logit;
    float   p;
} sk8_logit_row_t;

#ifdef __cplusplus
extern "C" {
#endif

void sk8_set_boot_silent(int silent);
sk8_kstat_t sk8_kinit(sk8_kernel_t * k);
void sk8_commit_token(sk8_kernel_t * k, const char * piece_utf8);
void sk8_compute_manifold_mask(sk8_kernel_t * k, sk8_manifold_mask_t * out);
void sk8_apply_manifold_mask(
        const sk8_manifold_mask_t * mask,
        sk8_logit_row_t * rows,
        size_t n,
        int32_t coherence_id,
        int32_t eos_id);
const char * sk8_boot_assertion_text(void);
int sk8_boot_assertion_count(void);

#ifdef __cplusplus
}
#endif

/* Alias for Spektre / llama context pointer (same layout as prior ks_t slot). */
typedef sk8_kernel_t kernel_t;

#endif /* SUPERKERNEL_V8_H */
