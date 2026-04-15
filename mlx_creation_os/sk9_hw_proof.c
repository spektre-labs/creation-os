/*
 * Hardware-anchored AMX pulse proof: ARM cntpct_el0 delta + exact IEEE double(1.0) bit check.
 * No private KPC frameworks — cycle count is the generic timer; instruction retired needs
 * privileged counters or Instruments.
 */
#include "amx_aarch64.h"

#include <mach/mach_time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__) && defined(__arm64__)

extern void sk9_bare_amx_enter(void);
extern void sk9_bare_amx_exit(void);
extern void sk9_bare_amx_fma32_vec_z0_pulse(void);
extern void sk9_thread_rt_begin(void);
extern void sk9_thread_rt_end(void);
extern int sk9_amx_pulse_fixedpoint_int31_identity(void);

static uint64_t sk9_cntpct(void) {
    uint64_t v;
    __asm__ volatile("isb\n mrs %0, cntpct_el0" : "=r"(v) : : "memory");
    return v;
}

static uint64_t fpcr_dn_on(void) {
    uint64_t old_fpcr, new_fpcr;
    __asm__ volatile("mrs %0, fpcr" : "=r"(old_fpcr));
    new_fpcr = old_fpcr | (1ull << 25);
    __asm__ volatile("msr fpcr, %0" : : "r"(new_fpcr));
    return old_fpcr;
}

static void fpcr_restore(uint64_t fpcr) { __asm__ volatile("msr fpcr, %0" : : "r"(fpcr)); }

static void pack_unit(float *x16, float *y16) {
    for (int i = 0; i < 16; i++) {
        x16[i] = 1.0f;
        y16[i] = 1.0f;
    }
}

static int env_truth_fail_abort(void) {
    const char *e = getenv("CREATION_OS_HARD_FAIL");
    return e && e[0] != '0' && strcmp(e, "false") != 0 && strcmp(e, "no") != 0;
}

/* Returns 1 iff (double)z_bits_as_float has exact IEEE 1.0 encoding. */
static int double_one_exact_from_f32(float z) {
    double d = (double)z;
    uint64_t u;
    memcpy(&u, &d, sizeof(u));
    return u == 0x3FF0000000000000ull ? 1 : 0;
}

/*
 * Run AMX unit pulse; fill *cyc with cntpct delta, *ns with converted mach time window.
 * Return bitmask: 1 = cycle delta > CREATION_OS_MAX_PULSE_CYCLES (0 disables),
 * 2 = double bits of (double)z0 != 0x3ff0000000000000, 4 = Q31 int identity asm fail.
 * If CREATION_OS_HARD_FAIL and (2|4), __builtin_trap().
 */
int sk9_hw_proof_amx_pulse(uint64_t *cyc_out, uint64_t *ns_out, int *exact_one_out) {
    static _Alignas(256) uint8_t xy_blob[512];
    static _Alignas(256) uint8_t z_scratch[256];

    if (cyc_out)
        *cyc_out = 0;
    if (ns_out)
        *ns_out = 0;
    if (exact_one_out)
        *exact_one_out = 0;

    uint64_t max_cyc = 500000;
    const char *mc = getenv("CREATION_OS_MAX_PULSE_CYCLES");
    if (mc && mc[0])
        max_cyc = (uint64_t)strtoull(mc, NULL, 10);

    memset(xy_blob, 0, sizeof(xy_blob));
    memset(z_scratch, 0, sizeof(z_scratch));
    pack_unit((float *)xy_blob, (float *)(xy_blob + 256));

    mach_timebase_info_data_t tb;
    mach_timebase_info(&tb);

    sk9_thread_rt_begin();

    uint64_t t0 = mach_absolute_time();
    uint64_t c0 = sk9_cntpct();
    uint64_t fpc = fpcr_dn_on();
    sk9_bare_amx_enter();
    AMX_LDX(AMX_PTR_ROW_FLAGS(xy_blob, 0, 1));
    AMX_LDY(AMX_PTR_ROW_FLAGS(xy_blob + 256, 0, 1));
    sk9_bare_amx_fma32_vec_z0_pulse();
    AMX_STZ(AMX_PTR_ROW_FLAGS(z_scratch, 0, 0));
    sk9_bare_amx_exit();
    fpcr_restore(fpc);
    uint64_t c1 = sk9_cntpct();
    uint64_t t1 = mach_absolute_time();

    sk9_thread_rt_end();

    uint64_t cyc = c1 - c0;
    uint64_t ns = (t1 - t0) * (uint64_t)tb.numer / (uint64_t)tb.denom;
    if (cyc_out)
        *cyc_out = cyc;
    if (ns_out)
        *ns_out = ns;

    float z0;
    memcpy(&z0, z_scratch, sizeof(z0));
    int ok1 = double_one_exact_from_f32(z0);
    int ok31 = sk9_amx_pulse_fixedpoint_int31_identity();
    if (exact_one_out)
        *exact_one_out = (ok1 && ok31) ? 1 : 0;

    int dirty_cyc = (max_cyc > 0 && cyc > max_cyc) ? 1 : 0;
    int bad_bit = ok1 ? 0 : 2;
    int bad_int = ok31 ? 0 : 4;

    if ((bad_bit | bad_int) && env_truth_fail_abort())
        __builtin_trap();

    return dirty_cyc | bad_bit | bad_int;
}

#else

int sk9_hw_proof_amx_pulse(uint64_t *cyc_out, uint64_t *ns_out, int *exact_one_out) {
    (void)cyc_out;
    (void)ns_out;
    (void)exact_one_out;
    return 0;
}

#endif
