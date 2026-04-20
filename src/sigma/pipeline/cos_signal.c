/* σ-Signal — graceful shutdown implementation.  See signal.h for
 * the contract; this file is strict about async-signal-safety:
 * the handler touches only sig_atomic_t and write(2).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "cos_signal.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

/* Shared state.  The handler only ever writes to the flag + signum;
 * the main thread reads / clears them. */
static volatile sig_atomic_t g_shutdown_flag   = 0;
static volatile sig_atomic_t g_latched_signum  = 0;
static cos_sigma_shutdown_fn g_hook            = NULL;
static void                 *g_hook_ctx        = NULL;
static int                   g_installed       = 0;

/* Async-signal-safe handler: write(2) a short breadcrumb and set
 * the flag.  No printf, no malloc, no locks. */
static void sigma_signal_handler(int signum) {
    /* breadcrumb — stringify a couple of signums for operator UX */
    const char *tag = "[cos] signal received — draining, persist state, exit\n";
    (void)!write(STDERR_FILENO, tag, strlen(tag));
    g_latched_signum = (sig_atomic_t)signum;
    g_shutdown_flag  = 1;
}

int cos_sigma_signal_install(cos_sigma_shutdown_fn hook, void *ctx) {
    g_hook        = hook;
    g_hook_ctx    = ctx;
    g_shutdown_flag  = 0;
    g_latched_signum = 0;

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = sigma_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    /* If either sigaction fails, unwind the already-installed
     * handlers so we never leave the process in a half-wired
     * state. */
    if (sigaction(SIGINT,  &sa, NULL) != 0) return -1;
    if (sigaction(SIGTERM, &sa, NULL) != 0) { cos_sigma_signal_uninstall(); return -2; }
    if (sigaction(SIGHUP,  &sa, NULL) != 0) { cos_sigma_signal_uninstall(); return -3; }

    g_installed = 1;
    return 0;
}

void cos_sigma_signal_uninstall(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    (void)sigaction(SIGINT,  &sa, NULL);
    (void)sigaction(SIGTERM, &sa, NULL);
    (void)sigaction(SIGHUP,  &sa, NULL);
    g_hook      = NULL;
    g_hook_ctx  = NULL;
    g_installed = 0;
    /* Flags deliberately left untouched so
     * cos_sigma_signal_pending() still reports the last state
     * after uninstall — useful for tests inspecting state
     * post-shutdown. */
}

int cos_sigma_signal_run_shutdown_if_requested(int *out_exit_code) {
    if (!g_shutdown_flag) return 0;
    int rv = 0;
    if (g_hook) {
        rv = g_hook((int)g_latched_signum, g_hook_ctx);
    }
    g_shutdown_flag = 0;     /* consumed */
    if (out_exit_code) *out_exit_code = rv;
    return 1;
}

int cos_sigma_signal_pending (void) { return g_shutdown_flag ? 1 : 0; }
int cos_sigma_signal_latched (void) { return (int)g_latched_signum; }

void cos_sigma_signal_raise_for_test(int signum) {
    /* Bypass the real signal delivery — exercises the flag path
     * without mutating kernel-level signal disposition. */
    g_latched_signum = (sig_atomic_t)signum;
    g_shutdown_flag  = 1;
}

/* ------------------------------------------------------------------ *
 *  Self-test — hook fires exactly once, flag is cleared, counter
 *  reflects the signum, uninstall does not crash.
 * ------------------------------------------------------------------ */

struct selftest_ctx {
    int call_count;
    int last_signum;
};

static int selftest_hook(int signum, void *ctx) {
    struct selftest_ctx *c = (struct selftest_ctx*)ctx;
    c->call_count++;
    c->last_signum = signum;
    return 42;      /* distinctive exit code */
}

int cos_sigma_signal_self_test(void) {
    struct selftest_ctx ctx = { 0, 0 };
    if (cos_sigma_signal_install(selftest_hook, &ctx) != 0) return -1;
    if (cos_sigma_signal_pending()) return -2;  /* fresh state must be clean */

    /* First simulated signal: hook fires, exit-code forwarded. */
    cos_sigma_signal_raise_for_test(SIGINT);
    if (!cos_sigma_signal_pending()) return -3;
    int ec = 0;
    if (cos_sigma_signal_run_shutdown_if_requested(&ec) != 1) return -4;
    if (ec != 42)               return -5;
    if (ctx.call_count != 1)    return -6;
    if (ctx.last_signum != SIGINT) return -7;

    /* After the hook runs the pending-flag must be cleared. */
    if (cos_sigma_signal_pending()) return -8;
    if (cos_sigma_signal_run_shutdown_if_requested(&ec) != 0) return -9;

    /* Second signal: flag latches a fresh signum. */
    cos_sigma_signal_raise_for_test(SIGTERM);
    if (cos_sigma_signal_latched() != SIGTERM) return -10;
    cos_sigma_signal_run_shutdown_if_requested(&ec);
    if (ctx.call_count != 2) return -11;
    if (ctx.last_signum != SIGTERM) return -12;

    /* Uninstall must not crash; subsequent raise still latches. */
    cos_sigma_signal_uninstall();
    cos_sigma_signal_raise_for_test(SIGHUP);
    /* After uninstall the hook pointer is NULL, so
     * run_shutdown_if_requested returns 1 but does not invoke
     * selftest_hook again. */
    cos_sigma_signal_run_shutdown_if_requested(&ec);
    if (ctx.call_count != 2) return -13;  /* unchanged */

    return 0;
}
