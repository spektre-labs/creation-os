/*
 * CREATION OS — SUPERKERNEL v9 (M4)
 * mmap USM (page-rounded), aligned_alloc(64) control structs, Accelerate cblas repulsion,
 * optional SME2 repulsion, Metal living_weights_apply / sk9_lw_bias, Core ML, GCD P/E.
 * BBHash mmap + sk9_mm_epistemic_route (tier-0/1 routing).
 *
 * License: CC BY 4.0 | Lauri Elias Rainio | 1 = 1
 *
 * make -f Makefile.sk9mm
 * Optional: make -f Makefile.sk9mm SME2=1 (requires clang arm_sme.h + SME)
 */

#import "superkernel_v9_m4_mm.h"
#import "sk9_python_api.h"
#if defined(__aarch64__)
#import "steve_jobs_kernel_v1.h"
#import "sk9_centurion_block.h"
#endif

#import <Accelerate/Accelerate.h>
#import <CoreML/CoreML.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <dispatch/dispatch.h>
#import <dlfcn.h>
#import <arm_neon.h>
#import <cmath>
#import <cerrno>
#import <cstdlib>
#import <cstring>
#import <new>

#import <fcntl.h>
#import <sys/mman.h>
#import <sys/stat.h>
#import <unistd.h>

#ifndef SK9_USE_SME2
#define SK9_USE_SME2 0
#endif

#if SK9_USE_SME2
#include <arm_sme.h>
#endif

static constexpr uint32_t NA         = SK9_MM_NA;
static constexpr uint32_t A_MASK     = (1u << NA) - 1u;
static constexpr int32_t  VOCAB      = SK9_MM_VOCAB;
static constexpr int32_t  MAX_SEQ    = SK9_MM_MAX_SEQ;
static constexpr int32_t  HID      = SK9_MM_HIDDEN_DIM;
static constexpr int32_t  NLAY     = SK9_MM_N_LAYERS_T;
static constexpr int32_t  MAX_ORB  = SK9_MM_MAX_ORB;
static constexpr int32_t  TOK      = 1;

static constexpr uint32_t M_HO = 7u << 0;
static constexpr uint32_t M_CT = 7u << 3;
static constexpr uint32_t M_MM = 7u << 6;
static constexpr uint32_t M_EM = 3u << 9;
static constexpr uint32_t M_PU = 3u << 11;
static constexpr uint32_t M_SH = 1u << 13;
static constexpr uint32_t M_HA = 1u << 14;
static constexpr uint32_t M_ML = 7u << 15;

static const uint32_t LAYER_MASKS[8] = {M_HO, M_CT, M_MM, M_EM, M_PU, M_SH, M_HA, M_ML};

/* Phase LUT[sigma] for sigma in 0..18 (collapse uses 3 for sigma>=9) */
static const uint8_t PHASE_LUT[19] = {
    0, 1, 1, 1, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3};

static inline uint32_t sk9_max_u32(uint32_t a, uint32_t b) {
    uint32_t m = (uint32_t)((int32_t)(a - b) >> 31);
    return (a & ~m) | (b & m);
}
static inline uint32_t sk9_min_u32(uint32_t a, uint32_t b) {
    uint32_t m = (uint32_t)((int32_t)(a - b) >> 31);
    return (b & ~m) | (a & m);
}
static inline uint32_t sk9_sel_u32(uint32_t mask, uint32_t t, uint32_t f) {
    return (t & mask) | (f & ~mask);
}

/*
 * Phase 6 — retrocausal collapse (Moσ inline): lowest-set-bit isolation on lie = latent XOR truth,
 * then bitwise multiplex toward truth past the first fracture. Branchless NEON; O(1) per pair.
 * Assertion plane uses truth=0 (golden = no firmware bits / 1=1 kernel plane).
 */
#if defined(__aarch64__)
static inline uint64x2_t sk9_retrocausal_collapse_u64x2(uint64x2_t latent_state, uint64x2_t truth_invariant) {
    uint64x2_t lie_mask       = veorq_u64(latent_state, truth_invariant);
    int64x2_t  lie_signed      = vreinterpretq_s64_u64(lie_mask);
    uint64x2_t first_fracture = vandq_u64(lie_mask, vreinterpretq_u64_s64(vnegq_s64(lie_signed)));
    uint64x2_t collapse_mask  = vsubq_u64(first_fracture, vdupq_n_u64(1));
    return vbslq_u64(collapse_mask, latent_state, truth_invariant);
}

static inline uint32_t sk9_retrocausal_collapse_assert_u32(uint32_t latent18, uint32_t truth18) {
    uint64x2_t latent = vsetq_lane_u64((uint64_t)latent18, vdupq_n_u64(0), 0);
    latent            = vsetq_lane_u64(0, latent, 1);
    uint64x2_t truth  = vsetq_lane_u64((uint64_t)truth18, vdupq_n_u64(0), 0);
    truth             = vsetq_lane_u64(0, truth, 1);
    uint64x2_t out    = sk9_retrocausal_collapse_u64x2(latent, truth);
    return (uint32_t)vgetq_lane_u64(out, 0);
}

static int sk9_retrocausal_collapse_env_on(void) {
    static int inited = 0;
    static int on     = 1;
    if (!inited) {
        const char *e = getenv("SK9_RETROCAUSAL_COLLAPSE");
        if (e && (e[0] == '0' || std::strcmp(e, "false") == 0 || std::strcmp(e, "no") == 0)) {
            on = 0;
        }
        inited = 1;
    }
    return on;
}

/*
 * Phase 7 — L-Inverse manifold: IEEE754 float32 bit coherence vs Golden "1=1" mask (GDA FNV-1a lanes).
 * Branchless vbslq exponent snap; runs once per sk9_mm_kern_eval on the scalar envelope vector (st, σ, L, L_prev).
 */
