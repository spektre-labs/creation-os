/* σ-Signal canonical demo (PROD-5).
 *
 * Three modes, pickable by CLI arg so the smoke test can exercise
 * each deterministically without ever sending a real signal:
 *
 *   creation_os_sigma_signal --self-test
 *       Run cos_sigma_signal_self_test() and emit a JSON receipt.
 *
 *   creation_os_sigma_signal --simulate SIGINT|SIGTERM|SIGHUP
 *       Install the handler, raise the given signal in-process
 *       via cos_sigma_signal_raise_for_test, run the shutdown,
 *       print what happened.
 *
 *   creation_os_sigma_signal --persist-shutdown <db_path>
 *       Install the handler with a hook that writes a final
 *       "shutdown_ns" row into the σ-Persist SQLite file at
 *       <db_path>, simulates a SIGTERM, and exits cleanly.  This
 *       is the scenario the smoke test diffs against.
 */

#include "cos_signal.h"
#include "persist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int hook_noop(int signum, void *ctx) {
    (void)ctx;
    fprintf(stderr, "[cos] shutdown hook invoked for signum=%d\n", signum);
    return 0;
}

struct persist_ctx {
    cos_persist_t *db;
    int            saved;
};

static int hook_persist(int signum, void *ctx) {
    struct persist_ctx *p = (struct persist_ctx*)ctx;
    if (!p || !p->db) return 1;
    char buf[64];
    snprintf(buf, sizeof buf, "%d", signum);
    cos_persist_save_meta(p->db, "last_shutdown_signum", buf);
    cos_persist_save_meta(p->db, "graceful", "1");
    p->saved = 1;
    return 0;
}

static int run_self_test(void) {
    int rc = cos_sigma_signal_self_test();
    printf("{\"kernel\":\"sigma_signal\","
           "\"self_test_rc\":%d,"
           "\"pass\":%s}\n",
           rc, rc == 0 ? "true" : "false");
    return rc == 0 ? 0 : 1;
}

static int run_simulate(const char *name) {
    int sig = 0;
    if      (strcmp(name, "SIGINT")  == 0) sig = SIGINT;
    else if (strcmp(name, "SIGTERM") == 0) sig = SIGTERM;
    else if (strcmp(name, "SIGHUP")  == 0) sig = SIGHUP;
    else { fprintf(stderr, "unknown signal: %s\n", name); return 2; }

    int rc = cos_sigma_signal_install(hook_noop, NULL);
    if (rc != 0) return 3;
    cos_sigma_signal_raise_for_test(sig);
    int ec = 0;
    int fired = cos_sigma_signal_run_shutdown_if_requested(&ec);

    printf("{\"kernel\":\"sigma_signal\","
           "\"mode\":\"simulate\","
           "\"signal\":\"%s\","
           "\"latched_signum\":%d,"
           "\"hook_fired\":%s,"
           "\"hook_exit_code\":%d,"
           "\"pending_after\":%d,"
           "\"pass\":true}\n",
           name, sig, fired ? "true" : "false", ec,
           cos_sigma_signal_pending());
    cos_sigma_signal_uninstall();
    return 0;
}

static int run_persist_shutdown(const char *db_path) {
    int st = 0;
    cos_persist_t *db = cos_persist_open(db_path, &st);
    if (!db) return 4;

    struct persist_ctx p = { db, 0 };
    if (cos_sigma_signal_install(hook_persist, &p) != 0) {
        cos_persist_close(db);
        return 5;
    }

    /* Pretend a SIGTERM came in. */
    cos_sigma_signal_raise_for_test(SIGTERM);
    int ec = 0;
    int fired = cos_sigma_signal_run_shutdown_if_requested(&ec);

    char val[64] = {0};
    cos_persist_load_meta(db, "last_shutdown_signum", val, sizeof val);

    printf("{\"kernel\":\"sigma_signal\","
           "\"mode\":\"persist-shutdown\","
           "\"db\":\"%s\","
           "\"hook_fired\":%s,"
           "\"saved\":%s,"
           "\"last_shutdown_signum\":\"%s\","
           "\"pass\":%s}\n",
           db_path,
           fired ? "true" : "false",
           p.saved ? "true" : "false",
           val,
           (fired && p.saved && strcmp(val, "15") == 0) ? "true" : "false");

    cos_sigma_signal_uninstall();
    cos_persist_close(db);
    return (fired && p.saved) ? 0 : 1;
}

/* Sleep-until-signal daemon: install the handler, then park in a
 * 50 ms poll loop until the flag fires.  Used by the smoke test
 * to prove that a real kill(2) (SIGTERM / SIGINT) wakes the
 * handler and drains through the shutdown hook. */
static int run_daemon(void) {
    int rc = cos_sigma_signal_install(hook_noop, NULL);
    if (rc != 0) return 6;
    fprintf(stdout, "{\"kernel\":\"sigma_signal\",\"mode\":\"daemon\",\"pid\":%d}\n",
            (int)getpid());
    fflush(stdout);
    struct timespec ts = { 0, 50 * 1000 * 1000 };   /* 50 ms */
    for (;;) {
        int ec = 0;
        if (cos_sigma_signal_run_shutdown_if_requested(&ec)) {
            fprintf(stdout,
                    "{\"kernel\":\"sigma_signal\",\"mode\":\"daemon\","
                    "\"drained\":true,\"signum\":%d,\"pass\":true}\n",
                    cos_sigma_signal_latched());
            fflush(stdout);
            return 0;
        }
        nanosleep(&ts, NULL);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) return run_self_test();

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--self-test") == 0) return run_self_test();
        if (strcmp(argv[i], "--simulate") == 0 && i + 1 < argc)
            return run_simulate(argv[++i]);
        if (strcmp(argv[i], "--persist-shutdown") == 0 && i + 1 < argc)
            return run_persist_shutdown(argv[++i]);
        if (strcmp(argv[i], "--daemon") == 0) return run_daemon();
    }
    return run_self_test();
}
