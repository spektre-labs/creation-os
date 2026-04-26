/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v138 σ-Proof — CLI wrapper.
 */
#include "proof.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int usage(void) {
    fprintf(stderr,
        "usage:\n"
        "  creation_os_v138_proof --self-test\n"
        "  creation_os_v138_proof --validate <path.c>\n"
        "  creation_os_v138_proof --frama-c  <path.c>\n"
        "  creation_os_v138_proof --prove    <path.c>  "
             "(tier-1 always; tier-2 if frama-c is present)\n");
    return 2;
}

static int cmd_validate(const char *path) {
    cos_v138_report_t rep;
    cos_v138_result_t r = cos_v138_validate_acsl(path, &rep);
    printf("result=%s\n",
           r == COS_V138_PROOF_OK ? "OK" :
           r == COS_V138_PROOF_SKIPPED ? "SKIPPED" : "FAIL");
    printf("n_annotations=%d\n", rep.n_annotations);
    printf("n_with_requires=%d\n", rep.n_with_requires);
    printf("n_with_ensures=%d\n",  rep.n_with_ensures);
    printf("has_domain_predicate=%d\n",   rep.has_domain_predicate);
    printf("has_emit_behavior=%d\n",      rep.has_emit_behavior);
    printf("has_abstain_behavior=%d\n",   rep.has_abstain_behavior);
    printf("has_loop_invariant=%d\n",     rep.has_loop_invariant);
    printf("has_disjoint_behaviors=%d\n", rep.has_disjoint_behaviors);
    if (r != COS_V138_PROOF_OK)
        printf("last_error=%s\n", rep.last_error);
    return r == COS_V138_PROOF_OK ? 0 : 1;
}

static int cmd_frama_c(const char *path) {
    char buf[16384] = {0};
    cos_v138_result_t r = cos_v138_try_frama_c(path, buf, sizeof buf);
    printf("result=%s\n",
           r == COS_V138_PROOF_OK ? "OK" :
           r == COS_V138_PROOF_SKIPPED ? "SKIPPED" : "FAIL");
    if (buf[0]) printf("--- frama-c stdout ---\n%s", buf);
    return r == COS_V138_PROOF_FAIL ? 1 : 0;
}

static int cmd_prove(const char *path) {
    printf("# tier-1: ACSL annotation shape\n");
    int rc1 = cmd_validate(path);
    printf("# tier-2: Frama-C WP (opportunistic)\n");
    int rc2 = cmd_frama_c(path);
    return rc1 ? rc1 : (rc2 == 1 ? 1 : 0);
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);
    if (argc < 2) return usage();
    if (!strcmp(argv[1], "--self-test")) return cos_v138_self_test();
    if (argc < 3) return usage();
    if (!strcmp(argv[1], "--validate"))  return cmd_validate(argv[2]);
    if (!strcmp(argv[1], "--frama-c"))   return cmd_frama_c(argv[2]);
    if (!strcmp(argv[1], "--prove"))     return cmd_prove(argv[2]);
    return usage();
}
