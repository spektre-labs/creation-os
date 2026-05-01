/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-Corpus — ingest Creation OS's own papers, specs, and Codex
 * into a σ-RAG index so the system can answer meta-questions
 * about itself from local prose.
 *
 * A corpus is a manifest: a newline-delimited list of file paths
 * (relative to the manifest's directory, or absolute).  Each path
 * must resolve to a UTF-8 text file.  The ingester reads the file
 * in full, tags chunks with the basename as their source, and
 * reports per-file success / failure.
 *
 * Files that fail to open are recorded but do not abort the batch:
 * the stated invariant is "whatever is on disk, get it into the
 * index; tell the caller what slipped".
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_CORPUS_H
#define COS_SIGMA_CORPUS_H

#include <stddef.h>
#include "rag.h"

#ifdef __cplusplus
extern "C" {
#endif

#define COS_CORPUS_MAX_FILES   512
#define COS_CORPUS_PATH_MAX    512
#define COS_CORPUS_READ_CAP   (1u << 20)   /* 1 MiB per doc hard cap */

enum cos_corpus_status {
    COS_CORPUS_OK        =  0,
    COS_CORPUS_ERR_ARG   = -1,
    COS_CORPUS_ERR_IO    = -2,
    COS_CORPUS_ERR_OOM   = -3,
};

typedef struct {
    char   path[COS_CORPUS_PATH_MAX];
    int    ok;               /* 1 = read + indexed                  */
    size_t bytes_read;
    int    chunks_added;
    int    status;           /* 0 OK or negative error              */
} cos_corpus_file_report_t;

typedef struct {
    cos_corpus_file_report_t files[COS_CORPUS_MAX_FILES];
    int    n;
    int    n_ok;
    int    n_skipped;
    size_t total_bytes;
    int    total_chunks;
} cos_corpus_report_t;

/* Ingest a newline-delimited manifest file.  Paths are resolved
 * relative to the manifest's directory (or used as-is if absolute).
 * Blank lines and lines starting with '#' are skipped. */
int cos_corpus_ingest_manifest(cos_rag_index_t *idx,
                               const char *manifest_path,
                               cos_corpus_report_t *report);

/* Ingest a single file directly. */
int cos_corpus_ingest_file(cos_rag_index_t *idx,
                           const char *path,
                           cos_corpus_file_report_t *out);

/* JSON render of a report (sorted by path as supplied). */
int cos_corpus_report_json(const cos_corpus_report_t *rep,
                           char *dst, size_t cap);

int cos_corpus_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_CORPUS_H */