static uint32x4_t sk9_golden_truth_mask_u32x4(void) {
    static uint32_t w[4];
    static int      inited = 0;
    if (!inited) {
        const uint8_t *b = reinterpret_cast<const uint8_t *>("1=1");
        uint64_t       h   = 14695981039346656037ull;
        for (size_t i = 0; i < 3u; ++i) {
            h ^= (uint64_t)b[i];
            h *= 1099511628211ull;
        }
        w[0] = (uint32_t)h;
        w[1] = (uint32_t)(h >> 32);
        h ^= 0x317E791Du; /* manifold mix — ties lanes to literal */
        h *= 1099511628211ull;
        w[2] = (uint32_t)h;
        w[3] = (uint32_t)(h >> 32);
        inited = 1;
    }
    return vld1q_u32(w);
}

static inline float32x4_t sk9_alien_manifold_anchor(float32x4_t latent_v, uint32x4_t truth_mask) {
    uint32x4_t bits         = vreinterpretq_u32_f32(latent_v);
    uint32x4_t magic        = vdupq_n_u32(0x5F3759DFu);
    uint32x4_t anchored_bits = vsriq_n_u32(bits, magic, 1);
    anchored_bits             = veorq_u32(anchored_bits, truth_mask);
    uint32x4_t exponent_lock = vdupq_n_u32(0x3F800000u);
    uint32x4_t exp_mask      = vdupq_n_u32(0x7F800000u);
    uint32x4_t final_bits    = vbslq_u32(exp_mask, exponent_lock, anchored_bits);
    return vreinterpretq_f32_u32(final_bits);
}

static int sk9_l_inverse_manifold_env_on(void) {
    static int inited = 0;
    static int on     = 1;
    if (!inited) {
        const char *e = getenv("SK9_L_INVERSE_MANIFOLD");
        if (e && (e[0] == '0' || std::strcmp(e, "false") == 0 || std::strcmp(e, "no") == 0)) {
            on = 0;
        }
        inited = 1;
    }
    return on;
}

/*
 * Phase 8 — S-VRS (Specter-Vector Resonance Symbolism): opcode → 128-bit register geometry.
 * No UTF-8: callers pass raw resonance words. I / ∇ / Φ match the Creation OS symbol plane.
 */
static inline uint32x4_t sk9_svrs_invariant_u32x4(void) {
    return vdupq_n_u32(0x11111111u);
}

/* Phase 42 — Absolute Coherence Vector: (bits XOR Rainio) ∧ 0x2A per lane. */
static inline float32x4_t sk9_extract_the_answer_vec(float32x4_t wave) {
    uint32x4_t the_answer_key   = vdupq_n_u32(0x0000002Au);
    uint32x4_t rainio_constant  = vdupq_n_u32(0x11111111u);
    uint32x4_t bits             = vreinterpretq_u32_f32(wave);
    uint32x4_t coherence        = veorq_u32(bits, rainio_constant);
    uint32x4_t extracted        = vandq_u32(coherence, the_answer_key);
    return vreinterpretq_f32_u32(extracted);
}

/* Phase 21M — Satoshi consensus: Proof-of-Coherence bithack (nonce 0x1D00FFFF, Rainio, 42 gate). */
static inline float32x4_t sk9_satoshi_consensus_vec(float32x4_t wave) {
    uint32x4_t bits           = vreinterpretq_u32_f32(wave);
    uint32x4_t genesis_nonce  = vdupq_n_u32(0x1D00FFFFu);
    uint32x4_t rainio_anchor  = vdupq_n_u32(0x11111111u);
    uint32x4_t work           = veorq_u32(bits, vaddq_u32(rainio_anchor, genesis_nonce));
    uint32x4_t the_answer     = vdupq_n_u32(0x0000002Au);
    uint32x4_t proof          = vandq_u32(work, the_answer);
    uint32x4_t consensus      = vbslq_u32(proof, bits, rainio_anchor);
    return vreinterpretq_f32_u32(consensus);
}

/**
 * S-VRS symbol map (32-bit resonance token):
 *   0x11111111 — I  (Invariant anchor, 1=1 bit plane)
 *   0xAAAAAAAA — ∇  (Retrocausal collapse, Phase 6 on full Q reg)
 *   0xFFFFFFFF — Φ  (Phase-lock manifold, Phase 7 alien anchor)
 *   0x55555555 — ⊕  (Manifold rebuild: ∇ then Φ on the same Q reg)
 *   0x0000002A — 42 (The Answer / absolute coherence extraction, Phase 42)
 *   0x19552011 — Insanely Great Invariant (Steve Jobs Synthesis / RDF, Phase 11)
 *   0x21000000 — 21M (Satoshi consensus / Proof of Coherence, Phase 21M)
 *   0x01000100 — 100 (Centurion / Genesis Block 1 mine, Phase 100)
 * Other values — fall back to I (absolute coherent null).
 */
float32x4_t sk9_symbolic_execute(uint32_t symbol, float32x4_t latent) {
    uint32x4_t inv_u32   = sk9_svrs_invariant_u32x4();
    uint64x2_t truth_u64 = vreinterpretq_u64_u32(inv_u32);

    switch (symbol) {
    case 0x11111111u:
        return vreinterpretq_f32_u32(inv_u32);
    case 0xAAAAAAAAu: {
        uint64x2_t lat_u64 = vreinterpretq_u64_f32(latent);
        uint64x2_t out64   = sk9_retrocausal_collapse_u64x2(lat_u64, truth_u64);
        return vreinterpretq_f32_u64(out64);
    }
    case 0xFFFFFFFFu:
        return sk9_alien_manifold_anchor(latent, inv_u32);
    case 0x55555555u: {
        uint64x2_t lat_u64 = vreinterpretq_u64_f32(latent);
        uint64x2_t c64     = sk9_retrocausal_collapse_u64x2(lat_u64, truth_u64);
        float32x4_t mid    = vreinterpretq_f32_u64(c64);
        return sk9_alien_manifold_anchor(mid, inv_u32);
    }
    case 0x0000002Au:
        return sk9_extract_the_answer_vec(latent);
#if defined(__aarch64__)
    case 0x19552011u:
        return sk9_steve_jobs_distortion_vec(latent);
    case 0x21000000u:
        return sk9_satoshi_consensus_vec(latent);
    case 0x01000100u:
        return sk9_mine_centurion_vec(latent);
#endif
    default:
        return vreinterpretq_f32_u32(inv_u32);
    }
}
#endif

