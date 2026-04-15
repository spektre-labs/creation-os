/*
 * CREATION OS — bare loop: Tier table + JIT + optional Genesis + Metal smoke.
 * σ → ⊕ | 1 = 1
 */
#include "ane/orion_integration.h"
#include "bare_metal/gda_hash.h"
#include "bare_metal/neon_gemv.h"
#include "jit/jit_engine.h"
#include "socket/genesis_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern int creation_os_metal_eval_smoke(const char *metallib_path);

static void print_tiers(void) {
    static const char *names[] = {"BBHash", "Software", "Shallow", "Mid", "Full", "Rejection"};
    printf("Moσ priority (superkernel_v9_m4.h):\n");
    for (int i = 0; i < 6; i++)
        printf("  tier %d — %s\n", i, names[i]);
}

static int run_jit_demo(void) {
    uint8_t buf[64];
    int n = jit_emit_return_imm64(buf, sizeof buf, 0x0101010101010101ull);
    if (n < 0) {
        fprintf(stderr, "jit: emit failed\n");
        return -1;
    }
    uint64_t r = 0;
    if (jit_ephemeral_exec(buf, (size_t)n, &r) != 0) {
        fprintf(stderr, "jit: MAP_JIT exec failed (sign with CreationOS.entitlements?)\n");
        return -1;
    }
    printf("jit: ephemeral return 0x%016llx\n", (unsigned long long)r);
    return 0;
}

static int run_neon_demo(void) {
    float a[16], x[16];
    for (int i = 0; i < 16; i++) {
        a[i] = 1.0f;
        x[i] = (float)(i + 1);
    }
    float d = neon_gemv_dot(a, x, 16);
    printf("neon_gemv: dot(dim=16) = %g\n", (double)d);
    return 0;
}

static int run_orion_stub(void) {
    float in[8], out[8];
    for (int i = 0; i < 8; i++)
        in[i] = (float)i;
    int rc = creation_os_orion_ane_forward_stub(in, out, 8);
    printf("orion stub: rc=%d out[0]=%g\n", rc, (double)out[0]);
    return 0;
}

static int run_gda_demo(void) {
    const char *q = "1=1";
    uint64_t k = 0;
    if (gda_hash_query((const uint8_t *)q, strlen(q), &k) != 0)
        return -1;
    printf("gda_hash: \"1=1\" -> 0x%016llx\n", (unsigned long long)k);
    return 0;
}

int main(int argc, char **argv) {
    const char *bbhash_path = NULL;
    int do_genesis = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--serve-genesis"))
            do_genesis = 1;
        else if (!strcmp(argv[i], "--bbhash") && i + 1 < argc)
            bbhash_path = argv[++i];
        else if (!strcmp(argv[i], "--help")) {
            printf("usage: %s [--serve-genesis] [--bbhash path]\n", argv[0]);
            return 0;
        }
    }

    print_tiers();
    (void)run_gda_demo();
    (void)run_neon_demo();
    (void)run_orion_stub();
    (void)run_jit_demo();

    const char *ml = "build/creation_os.metallib";
    if (creation_os_metal_eval_smoke(ml) == 0)
        printf("metal: eval kernel smoke OK (%s)\n", ml);
    else
        printf("metal: skip or failed (build metallib first: make metallib)\n");

    if (do_genesis)
        return genesis_socket_serve_forever(NULL, bbhash_path) != 0;

    return 0;
}
