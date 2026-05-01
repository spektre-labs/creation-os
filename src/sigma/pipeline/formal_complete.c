/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/*
 * σ-formal-complete — implementation.
 *
 * Lean parser is deliberately line-oriented and conservative:
 *   - A "theorem" entry begins with a line matching ^theorem <name>.
 *   - The entry ends at the next line that starts with
 *     "theorem "/"def "/"namespace "/"end " (column 0).
 *   - `has_sorry` is set if the body contains the whole-word token
 *     `sorry` (matched with lexical boundaries, not inside a string).
 *   - `abstract_variant` is set when the name contains UTF-8 α
 *     (0xCE 0xB1) — the `gateα / gateβ` naming convention we use
 *     for LinearOrder-lifted Float counterparts.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#define _GNU_SOURCE
#include "formal_complete.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *slurp_file(const char *path, size_t *n_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (n < 0) { fclose(fp); return NULL; }
    char *buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(fp); return NULL; }
    size_t r = fread(buf, 1, (size_t)n, fp);
    fclose(fp);
    buf[r] = '\0';
    if (n_out) *n_out = r;
    return buf;
}

static int line_starts_with(const char *line, const char *prefix) {
    size_t n = strlen(prefix);
    return strncmp(line, prefix, n) == 0;
}

/* Strip Lean comments from `src` into `dst`.  We recognise
 *   --  …\n            line comment
 *   /- … -/             nestable block comment (incl. /-- -/)
 * Strings ("…") are preserved as-is.  Nesting depth is tracked so
 * `/- a /- b -/ c -/` collapses correctly. */
static void strip_lean_comments(const char *src, char *dst, size_t cap) {
    size_t o = 0;
    int in_block = 0;
    int in_str   = 0;
    while (*src && o + 1 < cap) {
        if (in_block > 0) {
            if (src[0] == '-' && src[1] == '/') { in_block--; src += 2; }
            else if (src[0] == '/' && src[1] == '-') { in_block++; src += 2; }
            else src++;
            continue;
        }
        if (in_str) {
            dst[o++] = *src;
            if (*src == '\\' && src[1]) { if (o + 1 < cap) dst[o++] = src[1]; src += 2; continue; }
            if (*src == '"') in_str = 0;
            src++;
            continue;
        }
        if (src[0] == '-' && src[1] == '-') {
            while (*src && *src != '\n') src++;
            continue;
        }
        if (src[0] == '/' && src[1] == '-') { in_block = 1; src += 2; continue; }
        if (*src == '"') in_str = 1;
        dst[o++] = *src++;
    }
    dst[o] = '\0';
}

/* Whole-word "sorry" check on a comment-stripped body. */
static int body_has_sorry(const char *body) {
    char stripped[8192];
    strip_lean_comments(body, stripped, sizeof stripped);
    const char *p = stripped;
    while ((p = strstr(p, "sorry")) != NULL) {
        int left_ok  = (p == stripped) ||
                       !(isalnum((unsigned char)p[-1]) || p[-1] == '_');
        const char *end = p + 5;
        int right_ok = !(isalnum((unsigned char)*end) || *end == '_');
        if (left_ok && right_ok) return 1;
        p++;
    }
    return 0;
}

/* Greek small letter alpha in UTF-8 is 0xCE 0xB1. */
static int name_has_alpha(const char *name) {
    for (const char *p = name; *p; p++) {
        if ((unsigned char)p[0] == 0xCE && (unsigned char)p[1] == 0xB1)
            return 1;
    }
    return 0;
}

/* Extract the identifier that follows the word "theorem ".  Stops
 * at whitespace, '(', ':', or '{'.  Caller supplies a buffer. */
static int extract_theorem_name(const char *line, char *name, size_t cap) {
    const char *p = strstr(line, "theorem ");
    if (!p) return -1;
    p += 8;
    while (*p && isspace((unsigned char)*p)) p++;
    size_t n = 0;
    while (*p && !isspace((unsigned char)*p) && *p != '(' && *p != ':' &&
           *p != '{' && n + 1 < cap) {
        name[n++] = *p++;
    }
    name[n] = '\0';
    return n > 0 ? 0 : -1;
}

