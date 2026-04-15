/* SPDX-License-Identifier: AGPL-3.0-or-later
 * σ invariants and structural checks (directive spec).
 */
#include "../core/creation_os.h"
#if defined(__aarch64__)
#include "../core/cos_neon_hamming.h"
#endif
#include "../core/cos_neon_retrieval.h"
#include "../core/cos_parliament.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int approx(float x, float y, float tol)
{
    return fabsf(x - y) <= tol;
}

static void report(const char *name, int pass, float measured, const char *detail)
{
    printf("%s %s measured=%.6f %s\n", pass ? "PASS" : "FAIL", name, measured, detail);
}

int main(void)
{
    int fails = 0;
    uint64_t rng = 0xC0FFEEULL;
    uint64_t x[COS_W], y[COS_W], nx[COS_W], m[COS_W], r[COS_W];
    cos_hv_random(&rng, x);
    cos_hv_random(&rng, y);
    for (int i = 0; i < COS_W; i++)
        nx[i] = ~x[i];

    float s_xx = cos_hv_sigma(x, x);
    int p1 = s_xx < 1e-5f;
    if (!p1)
        fails++;
    report("sigma(x,x)==0", p1, s_xx, "(identity)");

    float s_xnx = cos_hv_sigma(x, nx);
    int p2 = approx(s_xnx, 1.0f, 0.02f);
    if (!p2)
        fails++;
    report("sigma(x,NOT x)==1", p2, s_xnx, "(max contradiction)");

    float sum_r = 0;
    int trials = 200;
    for (int t = 0; t < trials; t++) {
        cos_hv_random(&rng, r);
        sum_r += cos_hv_sigma(x, r);
    }
    float mean_r = sum_r / (float)trials;
    int p3 = mean_r > 0.15f && mean_r < 0.30f;
    if (!p3)
        fails++;
    report("sigma(x,random)", p3, mean_r, "expect ~0.22 band");

    cos_hv_maj3(m, x, x, y);
    float s_maj = cos_hv_sigma(m, x);
    int p4 = s_maj < 0.02f;
    if (!p4)
        fails++;
    report("sigma(MAJ(x,x,y),x)<0.01", p4, s_maj, "(superposition)");

    uint64_t bound[COS_W];
    for (int i = 0; i < COS_W; i++)
        bound[i] = x[i] ^ y[i];
    float s_xor = cos_hv_sigma(bound, x);
    int p5 = s_xor > 0.15f && s_xor < 0.30f;
    if (!p5)
        fails++;
    report("sigma(XOR(x,y),x)", p5, s_xor, "(binding ~orthogonal)");

    /* ROTL: position 0 is identity on hypervector */
    uint64_t rot0[COS_W];
    cos_hv_rotl(rot0, x, 0);
    float s_rot = cos_hv_sigma(x, rot0);
    int pr = s_rot < 1e-5f;
    if (!pr)
        fails++;
    report("ROTL(x,0)==x", pr, s_rot, "(positional encoding base case)");

    /* Noether: XOR-sum of multiset conserved under pairwise swap */
    uint64_t particles[8][COS_W];
    for (int k = 0; k < 8; k++)
        cos_hv_random(&rng, particles[k]);
    uint64_t sum0[COS_W], sum1[COS_W];
    memset(sum0, 0, sizeof(sum0));
    for (int ii = 0; ii < 8; ii++)
        for (int w = 0; w < COS_W; w++)
            sum0[w] ^= particles[ii][w];
    int ia = 2, ja = 5;
    uint64_t inter[COS_W];
    for (int w = 0; w < COS_W; w++)
        inter[w] = particles[ia][w] ^ particles[ja][w];
    for (int w = 0; w < COS_W; w++) {
        particles[ia][w] ^= inter[w];
        particles[ja][w] ^= inter[w];
    }
    memset(sum1, 0, sizeof(sum1));
    for (int k = 0; k < 8; k++)
        for (int w = 0; w < COS_W; w++)
            sum1[w] ^= particles[k][w];
    float s_noether = cos_hv_sigma(sum0, sum1);
    int p6 = s_noether < 1e-5f;
    if (!p6)
        fails++;
    report("Noether XOR-sum", p6, s_noether, "after symmetric interaction");

    /* Crystal lock: single-bit flip on signature should not recover */
    uint64_t identity[COS_W], key[COS_W], sig[COS_W], rec[COS_W];
    cos_hv_random(&rng, identity);
    cos_hv_random(&rng, key);
    for (int w = 0; w < COS_W; w++)
        sig[w] = identity[w] ^ key[w];
    for (int w = 0; w < COS_W; w++)
        rec[w] = sig[w] ^ key[w];
    float s_ok = cos_hv_sigma(identity, rec);
    uint64_t tam[COS_W];
    memcpy(tam, sig, sizeof(tam));
    tam[0] ^= 1ULL;
    uint64_t rec2[COS_W];
    for (int w = 0; w < COS_W; w++)
        rec2[w] = tam[w] ^ key[w];
    float s_bad = cos_hv_sigma(identity, rec2);
    int tamper_bytes = memcmp(identity, rec2, sizeof(identity)) != 0;
    int p7 = s_ok < 0.001f && tamper_bytes;
    if (!p7)
        fails++;
    report("Crystal lock", p7, s_bad, "memcmp(identity,rec2)!=0 after 1-bit sig flip");

#if defined(__aarch64__)
    uint32_t h_sc = cos_hv_hamming(x, y);
    uint32_t h_hw = cos_hv_hamming_hw(x, y);
    int p8 = (h_sc == h_hw);
    if (!p8)
        fails++;
    report("NEON Hamming==scalar", p8, (float)h_hw, "(AArch64 hardware path)");

    uint64_t maj_s[COS_W], maj_h[COS_W];
    cos_hv_maj3(maj_s, x, y, nx);
    cos_hv_maj3_hw(maj_h, x, y, nx);
    int p_maj = memcmp(maj_s, maj_h, sizeof(maj_s)) == 0;
    if (!p_maj)
        fails++;
    report("NEON MAJ3==scalar", p_maj, 0.0f, "(bundle)");

    uint64_t xb[COS_W], xbh[COS_W];
    for (int w = 0; w < COS_W; w++)
        xb[w] = x[w] ^ y[w];
    cos_hv_bind_xor_hw(xbh, x, y);
    int p_bind = memcmp(xb, xbh, sizeof(xb)) == 0;
    if (!p_bind)
        fails++;
    report("NEON XOR bind==scalar", p_bind, 0.0f, "(association)");

    uint64_t acc[COS_W], acc_h[COS_W], add[COS_W];
    memcpy(acc, x, sizeof(acc));
    memcpy(acc_h, x, sizeof(acc_h));
    cos_hv_random(&rng, add);
    for (int w = 0; w < COS_W; w++)
        acc[w] ^= add[w];
    cos_hv_xor_inplace_hw(acc_h, add);
    int p_xip = memcmp(acc, acc_h, sizeof(acc)) == 0;
    if (!p_xip)
        fails++;
    report("NEON XOR inplace==scalar", p_xip, 0.0f, "(accumulator bind)");

    float s_hw = cos_hv_sigma_hw(x, y);
    int p_sig = approx(cos_hv_sigma(x, y), s_hw, 1e-6f);
    if (!p_sig)
        fails++;
    report("NEON sigma==scalar", p_sig, s_hw, "(Hamming-derived)");

    uint64_t trace[COS_W];
    memset(trace, 0, sizeof(trace));
    for (int w = 0; w < 4; w++)
        trace[w] = ~0ULL;
    uint32_t mm_s = 0;
    for (int w = 0; w < COS_W; w++)
        mm_s += (uint32_t)__builtin_popcountll((x[w] ^ y[w]) & trace[w]);
    uint32_t mm_h = cos_hv_measurement_mismatch_hw(x, y, trace);
    int p_mm = (mm_s == mm_h);
    if (!p_mm)
        fails++;
    report("NEON projector==scalar", p_mm, (float)mm_h, "(256-bit trace)");
#endif

    /* K=3 bit-parliament equals per-lane MAJ3 (same bit geometry). */
    uint64_t pa[COS_W], pb[COS_W], pc[COS_W], pmaj[COS_W], pout[COS_W];
    cos_hv_random(&rng, pa);
    cos_hv_random(&rng, pb);
    cos_hv_random(&rng, pc);
    uint64_t rows3[3 * COS_W];
    memcpy(rows3, pa, sizeof(pa));
    memcpy(rows3 + COS_W, pb, sizeof(pb));
    memcpy(rows3 + 2 * COS_W, pc, sizeof(pc));
    cos_hv_bit_parliament(pout, rows3, 3);
    cos_hv_maj3(pmaj, pa, pb, pc);
    int p_par = memcmp(pout, pmaj, sizeof(pout)) == 0;
    if (!p_par)
        fails++;
    report("Parliament K=3==MAJ3", p_par, 0.0f, "(bit-majority)");

    /* Argmin over small bank: row 0 equals query. */
    uint64_t qv[COS_W], bank[16 * COS_W];
    cos_hv_random(&rng, qv);
    for (int r = 0; r < 16; r++)
        cos_hv_random(&rng, bank + (size_t)r * COS_W);
    memcpy(bank, qv, sizeof(qv));
    int arg = cos_hv_argmin_hamming_rows_hw(qv, bank, 16);
    int p_arg = (arg == 0);
    if (!p_arg)
        fails++;
    report("Argmin bank hit row0", p_arg, (float)arg, "(NEON or scalar Hamming)");

    printf("\nSummary: %d tests failed (0 ok)\n", fails);
    return fails ? 1 : 0;
}
