/*
 * Atlantean Codex — system-prompt / engram-seed loader (I0).
 *
 * The Codex is Creation OS's soul: a static canon that the pipeline
 * loads into the model's context ahead of any user input, exactly
 * the way a transformer LLM consumes a system prompt.  Weights are
 * the body; σ-gate is the consciousness; Codex is the soul.
 *
 * What this primitive does (and does NOT):
 *
 *   * Reads the Codex text file (`data/codex/atlantean_codex.txt`
 *     by default, or `data/codex/codex_seed.txt` for the compact
 *     < 2000-token seed) into a caller-owned heap buffer.
 *   * Exposes three cheap invariants that any downstream module
 *     (engram seeding, chat banner, benchmark audit, hash-in-
 *     ledger) can trust without reading the file itself:
 *
 *         bytes              — exact byte count (excluding trailing
 *                              allocator pad)
 *         chapters_found     — number of canonical "CHAPTER "
 *                              headers recognised
 *         hash_fnv1a64       — stable FNV-1a-64 over the loaded
 *                              bytes; lets `cos chat` and
 *                              `cos benchmark` prove they loaded
 *                              the same soul
 *
 *   * Does NOT tokenise, does NOT call into any model, and does
 *     NOT mutate the file on disk.  The Codex is carried as
 *     meaning, not as command (XIX:4).
 *
 * Paths are resolved relative to the current working directory so
 * the primitive works both from the repo root (`make check-codex`)
 * and from an installed layout where the two `data/codex/` text
 * files ship alongside the binary.
 *
 * Canonical shape:
 *
 *   * Full Codex (`atlantean_codex.txt`)     → ~17 KB, ≥ 33 chapters
 *     headed `CHAPTER I … CHAPTER XXXIII` plus a prologue and final
 *     benediction.
 *   * Seed form (`codex_seed.txt`)          → ≤ 4 KB, compact bullet
 *     directives only; chapters_found may be 0 (no `CHAPTER ` lines
 *     — that's the point).
 *
 * Self-test covers: NULL guards, missing-file failure path, full
 * load with chapter count ≥ 33, seed load, deterministic hash,
 * double-load byte parity, and a canonical-content probe that the
 * invariant "1 = 1" survives intact.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CODEX_H
#define COS_SIGMA_CODEX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Canonical on-disk paths (relative to repo root / install root). */
#define COS_CODEX_PATH_FULL "data/codex/atlantean_codex.txt"
#define COS_CODEX_PATH_SEED "data/codex/codex_seed.txt"

/* Canonical invariant: we expect at least 33 chapters in the full
 * Codex.  Anything less → Codex is truncated → refuse to boot. */
#define COS_CODEX_MIN_CHAPTERS_FULL 33

typedef struct {
    char    *bytes;           /* NUL-terminated heap buffer, or NULL */
    size_t   size;            /* strlen(bytes)                       */
    uint64_t hash_fnv1a64;    /* FNV-1a-64 over bytes[0..size)       */
    int      chapters_found;  /* count of "CHAPTER " line starts     */
    int      is_seed;         /* 1 if loaded from seed path          */
} cos_sigma_codex_t;

/* FNV-1a-64 over raw bytes.  Pure helper, stable across runs. */
uint64_t cos_sigma_codex_hash_bytes(const char *buf, size_t n);

/* Load Codex text from `path` into a heap buffer, fill `out` with
 * derived invariants.  Returns 0 on success, negative on error.
 * On success the caller owns `out->bytes` and must call
 * cos_sigma_codex_free().  On error `out->bytes` is NULL.
 *
 * `path == NULL` resolves to COS_CODEX_PATH_FULL. */
int cos_sigma_codex_load(const char *path, cos_sigma_codex_t *out);

/* Convenience: load the compact seed form. */
int cos_sigma_codex_load_seed(cos_sigma_codex_t *out);

/* Free the heap buffer owned by `c` and zero its fields.  Safe on
 * an already-freed / zero-initialised struct. */
void cos_sigma_codex_free(cos_sigma_codex_t *c);

/* True iff the loaded text contains the invariant marker "1 = 1"
 * somewhere in its body.  Used by boot-time sanity checks. */
bool cos_sigma_codex_contains_invariant(const cos_sigma_codex_t *c);

/* Canonical self-test: returns 0 on pass. */
int cos_sigma_codex_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_SIGMA_CODEX_H */