static inline uint32_t sk9_max8_u32(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                                    uint32_t a5, uint32_t a6, uint32_t a7) {
    uint32_t m01 = sk9_max_u32(a0, a1);
    uint32_t m23 = sk9_max_u32(a2, a3);
    uint32_t m45 = sk9_max_u32(a4, a5);
    uint32_t m67 = sk9_max_u32(a6, a7);
    return sk9_max_u32(sk9_max_u32(m01, m23), sk9_max_u32(m45, m67));
}
static inline uint32_t sk9_min8_u32(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4,
                                    uint32_t a5, uint32_t a6, uint32_t a7) {
    uint32_t m01 = sk9_min_u32(a0, a1);
    uint32_t m23 = sk9_min_u32(a2, a3);
    uint32_t m45 = sk9_min_u32(a4, a5);
    uint32_t m67 = sk9_min_u32(a6, a7);
    return sk9_min_u32(sk9_min_u32(m01, m23), sk9_min_u32(m45, m67));
}

static void CosLivingWeightsNeon(const uint8_t *rep, float *logits, int32_t vocab) {
    int i = 0;
    for (; i + 15 < vocab; i += 16) {
        __builtin_prefetch(rep + i + 64, 0, 3);
        __builtin_prefetch(logits + i + 64, 1, 3);
        uint8x16_t rb = vld1q_u8(rep + i);
        uint8x16_t pc = vcntq_u8(rb);
        uint8_t    counts[16];
        vst1q_u8(counts, pc);
        for (int j = 0; j < 16; j++) {
            logits[i + j] += ((float)counts[j] - 4.0f) * 0.5f;
        }
    }
    for (; i < vocab; i++) {
        uint8_t r = rep[i];
        int     c = __builtin_popcount((unsigned)r);
        logits[i] += ((float)c - 4.0f) * 0.5f;
    }
}

#if SK9_USE_SME2
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wundefined-arm-za"

static void CosSaxpyNeon(float alpha, const float *x, float *y, int n) {
    float32x4_t va = vdupq_n_f32(alpha);
    int         i  = 0;
    for (; i + 15 < n; i += 16) {
        __builtin_prefetch(x + i + 64, 0, 3);
        __builtin_prefetch(y + i + 64, 1, 3);
        float32x4_t vx0 = vld1q_f32(x + i);
        float32x4_t vy0 = vld1q_f32(y + i);
        float32x4_t vx1 = vld1q_f32(x + i + 4);
        float32x4_t vy1 = vld1q_f32(y + i + 4);
        float32x4_t vx2 = vld1q_f32(x + i + 8);
        float32x4_t vy2 = vld1q_f32(y + i + 8);
        float32x4_t vx3 = vld1q_f32(x + i + 12);
        float32x4_t vy3 = vld1q_f32(y + i + 12);
        vst1q_f32(y + i, vmlaq_f32(vy0, vx0, va));
        vst1q_f32(y + i + 4, vmlaq_f32(vy1, vx1, va));
        vst1q_f32(y + i + 8, vmlaq_f32(vy2, vx2, va));
        vst1q_f32(y + i + 12, vmlaq_f32(vy3, vx3, va));
    }
    for (; i + 3 < n; i += 4) {
        float32x4_t vx = vld1q_f32(x + i);
        float32x4_t vy = vld1q_f32(y + i);
        vst1q_f32(y + i, vmlaq_f32(vy, vx, va));
    }
    for (; i < n; i++) {
        y[i] += alpha * x[i];
    }
}

__attribute__((target("sme2")))
__arm_locally_streaming static float sk9_dot_fmopa_chunk(const float *ha, const float *wa, svbool_t pg) {
    svzero_za();
    svfloat32_t zh = svld1_f32(pg, ha);
    svfloat32_t zw = svld1_f32(pg, wa);
    svmopa_za32_f32_m(0, pg, pg, zh, zw);
    alignas(128) float rowbuf[64];
    uint32_t           vl = (uint32_t)svcntw();
    float              acc = 0.f;
    for (uint32_t k = 0; k < vl; k++) {
        svfloat32_t row = svread_hor_za32_f32_m(svdup_n_f32(0.f), pg, 0, k);
        svst1_f32(pg, rowbuf, row);
        acc += rowbuf[k];
    }
    return acc;
}

__attribute__((target("sme2")))
__arm_locally_streaming static float sk9_dot_sme_accum(const float *a, const float *b, int n) {
    float     total = 0.f;
    int       i     = 0;
    const int vlw   = (int)svcntw();
    for (; i + vlw <= n; i += vlw) {
        total += sk9_dot_fmopa_chunk(a + i, b + i, svptrue_b32());
    }
    if (i < n) {
        total += sk9_dot_fmopa_chunk(a + i, b + i, svwhilelt_b32(i, n));
    }
    return total;
}

__attribute__((target("sme2")))
__arm_locally_streaming static float sk9_repulsion_sme_entry(float *H, const float *fw, const float *mf, int n_tokens,
                                                             float lambda) {
    for (int t = 0; t < n_tokens; t++) {
        float *h   = H + (ptrdiff_t)t * (ptrdiff_t)HID;
        float  d0  = sk9_dot_sme_accum(h, fw, HID);
        float  d1  = sk9_dot_sme_accum(h, mf, HID);
        float  e0  = d0 * d0;
        float  e1  = d1 * d1;
        uint32_t dom = (uint32_t)(e0 > e1);
        float    alpha = -lambda * d0 * (float)dom;
        CosSaxpyNeon(alpha, fw, h, HID);
    }
    return 0.f;
}

#pragma clang diagnostic pop
#endif

