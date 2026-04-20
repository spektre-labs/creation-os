/*
 * σ-Signal — graceful shutdown + signal wiring for long-lived
 * σ-pipeline processes (`cos-mesh-node`, `cos-network serve`,
 * REPL / chat loops).
 *
 * A long-running agent must (a) notice when the OS hands it a
 * termination signal, (b) finish whatever write is in flight, and
 * (c) persist the five stateful facets (meta, engrams, live, cost,
 * τ) to the σ-Persist SQLite file before exiting — otherwise the
 * in-memory state is lost to SIGINT / SIGTERM / SIGHUP.
 *
 * Design:
 *   1. A single volatile flag `cos_sigma_shutdown_requested` is
 *      raised by the installed signal handler and consumed by the
 *      caller's event loop on its own schedule.
 *   2. The handler itself is strictly async-signal-safe: it does
 *      nothing beyond write(2) to stderr for a 1-liner breadcrumb
 *      and set the flag.  No printf, no malloc, no SQLite.
 *   3. A shutdown hook registered by the caller is invoked from
 *      the main thread once the flag is observed.  That is where
 *      σ-Persist save / cost ledger flush / "State saved." print
 *      happens — all in the main thread, safely.
 *   4. Signals handled by default: SIGINT, SIGTERM, SIGHUP.
 *      SIGQUIT is left to libc so operators retain core-dump
 *      access on legitimate abort.
 *
 * Contracts:
 *   - `install` is idempotent; calling it twice overwrites the
 *     previous hook / context.
 *   - `run_shutdown_if_requested` returns 1 when the hook fires,
 *     0 otherwise — callers use this as their event-loop sentinel.
 *   - `raise_for_test` is a deterministic escape hatch for smoke
 *     tests that do not want to actually send signals.
 *
 * Thread-safety: the flag is `sig_atomic_t volatile`; the hook
 * runs on the main thread only.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_SIGNAL_H
#define COS_SIGMA_SIGNAL_H

#include <signal.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Shutdown hook: `ctx` is the pointer the caller handed to
 * cos_sigma_signal_install.  Return value is the exit code the
 * caller should use; non-zero is surfaced by
 * cos_sigma_signal_run_shutdown_if_requested. */
typedef int (*cos_sigma_shutdown_fn)(int signum, void *ctx);

/* Install the default SIGINT / SIGTERM / SIGHUP handlers and
 * register `hook` to be invoked once the flag is observed.
 * Returns 0 on success, <0 on sigaction failure. */
int  cos_sigma_signal_install(cos_sigma_shutdown_fn hook, void *ctx);

/* Remove the installed hook and restore SIG_DFL for the three
 * signals.  Idempotent.  Intended for embeddings / unit tests. */
void cos_sigma_signal_uninstall(void);

/* Non-blocking check: if a signal has latched, fires the hook and
 * returns 1 (and sets *out_exit_code to the hook's return value).
 * Returns 0 if no signal has been seen yet. */
int  cos_sigma_signal_run_shutdown_if_requested(int *out_exit_code);

/* Returns 1 iff a shutdown has been requested but not yet run. */
int  cos_sigma_signal_pending(void);

/* Returns the signum that latched the shutdown, 0 if none. */
int  cos_sigma_signal_latched(void);

/* Deterministic self-test entry: pretends signum fired the
 * handler.  Safe to call from tests that do not want to actually
 * send kill signals. */
void cos_sigma_signal_raise_for_test(int signum);

int  cos_sigma_signal_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_SIGNAL_H */
