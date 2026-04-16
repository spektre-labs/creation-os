/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v61 — OpenBSD pledge/unveil wrapper.
 *
 * Usage: compile with `cc -o pledged pledged.c` on OpenBSD, then run
 * the Shield binary under it.  This file is a *reference stub* — on
 * Linux / macOS it compiles to a no-op that returns 0.
 *
 *   ./pledged stdio creation_os_v61 --self-test
 *
 * Tier claim: I (interpreted, documented).  Not exercised in CI
 * because no OpenBSD runner is in the free-tier matrix.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s <promises> <cmd> [args...]\n"
                "  promises: e.g. 'stdio rpath wpath proc exec'\n",
                argv[0]);
        return 2;
    }
    const char *promises = argv[1];

#if defined(__OpenBSD__)
    /* pledge() is OpenBSD only.  On first call it sets the promise
     * set; on second call it can only reduce, never grow. */
    extern int pledge(const char *promises, const char *execpromises);
    if (pledge(promises, "stdio exec") == -1) {
        fprintf(stderr, "pledge failed: %s\n", strerror(errno));
        return 1;
    }
#else
    (void)promises;
    fprintf(stderr, "pledge-stub: SKIP (not OpenBSD; running as passthrough)\n");
#endif

    execvp(argv[2], argv + 2);
    fprintf(stderr, "exec failed: %s\n", strerror(errno));
    return 127;
}