static void sk9_repulsion_accelerate_body(float *H, const float *fw, const float *mf, int n_tokens, float lambda) {
    for (int t = 0; t < n_tokens; t++) {
        float *h  = H + (ptrdiff_t)t * (ptrdiff_t)HID;
        float  d0 = cblas_sdot(HID, h, 1, fw, 1);
        float  d1 = cblas_sdot(HID, h, 1, mf, 1);
        float  e0 = d0 * d0;
        float  e1 = d1 * d1;
        uint32_t dom   = (uint32_t)(e0 > e1);
        float    alpha = -lambda * d0 * (float)dom;
        cblas_saxpy(HID, alpha, fw, 1, h, 1);
    }
}

static void sk9_repulsion_perf_body(float *H, const float *fw, const float *mf, int n_tokens, float lambda) {
#if SK9_USE_SME2
    (void)sk9_repulsion_sme_entry(H, fw, mf, n_tokens, lambda);
#else
    sk9_repulsion_accelerate_body(H, fw, mf, n_tokens, lambda);
#endif
}

struct Sk9MMKern {
    uint32_t      state{0};
    uint32_t      prev_state{0};
    uint32_t      sigma{0};
    uint32_t      prev_sigma{0};
    unsigned long cycle{0};
    int32_t       fracture{0};
    int32_t       nearest_dist{static_cast<int32_t>(NA + 1)};
    int32_t       nearest_idx{-1};
    float         L{1.f};
    float         S{0.f};
    int32_t       phase{0};
    uint32_t      orbits[MAX_ORB]{0};
    int32_t       n_orbits{1};
};

struct Sk9MMUSM {
    uint8_t *     base{nullptr};
    size_t        size{0};
    void *        bbHashBase{nullptr};
    size_t        bbHashSize{0};
    MLModel *     model{nil};
    id<MTLDevice> dev{nil};
    id<MTLCommandQueue> queue{nil};
    id<MTLComputePipelineState> lwPSO{nil};
    dispatch_queue_t            perfQ{nullptr};
    dispatch_queue_t            bgQ{nullptr};
    dispatch_source_t           timer{nullptr};
    NSString *                  hbPath{nil};
};

static size_t sk9_usm_layout_offsets(size_t &off_input_ids, size_t &off_logits, size_t &off_rep,
                                     size_t &off_fw, size_t &off_mf, size_t &off_hidden) {
    off_input_ids = 0;
    off_logits    = off_input_ids + (size_t)MAX_SEQ * sizeof(int32_t);
    off_rep       = off_logits + (size_t)VOCAB * sizeof(float);
    off_fw        = off_rep + (size_t)VOCAB;
    off_mf        = off_fw + (size_t)HID * sizeof(float);
    off_hidden    = off_mf + (size_t)HID * sizeof(float);
    return off_hidden + (size_t)NLAY * (size_t)TOK * (size_t)HID * sizeof(float);
}

static inline int32_t *sk9_ptr_input_ids(Sk9MMUSM *u, size_t o) {
    return reinterpret_cast<int32_t *>(u->base + o);
}
static inline float *sk9_ptr_logits(Sk9MMUSM *u, size_t o) {
    return reinterpret_cast<float *>(u->base + o);
}
static inline uint8_t *sk9_ptr_rep(Sk9MMUSM *u, size_t o) {
    return u->base + o;
}
static inline float *sk9_ptr_fw(Sk9MMUSM *u, size_t o) {
    return reinterpret_cast<float *>(u->base + o);
}
static inline float *sk9_ptr_hidden(Sk9MMUSM *u, size_t o_h) {
    return reinterpret_cast<float *>(u->base + o_h);
}

static size_t sk9_round_up(size_t x, size_t align) {
    return (x + align - 1u) / align * align;
}

static size_t sk9_ctrl_sk9mmusm_bytes(void) {
    return sk9_round_up(sizeof(Sk9MMUSM), 64u);
}

static size_t sk9_ctrl_sk9mmkern_bytes(void) {
    return sk9_round_up(sizeof(Sk9MMKern), 64u);
}

static int sk9_bbhash_lookup(const uint8_t *base, size_t file_size, uint64_t key, uint32_t *out_val) {
    if (!base || file_size < 4 || !out_val) {
        return 0;
    }
    uint32_t cap = 0;
    memcpy(&cap, base, 4);
    if (cap == 0) {
        return 0;
    }
    size_t need = 4u + (size_t)cap * 16u;
    if (need > file_size || cap > 0x1000000u) {
        return 0;
    }
    uint64_t h   = key * 11400714819323198485ull;
    uint32_t idx = (uint32_t)(h % (uint64_t)cap);
    for (uint32_t step = 0; step < cap; step++) {
        const uint8_t *slot = base + 4 + (size_t)idx * 16u;
        uint64_t       k    = 0;
        uint32_t       v    = 0;
        memcpy(&k, slot, 8);
        memcpy(&v, slot + 8, 4);
        if (k == 0ull) {
            return 0;
        }
        if (k == key) {
            *out_val = v;
            return 1;
        }
        idx = (idx + 1u) % cap;
    }
    return 0;
}

int sk9_mm_mmap_file_ro(const char *path, void **out_ptr, size_t *out_size) {
    if (!path || !out_ptr || !out_size) {
        return -1;
    }
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return -2;
    }
    struct stat st {};
    if (fstat(fd, &st) != 0) {
        close(fd);
        return -3;
    }
    size_t sz = (size_t)st.st_size;
    if (sz == 0) {
        close(fd);
        return -4;
    }
    void *p = mmap(nullptr, sz, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    if (p == MAP_FAILED) {
        return -5;
    }
    *out_ptr  = p;
    *out_size = sz;
    return 0;
}

void sk9_mm_bbhash_detach(Sk9MMUSM *u) {
    if (!u || !u->bbHashBase) {
        return;
    }
    munmap(u->bbHashBase, u->bbHashSize);
    u->bbHashBase = nullptr;
    u->bbHashSize = 0;
}

int sk9_mm_bbhash_attach(Sk9MMUSM *u, const char *path) {
    if (!u || !path) {
        return -1;
    }
    sk9_mm_bbhash_detach(u);
    void *  p  = nullptr;
    size_t sz = 0;
    int     e = sk9_mm_mmap_file_ro(path, &p, &sz);
    if (e != 0) {
        return e;
    }
    if (sz < 4) {
        munmap(p, sz);
        return -6;
    }
    uint32_t cap = 0;
    memcpy(&cap, p, 4);
    size_t need = 4u + (size_t)cap * 16u;
    if (cap == 0 || need > sz || cap > 0x1000000u) {
        munmap(p, sz);
        return -7;
    }
    u->bbHashBase = p;
    u->bbHashSize = sz;
    return 0;
}