/* Walk buffer line by line.  Whenever we see a theorem header, read
 * the body until the next line starts with one of the Lean block
 * starters. */
static int scan_lean(const char *buf, cos_formal_report_t *r) {
    const char *p = buf;
    int lineno = 0;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        lineno++;

        if (line_starts_with(p, "theorem ")) {
            if (r->n_theorems >= COS_FORMAL_MAX_THEOREMS) return -2;
            cos_formal_theorem_t *t = &r->theorems[r->n_theorems++];
            memset(t, 0, sizeof *t);
            t->line = lineno;

            char line_tmp[256];
            size_t cp = len < sizeof line_tmp - 1 ? len : sizeof line_tmp - 1;
            memcpy(line_tmp, p, cp);
            line_tmp[cp] = '\0';
            extract_theorem_name(line_tmp, t->name, sizeof t->name);
            t->abstract_variant = name_has_alpha(t->name);

            /* Accumulate body from this line inclusive up to the
             * next block-starter.  Cap body at 8 KiB. */
            char body[8192];
            size_t bo = 0;
            size_t room = sizeof body - 1;
            if (cp <= room) { memcpy(body, p, cp); bo = cp; } else { bo = 0; }
            body[bo] = '\0';

            const char *q = nl ? nl + 1 : p + len;
            while (*q) {
                if (line_starts_with(q, "theorem ")  ||
                    line_starts_with(q, "def ")      ||
                    line_starts_with(q, "namespace ") ||
                    line_starts_with(q, "end ")      ||
                    line_starts_with(q, "structure ") ||
                    line_starts_with(q, "inductive ")) {
                    break;
                }
                const char *q2 = strchr(q, '\n');
                size_t l2 = q2 ? (size_t)(q2 - q) : strlen(q);
                if (bo + l2 + 1 < sizeof body) {
                    memcpy(body + bo, q, l2);
                    bo += l2;
                    body[bo++] = '\n';
                    body[bo] = '\0';
                }
                if (!q2) break;
                q = q2 + 1;
            }
            t->has_sorry = body_has_sorry(body);
            if (t->has_sorry)       t->kind = COS_FORMAL_PENDING;
            else if (t->abstract_variant)
                                     t->kind = COS_FORMAL_DISCHARGED_ABSTRACT;
            else                     t->kind = COS_FORMAL_DISCHARGED_CONCRETE;
        }
        if (!nl) break;
        p = nl + 1;
    }
    return 0;
}

static void count_categories(cos_formal_report_t *r) {
    r->discharged_concrete = 0;
    r->discharged_abstract = 0;
    r->pending             = 0;
    for (int i = 0; i < r->n_theorems; i++) {
        switch (r->theorems[i].kind) {
            case COS_FORMAL_DISCHARGED_CONCRETE:
                r->discharged_concrete++; break;
            case COS_FORMAL_DISCHARGED_ABSTRACT:
                r->discharged_abstract++; break;
            case COS_FORMAL_PENDING:
                r->pending++; break;
        }
    }
}

/* ACSL: count lines that contain the tokens we care about. */
static void scan_acsl(const char *buf, cos_formal_report_t *r) {
    const char *p = buf;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[512];
        size_t cp = len < sizeof line - 1 ? len : sizeof line - 1;
        memcpy(line, p, cp);
        line[cp] = '\0';
        if (strstr(line, "requires ")) r->acsl_requires++;
        if (strstr(line, "ensures  ") || strstr(line, "ensures ")) {
            if (strstr(line, "ensures")) r->acsl_ensures++;
        }
        if (!nl) break;
        p = nl + 1;
    }
}

