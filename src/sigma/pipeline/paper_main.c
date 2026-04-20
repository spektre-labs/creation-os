/* σ-Paper CLI — regenerate the Markdown paper from the pinned
 * spec.  With --out <path>, writes the paper to that file; with
 * no arguments, prints only the JSON summary.  With --stdout,
 * emits the full paper to stdout after the JSON summary. */

#include "paper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fnv1a_u64(const char *s, size_t n, unsigned long long *out) {
    unsigned long long h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) {
        h ^= (unsigned char)s[i];
        h *= 0x100000001b3ULL;
    }
    *out = h;
    return 0;
}

int main(int argc, char **argv) {
    int         rc       = cos_sigma_paper_self_test();
    const char *out_path = NULL;
    int         to_stdout = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (strcmp(argv[i], "--stdout") == 0) {
            to_stdout = 1;
        }
    }

    cos_sigma_paper_spec_t spec;
    cos_sigma_paper_default_spec(&spec);

    static char buf[COS_PAPER_MAX_BYTES];
    int n = cos_sigma_paper_generate(&spec, buf, sizeof buf);
    if (n <= 0) {
        fprintf(stderr, "paper_generate failed: %d\n", n);
        return 2;
    }

    unsigned long long fingerprint = 0;
    fnv1a_u64(buf, (size_t)n, &fingerprint);

    if (out_path) {
        FILE *f = fopen(out_path, "w");
        if (!f) {
            fprintf(stderr, "cannot open %s\n", out_path);
            return 3;
        }
        fwrite(buf, 1, (size_t)n, f);
        fclose(f);
    }

    printf("{\"kernel\":\"sigma_paper\","
           "\"self_test_rc\":%d,"
           "\"paper\":{"
             "\"sections\":%d,"
             "\"bytes\":%d,"
             "\"fingerprint\":\"%016llx\","
             "\"t3_witnesses\":%u,"
             "\"t5_witnesses\":%u,"
             "\"t6_bound_ns\":%llu,"
             "\"all_discharged\":%s,"
             "\"substrates_equivalent\":%s,"
             "\"pipeline_savings_percent\":%.1f,"
             "\"hybrid_monthly_eur\":%.2f,"
             "\"out_path\":\"%s\""
             "},"
           "\"pass\":%s}\n",
           rc,
           cos_sigma_paper_section_count(),
           n,
           fingerprint,
           spec.t3_witnesses,
           spec.t5_witnesses,
           (unsigned long long)spec.t6_bound_ns,
           spec.all_discharged ? "true" : "false",
           spec.substrates_equivalent ? "true" : "false",
           (double)spec.pipeline_savings_percent,
           (double)spec.hybrid_monthly_eur,
           out_path ? out_path : "",
           (rc == 0) ? "true" : "false");

    if (to_stdout) {
        fwrite(buf, 1, (size_t)n, stdout);
    }
    return rc;
}