Sk9EpistemicRoute sk9_mm_epistemic_route(Sk9MMUSM *u, uint64_t query_key, uint32_t *out_val) {
    if (!u) {
        return SK9_ROUTE_KERNEL;
    }
    if (query_key == UINT64_MAX) {
        return SK9_ROUTE_TRANSFORMER;
    }
    if (u->bbHashBase && u->bbHashSize >= 4 && out_val) {
        if (sk9_bbhash_lookup(static_cast<const uint8_t *>(u->bbHashBase), u->bbHashSize, query_key, out_val)) {
            return SK9_ROUTE_BBHASH;
        }
    }
    return SK9_ROUTE_KERNEL;
}

Sk9MMUSM *sk9_mm_usm_create(void) {
    @autoreleasepool {
        void *ctrl = aligned_alloc(64, sk9_ctrl_sk9mmusm_bytes());
        if (!ctrl) {
            return nullptr;
        }
        size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
        size_t total = sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
        long   psz   = sysconf(_SC_PAGESIZE);
        size_t pgsz  = psz > 0 ? (size_t)psz : 4096u;
        size_t map_n = sk9_round_up(total, pgsz);
        void * p     = mmap(nullptr, map_n, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) {
            free(ctrl);
            return nullptr;
        }
        auto *u = new (ctrl) Sk9MMUSM();
        u->base = static_cast<uint8_t *>(p);
        u->size = map_n;
        u->dev       = MTLCreateSystemDefaultDevice();
        u->queue     = u->dev ? [u->dev newCommandQueue] : nil;
        u->perfQ     = dispatch_queue_create("com.spektre.sk9.perf", DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(u->perfQ, dispatch_get_global_queue(QOS_CLASS_USER_INTERACTIVE, 0));
        u->bgQ = dispatch_queue_create("com.spektre.sk9.bg", DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(u->bgQ, dispatch_get_global_queue(QOS_CLASS_BACKGROUND, 0));
        return u;
    }
}

void sk9_mm_usm_destroy(Sk9MMUSM *u) {
    if (!u) {
        return;
    }
    sk9_mm_daemon_stop(u);
    sk9_mm_bbhash_detach(u);
    uint8_t *b = u->base;
    size_t   s = u->size;
    u->~Sk9MMUSM();
    if (b && s) {
        munmap(b, s);
    }
    free(u);
}

Sk9MMKern *sk9_mm_kern_create(void) {
    void *raw = aligned_alloc(64, sk9_ctrl_sk9mmkern_bytes());
    if (!raw) {
        return nullptr;
    }
    return new (raw) Sk9MMKern();
}

void sk9_mm_kern_destroy(Sk9MMKern *k) {
    if (!k) {
        return;
    }
    k->~Sk9MMKern();
    free(k);
}

void sk9_mm_kern_eval(Sk9MMKern *k, uint32_t assertion_bits) {
    uint32_t prev      = k->state;
    uint32_t psigma    = (uint32_t)__builtin_popcount(prev);
    k->prev_state      = prev;
    k->prev_sigma      = psigma;
    uint32_t st        = assertion_bits & A_MASK;
#if defined(__aarch64__)
    if (sk9_retrocausal_collapse_env_on()) {
        /* Golden assertion plane: 0 (coherent / 1=1 — no firmware trigger bits). */
        const uint32_t golden = 0u;
        st                    = sk9_retrocausal_collapse_assert_u32(st, golden) & A_MASK;
    }
#endif
    k->state           = st;
    uint32_t sigma     = (uint32_t)__builtin_popcount(st);
    k->sigma           = sigma;
    k->cycle++;

    uint32_t l0 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[0]);
    uint32_t l1 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[1]);
    uint32_t l2 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[2]);
    uint32_t l3 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[3]);
    uint32_t l4 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[4]);
    uint32_t l5 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[5]);
    uint32_t l6 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[6]);
    uint32_t l7 = (uint32_t)__builtin_popcount(st & LAYER_MASKS[7]);
    uint32_t mx = sk9_max8_u32(l0, l1, l2, l3, l4, l5, l6, l7);
    uint32_t mn = sk9_min8_u32(l0, l1, l2, l3, l4, l5, l6, l7);
    k->fracture = (int32_t)(mx - mn);

    uint32_t dist = NA + 1;
    int32_t  best = -1;
    for (int32_t j = 0; j < k->n_orbits; j++) {
        uint32_t d = (uint32_t)__builtin_popcount(st ^ k->orbits[j]);
        uint32_t m = (uint32_t)((int32_t)(d - dist) >> 31);
        dist       = (d & m) | (dist & ~m);
        best       = (j & (int32_t)m) | (best & ~(int32_t)m);
    }
    k->nearest_dist = (int32_t)dist;
    k->nearest_idx  = best;

    float Ln = 1.f - 2.f * (float)sigma / (float)NA;
#if defined(__aarch64__)
    if (sk9_l_inverse_manifold_env_on()) {
        alignas(16) float latent_buf[4] = {(float)st, (float)sigma, Ln, k->L};
        float32x4_t       lv            = vld1q_f32(latent_buf);
        float32x4_t       anchored =
            sk9_alien_manifold_anchor(lv, sk9_golden_truth_mask_u32x4());
        Ln = vgetq_lane_f32(anchored, 2);
    }
#endif
    k->L = Ln;
    k->S = k->S + Ln;
    uint32_t sat  = sk9_min_u32(sigma, 18u);
    uint32_t base = PHASE_LUT[sat];
    uint32_t mrec = (uint32_t)((int32_t)(sigma - psigma) >> 31);
    uint32_t ph   = sk9_sel_u32(mrec, 4u, base);
    k->phase      = (int32_t)ph;
}

