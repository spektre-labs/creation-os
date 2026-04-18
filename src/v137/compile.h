/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Copyright (c) 2026 Spektre Labs / Lauri Rainio
 *
 * v137 σ-Compile — AOT compilation of the σ-decision tree.
 *
 * v60..v100's forty-layer σ-gate is currently evaluated as
 * interpreted C at runtime: a function-pointer table plus an
 * argv-of-thresholds loaded from `config.toml`.  v137 makes that
 * branchless and AOT:
 *
 *   1.  `cos_v137_emit_source` generates a self-contained C file
 *       whose single exported function
 *
 *           int cos_v137_compiled_gate(const float ch[8]);
 *
 *       evaluates the entire decision tree with no loads from the
 *       σ-profile table (all thresholds are C literals), no calls,
 *       and only bit-OR'd compare instructions — the "branchless
 *       `deny`".  Return 1 ⇒ ABSTAIN, 0 ⇒ EMIT.
 *
 *   2.  The Makefile target `compile-sigma` pipes it through
 *       `$(CC) -O3` (whose own lowering to LLVM IR or its GCC
 *       equivalent plus vectorisation is where the heavy lifting
 *       happens).  v137.1 emits LLVM IR directly via `clang -S
 *       -emit-llvm`; v137.0 trusts the toolchain.
 *
 *   3.  `cos_v137_interpreted_gate` is the runtime reference:
 *       same predicate, but the threshold vector is read at each
 *       call.  The merge-gate benchmarks the two back-to-back on
 *       the same input stream and asserts the compiled path is
 *       strictly faster.
 *
 *   4.  An *embedded* compiled gate, `cos_v137_example_compiled_gate`,
 *       is hand-written against the default profile (τ = 0.50 on
 *       every channel) so v137 can be exercised end-to-end from a
 *       single, already-compiled binary; emitting a fresh
 *       per-profile .c + running `$(CC) -O3` on it is the shape of
 *       the `compile-sigma` target but is a v137.1 follow-up for
 *       the merge-gate.
 *
 * σ-innovation: the σ layer is hot-path — it touches every token.
 * Trading generality for a closed-form, profile-specialised
 * branchless predicate removes the last interpretive cost in the
 * gate.
 */
#ifndef COS_V137_COMPILE_H
#define COS_V137_COMPILE_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V137_CHANNELS 8

/* Interpreted reference gate.  Any channel above its threshold
 * → ABSTAIN (return 1); otherwise EMIT (return 0).  Branchless
 * in principle but requires a table load per channel. */
int cos_v137_interpreted_gate(const float tau[COS_V137_CHANNELS],
                              const float ch [COS_V137_CHANNELS]);

/* Embedded AOT-compiled gate, specialised for τ = 0.50 across all
 * channels.  This is the exact shape the generator emits for the
 * default profile — attributes and all — so we can benchmark the
 * "compiled" path without invoking $(CC) at run time. */
int cos_v137_example_compiled_gate(const float ch[COS_V137_CHANNELS]);

/* Write a specialised decision tree as C source.  `path` may be
 * NULL to emit to stdout. */
int cos_v137_emit_source    (const float tau[COS_V137_CHANNELS],
                             const char *path);

/* Latency micro-benchmark.  Runs `iters` calls against `vectors`
 * (length COS_V137_CHANNELS * n_vectors).  Returns nanoseconds
 * per call and XORs an accumulator into *out_checksum to defeat
 * DCE.  Uses clock_gettime(CLOCK_MONOTONIC). */
double cos_v137_bench_interpreted(const float tau[COS_V137_CHANNELS],
                                  const float *vectors, int n_vectors,
                                  int iters, uint64_t *out_checksum);
double cos_v137_bench_compiled   (const float *vectors, int n_vectors,
                                  int iters, uint64_t *out_checksum);

int cos_v137_self_test      (void);

#ifdef __cplusplus
}
#endif
#endif
