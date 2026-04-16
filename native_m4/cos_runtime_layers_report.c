/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "creation_os_native_m4.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#include <sys/utsname.h>
#endif

static void print_metallib_probe(FILE *f)
{
    static const char *const paths[] = {
        "native_m4/creation_os_lw.metallib",
        "./native_m4/creation_os_lw.metallib",
        "creation_os_lw.metallib",
    };
    fprintf(f, "layer.kernel.metallib_candidates:\n");
    for (size_t i = 0; i < sizeof(paths) / sizeof(paths[0]); i++) {
#if !defined(_WIN32)
        int ok = (access(paths[i], R_OK) == 0);
#else
        int ok = 0;
#endif
        fprintf(f, "  - path=%s readable=%s\n", paths[i], ok ? "yes" : "no");
    }
    {
        const char *env = getenv("CREATION_OS_METALLIB");
        fprintf(f, "layer.kernel.CREATION_OS_METALLIB=%s\n", env && env[0] ? env : "(unset)");
    }
}

void cos_runtime_layers_report_print(FILE *f)
{
    if (!f)
        return;

    fprintf(f, "# creation_os_native_m4 — layers report (lab binary; not merge-gate)\n");
    fprintf(f, "# Re-run on target hardware after `make native-m4`.\n\n");

#if !defined(_WIN32)
    {
        struct utsname u;
        if (uname(&u) == 0) {
            fprintf(f, "layer.kernel.uname.sysname=%s\n", u.sysname);
            fprintf(f, "layer.kernel.uname.release=%s\n", u.release);
            fprintf(f, "layer.kernel.uname.machine=%s\n", u.machine);
        }
    }
#endif

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    fprintf(f, "layer.kernel.neon_compiled=yes\n");
#else
    fprintf(f, "layer.kernel.neon_compiled=no\n");
#endif

    fprintf(f, "layer.kernel.sme_runtime_probe=%s\n", cos_runtime_has_sme() ? "yes" : "no");

    {
        size_t rb = 0, lb = 0;
        cos_lw_buffer_sizes(65536, &rb, &lb);
        fprintf(f, "layer.kernel.buffer_sizes_example.vocab=65536 reputation_bytes=%zu logits_bytes=%zu\n", rb, lb);
    }

    print_metallib_probe(f);

    fprintf(f, "\n# Interpretation keys (see README “Capability layers” + docs/WHAT_IS_REAL.md):\n");
    fprintf(f, "# - kernel/runtime: this file + native_m4 self-test/bench quantify local helpers.\n");
    fprintf(f, "# - model/system/product: not fully represented by this binary; README table is canonical.\n");
}