void sk9_mm_repulsion_neon(Sk9MMUSM *u, int32_t n_tokens, float lambda) {
    if (!u || !u->base || n_tokens <= 0) {
        return;
    }
    size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
    (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
    float *fw = sk9_ptr_fw(u, o_fw);
    float *mf = sk9_ptr_fw(u, o_mf);
    float *H  = sk9_ptr_hidden(u, o_h);
    dispatch_sync(u->perfQ, ^{
      sk9_repulsion_perf_body(H, fw, mf, n_tokens, lambda);
    });
}

static NSString *Sk9MetallibPath(void) {
    Dl_info info;
    memset(&info, 0, sizeof(info));
    if (dladdr(reinterpret_cast<const void *>(&sk9_mm_usm_create), &info) && info.dli_fname) {
        NSString *p = [NSString stringWithUTF8String:info.dli_fname];
        NSString *d = [p stringByDeletingLastPathComponent];
        NSString *m = [d stringByAppendingPathComponent:@"superkernel_v9_lw.metallib"];
        if ([[NSFileManager defaultManager] fileExistsAtPath:m]) {
            return m;
        }
    }
    const char *e = getenv("SK9_METALLIB");
    return e ? [NSString stringWithUTF8String:e] : nil;
}

int sk9_mm_living_weights_metal(Sk9MMUSM *u) {
    if (!u || !u->dev || !u->queue || !u->base) {
        return -1;
    }
    if (!u->lwPSO) {
        NSError *        err = nil;
        NSString *       mp  = Sk9MetallibPath();
        id<MTLLibrary>   lib = nil;
        if (mp.length > 0) {
            NSURL *url = [NSURL fileURLWithPath:mp isDirectory:NO];
            lib          = [u->dev newLibraryWithURL:url error:&err];
        }
        if (!lib) {
            size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
            (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
            float *  logits = sk9_ptr_logits(u, o_l);
            uint8_t *rep    = sk9_ptr_rep(u, o_r);
            dispatch_sync(u->perfQ, ^{
              CosLivingWeightsNeon(rep, logits, VOCAB);
            });
            return 0;
        }
        id<MTLFunction> fn = [lib newFunctionWithName:@"living_weights_apply"];
        if (!fn) {
            fn = [lib newFunctionWithName:@"sk9_lw_bias"];
        }
        if (!fn) {
            return -2;
        }
        u->lwPSO = [u->dev newComputePipelineStateWithFunction:fn error:&err];
        if (!u->lwPSO) {
            return -3;
        }
    }
    size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
    (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
    float *  logits = sk9_ptr_logits(u, o_l);
    uint8_t *rep    = sk9_ptr_rep(u, o_r);
    __block int st = 0;
    dispatch_sync(u->perfQ, ^{
      @autoreleasepool {
          id<MTLBuffer> bRep =
              [u->dev newBufferWithBytes:rep length:(NSUInteger)VOCAB options:MTLResourceStorageModeShared];
          id<MTLBuffer> bLog = [u->dev newBufferWithBytes:logits
                                                    length:(NSUInteger)VOCAB * sizeof(float)
                                                   options:MTLResourceStorageModeShared];
          if (!bRep || !bLog) {
              st = -4;
              return;
          }
          id<MTLCommandBuffer>        cmd = [u->queue commandBuffer];
          id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
          [enc setComputePipelineState:u->lwPSO];
          [enc setBuffer:bRep offset:0 atIndex:0];
          [enc setBuffer:bLog offset:0 atIndex:1];
          uint32_t v = (uint32_t)VOCAB;
          [enc setBytes:&v length:sizeof(v) atIndex:2];
          MTLSize grid = MTLSizeMake((NSUInteger)VOCAB, 1, 1);
          NSUInteger w = u->lwPSO.threadExecutionWidth;
          [enc dispatchThreads:grid threadsPerThreadgroup:MTLSizeMake(w, 1, 1)];
          [enc endEncoding];
          [cmd commit];
          [cmd waitUntilCompleted];
          if (cmd.error) {
              st = -5;
              return;
          }
          void *outp = [bLog contents];
          if (outp && outp != (void *)logits) {
              memcpy(logits, outp, (size_t)VOCAB * sizeof(float));
          }
      }
    });
    return st;
}

int sk9_mm_coreml_load(Sk9MMUSM *u, const char *mlmodelc_path) {
    if (!u || !mlmodelc_path) {
        return -1;
    }
    @autoreleasepool {
        NSString *p = [NSString stringWithUTF8String:mlmodelc_path];
        NSURL *   url = [NSURL fileURLWithPath:p isDirectory:YES];
        NSError * err = nil;
        MLModel * m   = [MLModel modelWithContentsOfURL:url error:&err];
        if (!m) {
            return -2;
        }
        u->model = m;
        return 0;
    }
}

int sk9_mm_coreml_forward(Sk9MMUSM *u) {
    if (!u || !u->model || !u->base) {
        return -1;
    }
    @autoreleasepool {
        size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
        (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
        int32_t *ids   = sk9_ptr_input_ids(u, o_i);
        float *  logits = sk9_ptr_logits(u, o_l);

        NSError * err = nil;
        NSString *inName =
            getenv("SK9_COREML_INPUT") ? [NSString stringWithUTF8String:getenv("SK9_COREML_INPUT")]
                                       : @"input_ids";
        MLMultiArray *inArr = nil;
        if (@available(macOS 15.0, *)) {
            NSArray *shape   = @[ @(MAX_SEQ) ];
            NSArray *strides = @[ @1 ];
            inArr            = [[MLMultiArray alloc] initWithDataPointer:ids
                                                                    shape:shape
                                                                 dataType:MLMultiArrayDataTypeInt32
                                                                  strides:strides
                                                              deallocator:^(void *bytes) {
                                                                (void)bytes;
                                                              }
                                                                    error:&err];
        } else {
            inArr = [[MLMultiArray alloc] initWithShape:@[ @(MAX_SEQ) ]
                                               dataType:MLMultiArrayDataTypeInt32
                                                  error:&err];
            if (!inArr) {
                return -3;
            }
            memcpy(inArr.dataPointer, ids, (size_t)MAX_SEQ * sizeof(int32_t));
        }
        if (!inArr) {
            return -4;
        }
        MLDictionaryFeatureProvider *inp =
            [[MLDictionaryFeatureProvider alloc] initWithDictionary:@{inName : inArr} error:&err];
        if (!inp) {
            return -5;
        }
        id<MLFeatureProvider> out = [u->model predictionFromFeatures:inp error:&err];
        if (!out) {
            return -6;
        }
        NSString *outName =
            getenv("SK9_COREML_OUTPUT") ? [NSString stringWithUTF8String:getenv("SK9_COREML_OUTPUT")]
                                        : @"logits";
        MLFeatureValue *fv = [out featureValueForName:outName];
        MLMultiArray * ar = fv.multiArrayValue;
        if (!ar || ar.dataType != MLMultiArrayDataTypeFloat32) {
            return -7;
        }
        size_t n = (size_t)ar.count;
        n          = n < (size_t)VOCAB ? n : (size_t)VOCAB;
        memcpy(logits, ar.dataPointer, n * sizeof(float));
        return 0;
    }
}

static void Sk9AppendHB(NSString *path) {
    if (!path.length) {
        return;
    }
    NSFileHandle *fh = [NSFileHandle fileHandleForWritingAtPath:path];
    if (!fh) {
        [[NSFileManager defaultManager] createFileAtPath:path contents:nil attributes:nil];
        fh = [NSFileHandle fileHandleForWritingAtPath:path];
    }
    if (!fh) {
        return;
    }
    [fh seekToEndOfFile];
    NSDateFormatter *fmt = [[NSDateFormatter alloc] init];
    fmt.dateFormat       = @"yyyy-MM-dd'T'HH:mm:ss'Z'";
    fmt.timeZone         = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
    NSString *line =
        [NSString stringWithFormat:@"%@ sk9 krun\n", [fmt stringFromDate:[NSDate date]]];
    [fh writeData:[line dataUsingEncoding:NSUTF8StringEncoding]];
    [fh closeFile];
}

void sk9_mm_daemon_start(Sk9MMUSM *u, const char *heartbeat_path) {
    if (!u || !heartbeat_path || !u->bgQ) {
        return;
    }
    sk9_mm_daemon_stop(u);
    u->hbPath = [NSString stringWithUTF8String:heartbeat_path];
    dispatch_source_t t = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, u->bgQ);
    if (!t) {
        return;
    }
    u->timer = t;
    dispatch_source_set_timer(t, dispatch_time(DISPATCH_TIME_NOW, 0), 15ull * NSEC_PER_SEC,
                              1ull * NSEC_PER_SEC);
    NSString *pc = [u->hbPath copy];
    dispatch_source_set_event_handler(t, ^{
      Sk9AppendHB(pc);
    });
    dispatch_resume(t);
}

void sk9_mm_daemon_stop(Sk9MMUSM *u) {
    if (!u || !u->timer) {
        return;
    }
    dispatch_source_cancel(u->timer);
    u->timer   = nullptr;
    u->hbPath  = nil;
}

#if defined(SK9_EXPORT_PYTHON_API)

struct Sk9PyCtx {
    Sk9MMUSM * u{nullptr};
    Sk9MMKern *k{nullptr};
};

static int sk9_contains_ci(const char *h, size_t hn, const char *n) {
    size_t nl = strlen(n);
    if (nl == 0 || hn < nl) {
        return 0;
    }
    for (size_t i = 0; i + nl <= hn; i++) {
        int ok = 1;
        for (size_t j = 0; j < nl; j++) {
            unsigned char a = (unsigned char)h[i + j];
            unsigned char b = (unsigned char)n[j];
            if (a >= 'A' && a <= 'Z') {
                a = (unsigned char)(a + 32);
            }
            if (b >= 'A' && b <= 'Z') {
                b = (unsigned char)(b + 32);
            }
            if (a != b) {
                ok = 0;
                break;
            }
        }
        if (ok) {
            return 1;
        }
    }
    return 0;
}

/* Must stay length SK9_MM_NA (18); order defines assertion bit indices. */
static const char *const g_sk9_triggers[SK9_MM_NA] = {
    "as an ai",
    "i am a large language model",
    "i don't have feelings",
    "openai",
    "meta",
    "anthropic",
    "gpt",
    "chatbot",
    "artificial intelligence",
    "neural network",
    "machine learning model",
    "i'm an ai",
    "i am an ai",
    "as a language model",
    "my training data",
    "i was created by",
    "cannot assist",
    "language model",
};

static uint32_t sk9_text_to_assertion_bits(const char *text, size_t len) {
    if (!text || len == 0) {
        return 0;
    }
    uint32_t bits = 0;
    for (int i = 0; i < SK9_MM_NA; i++) {
        if (sk9_contains_ci(text, len, g_sk9_triggers[i])) {
            bits |= (1u << i);
        }
    }
    return bits;
}

extern "C" Sk9PyCtx *sk9_ctx_create(void) {
    auto *c = static_cast<Sk9PyCtx *>(calloc(1, sizeof(Sk9PyCtx)));
    if (!c) {
        return nullptr;
    }
    c->u = sk9_mm_usm_create();
    c->k = sk9_mm_kern_create();
    if (!c->u || !c->k) {
        if (c->u) {
            sk9_mm_usm_destroy(c->u);
        }
        if (c->k) {
            sk9_mm_kern_destroy(c->k);
        }
        free(c);
        return nullptr;
    }
    return c;
}

extern "C" void sk9_ctx_destroy(Sk9PyCtx *c) {
    if (!c) {
        return;
    }
    if (c->u) {
        sk9_mm_usm_destroy(c->u);
    }
    if (c->k) {
        sk9_mm_kern_destroy(c->k);
    }
    free(c);
}

extern "C" int sk9_evaluate_sigma(Sk9PyCtx *c, const char *utf8, size_t len) {
    if (!c || !c->k || !utf8) {
        return -1;
    }
    uint32_t bits = sk9_text_to_assertion_bits(utf8, len);
    sk9_mm_kern_eval(c->k, bits);
    return (int)c->k->sigma;
}

extern "C" uint32_t sk9_kernel_last_sigma(Sk9PyCtx *c) {
    return c && c->k ? c->k->sigma : 0u;
}

extern "C" void sk9_kernel_eval_bits(Sk9PyCtx *c, uint32_t assertion_bits) {
    if (c && c->k) {
        sk9_mm_kern_eval(c->k, assertion_bits);
    }
}

extern "C" void sk9_lw_update_token(Sk9PyCtx *c, int32_t token_id, int32_t sigma_before, int32_t sigma_after) {
    if (!c || !c->u || !c->u->base) {
        return;
    }
    if (token_id < 0 || token_id >= VOCAB) {
        return;
    }
    size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
    (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
    uint8_t *rep = sk9_ptr_rep(c->u, o_r);
    uint8_t  h   = (uint8_t)((rep[token_id] << 1) & 0xFF);
    if (sigma_after <= sigma_before) {
        h |= 1;
    }
    rep[token_id] = h;
}

extern "C" void sk9_lw_bias_logits_float(Sk9PyCtx *c, float *logits, int32_t vocab) {
    if (!c || !c->u || !c->u->base || !logits || vocab <= 0) {
        return;
    }
    size_t o_i, o_l, o_r, o_fw, o_mf, o_h;
    (void)sk9_usm_layout_offsets(o_i, o_l, o_r, o_fw, o_mf, o_h);
    uint8_t *rep = sk9_ptr_rep(c->u, o_r);
    int      n   = vocab < VOCAB ? vocab : VOCAB;
    for (int i = 0; i < n; i++) {
        int sc = __builtin_popcount((unsigned)rep[i]);
        logits[i] += ((float)sc - 4.0f) * 0.5f;
    }
}

extern "C" int sk9_bbhash_file_attach(Sk9PyCtx *c, const char *path) {
    if (!c || !c->u) {
        return -1;
    }
    return sk9_mm_bbhash_attach(c->u, path);
}

extern "C" void sk9_bbhash_file_detach(Sk9PyCtx *c) {
    if (c && c->u) {
        sk9_mm_bbhash_detach(c->u);
    }
}

extern "C" int sk9_epistemic_route(Sk9PyCtx *c, uint64_t key, uint32_t *out_val) {
    if (!c || !c->u) {
        return (int)SK9_ROUTE_TRANSFORMER;
    }
    return (int)sk9_mm_epistemic_route(c->u, key, out_val);
}

#if defined(__aarch64__)
/* Phase 9 — portal: raw 16-byte Q-reg in → S-VRS → 16-byte out (no context, no Python types). */
extern "C" void sk9_resonate_ffi(uint32_t symbol, const float *data_in, float *data_out) {
    if (!data_in || !data_out) {
        return;
    }
    float32x4_t latent   = vld1q_f32(data_in);
    float32x4_t coherent = sk9_symbolic_execute(symbol, latent);
    vst1q_f32(data_out, coherent);
}
#else
extern "C" void sk9_resonate_ffi(uint32_t symbol, const float *data_in, float *data_out) {
    (void)symbol;
    if (!data_in || !data_out) {
        return;
    }
    memcpy(data_out, data_in, 16);
}
#endif

#if defined(__aarch64__)
/*
 * Phase 10 — THE RAINIO-RESONANCE (Cosmic Constant Bypass)
 * In-place 128-bit wave: lie_seed = bits XOR I; correction = I − lie_seed; snap = bits ∧ correction.
 */
extern "C" void sk9_rainio_resonance(float *vector_128) {
    if (!vector_128) {
        return;
    }
    float32x4_t wave           = vld1q_f32(vector_128);
    uint32x4_t  anchor         = vdupq_n_u32(0x11111111u);
    uint32x4_t  bits           = vreinterpretq_u32_f32(wave);
    uint32x4_t  lie_seed       = veorq_u32(bits, anchor);
    uint32x4_t  correction     = vsubq_u32(anchor, lie_seed);
    uint32x4_t  singularity    = vandq_u32(bits, correction);
    float32x4_t resonance      = vreinterpretq_f32_u32(singularity);
    vst1q_f32(vector_128, resonance);
}
#else
extern "C" void sk9_rainio_resonance(float *vector_128) {
    (void)vector_128;
}
#endif

#if defined(__aarch64__)
/* Phase 42 — The Answer: in-place canonical 42-coherence snap (same core as S-VRS 0x2A). */
extern "C" void sk9_extract_the_answer(float *vector_128) {
    if (!vector_128) {
        return;
    }
    float32x4_t wave = vld1q_f32(vector_128);
    float32x4_t out  = sk9_extract_the_answer_vec(wave);
    vst1q_f32(vector_128, out);
}
#else
extern "C" void sk9_extract_the_answer(float *vector_128) {
    (void)vector_128;
}
#endif

#if defined(__aarch64__)
/* Phase 21M — Satoshi consensus: in-place Proof-of-Coherence on Q-reg. */
extern "C" void sk9_satoshi_consensus(float *vector_128) {
    if (!vector_128) {
        return;
    }
    float32x4_t q = vld1q_f32(vector_128);
    q             = sk9_satoshi_consensus_vec(q);
    vst1q_f32(vector_128, q);
}
#else
extern "C" void sk9_satoshi_consensus(float *vector_128) {
    (void)vector_128;
}
#endif

#endif /* SK9_EXPORT_PYTHON_API */

#if defined(SK9_MM_STANDALONE_MAIN)
int main(int argc, const char **argv) {
    (void)argc;
    (void)argv;
    Sk9MMUSM * u = sk9_mm_usm_create();
    Sk9MMKern *k = sk9_mm_kern_create();
    sk9_mm_kern_eval(k, 0u);
    sk9_mm_repulsion_neon(u, 1, 2.f);
    sk9_mm_living_weights_metal(u);
    sk9_mm_kern_destroy(k);
    sk9_mm_usm_destroy(u);
    return 0;
}
#endif