/* Minimal parser for COS_FORMAL_PROOFS / COS_FORMAL_PROOFS_TOTAL. */
static void scan_version_header(const char *buf, cos_formal_report_t *r) {
    const char *p;
    if ((p = strstr(buf, "COS_FORMAL_PROOFS ")) != NULL) {
        p += strlen("COS_FORMAL_PROOFS ");
        while (*p && isspace((unsigned char)*p)) p++;
        r->header_proofs = (int)strtol(p, NULL, 10);
    }
    if ((p = strstr(buf, "COS_FORMAL_PROOFS_TOTAL ")) != NULL) {
        p += strlen("COS_FORMAL_PROOFS_TOTAL ");
        while (*p && isspace((unsigned char)*p)) p++;
        r->header_proofs_total = (int)strtol(p, NULL, 10);
    }
}

/* Effective discharge count:
 *   - Lower bound: discharged_concrete (sorry-free Float-level)
 *   - Upper bound: min(total, discharged_concrete + discharged_abstract)
 *
 * We expose the upper bound so the banner can't exceed it.  The
 * check script asserts the header value falls within the honest
 * window. */
static void compute_effective(cos_formal_report_t *r) {
    int hi = r->discharged_concrete + r->discharged_abstract;
    if (r->header_proofs_total > 0 && hi > r->header_proofs_total)
        hi = r->header_proofs_total;
    r->effective_discharged = hi;
    /* Honest ledger means:
     *   - header_proofs is non-negative and not greater than the total,
     *   - header_proofs does NOT overclaim (≤ effective_discharged).
     * Underclaiming is always safe; the banner can catch up at any
     * release without breaking the invariant. */
    r->ledger_matches_truth =
        (r->header_proofs >= 0) &&
        (r->header_proofs <= r->header_proofs_total) &&
        (r->header_proofs <= hi);
}

int cos_formal_complete_scan(const char *lean_path,
                             const char *acsl_path,
                             const char *version_header_path,
                             cos_formal_report_t *r) {
    if (!r) return -1;
    memset(r, 0, sizeof *r);

    size_t nlean = 0, nacsl = 0, nhdr = 0;
    char *lean = slurp_file(lean_path, &nlean);
    char *acsl = slurp_file(acsl_path, &nacsl);
    char *hdr  = slurp_file(version_header_path, &nhdr);
    if (!lean || !acsl || !hdr) {
        free(lean); free(acsl); free(hdr);
        return -1;
    }

    int rc = scan_lean(lean, r);
    if (rc == 0) count_categories(r);
    scan_acsl(acsl, r);
    r->acsl_file_bytes = (int)nacsl;
    scan_version_header(hdr, r);
    compute_effective(r);

    free(lean); free(acsl); free(hdr);
    return rc;
}

int cos_formal_complete_self_test(void) {
    cos_formal_report_t r;
    int rc = cos_formal_complete_scan(
        "hw/formal/v259/Measurement.lean",
        "hw/formal/v259/sigma_measurement.h.acsl",
        "include/cos_version.h",
        &r);
    if (rc != 0)                          return -1;

    /* v2.0 Omega CLOSE-1 state:
     *   All T1..T6 theorems are discharged over core Lean 4 (Nat);
     *   the Float fragment is delegated to sigma_measurement.h.acsl
     *   (Frama-C Wp).  Post-CLOSE invariants:
     *     * at least 6 theorems in the Lean file,
     *     * at least 6 concrete discharges (T1..T6),
     *     * zero `sorry` pending — we have stopped underclaiming,
     *     * the α-family is optional (may be 0 now that we dropped
     *       the Mathlib-dependent LinearOrder lifts).
     */
    if (r.n_theorems < 6)                 return -2;
    if (r.discharged_concrete < 6)        return -3;
    if (r.pending != 0)                   return -4;
    if (r.header_proofs_total != 6)       return -5;
    if (!r.ledger_matches_truth)          return -6;
    if (r.acsl_requires < 3)              return -7;
    if (r.acsl_ensures < 5)               return -8;
    return 0;
}
