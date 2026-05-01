/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-arxiv — arXiv submission manifest generator.
 *
 * The kernel does not perform an arXiv submission.  Per
 * docs/CLAIM_DISCIPLINE.md, the act of submitting is a human,
 * one-shot operation that leaves an external DOI trail — not
 * something a deterministic C kernel can claim to have done.
 *
 * What the kernel DOES do:
 *   1. Read the two paper sources (Markdown + LaTeX) and compute
 *      their SHA-256 so the submission is pinned to an exact
 *      byte-for-byte tree state.
 *   2. Emit the submission metadata (title, authors, ORCID,
 *      affiliation, category, abstract placeholder, DOI slot)
 *      as a canonical JSON envelope.
 *   3. Self-check: metadata and anchor-file digests are both
 *      present so the actual arxiv submit step can pick them up.
 *
 * The accompanying check script diff-locks the manifest, so any
 * post-submit change to the paper sources forces a manifest
 * refresh (preventing silent content drift between what was
 * uploaded to arXiv and what the tree says was uploaded).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_SIGMA_ARXIV_H
#define COS_SIGMA_ARXIV_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_ARXIV_HEX_LEN         65   /* 64 + NUL */
#define COS_ARXIV_MAX_ANCHORS      8
#define COS_ARXIV_NAME_MAX       160

typedef struct {
    char path[COS_ARXIV_NAME_MAX];
    char sha256_hex[COS_ARXIV_HEX_LEN];
    long bytes;
    int  exists;
} cos_arxiv_anchor_t;

typedef struct {
    const char *title;
    const char *author;
    const char *orcid;
    const char *affiliation;
    const char *category;
    const char *abstract;
    const char *license;
    const char *version_tag;
    const char *doi_reserved;

    cos_arxiv_anchor_t anchors[COS_ARXIV_MAX_ANCHORS];
    int                n_anchors;
    int                anchors_present;
} cos_arxiv_manifest_t;

int cos_arxiv_manifest_build(cos_arxiv_manifest_t *m);
int cos_arxiv_manifest_write_json(const cos_arxiv_manifest_t *m,
                                  char *buf, size_t cap);
int cos_arxiv_self_test(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_SIGMA_ARXIV_H */
