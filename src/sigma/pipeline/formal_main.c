/* σ-Formal self-test + evidence ledger emitter.
 *
 * Runs T3/T4/T5/T6 and prints a single JSON ledger summarising
 * witnesses, violations, discharged flag, and (for T6) the
 * observed median / p99 / bound.
 */

#include "formal.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    cos_formal_result_t r[4];
    int rc = cos_sigma_formal_check_all(r);

    int discharged_count = 0;
    for (int i = 0; i < 4; ++i) if (r[i].discharged) discharged_count++;

    printf("{\"kernel\":\"sigma_formal\","
           "\"self_test_rc\":%d,"
           "\"ledger\":{"
             "\"theorems\":[", rc);
    for (int i = 0; i < 4; ++i) {
        if (i > 0) printf(",");
        printf("{\"id\":\"%s\",\"desc\":\"%s\","
                "\"witnesses\":%u,\"violations\":%u,"
                "\"discharged\":%s",
               r[i].theorem_id, r[i].description,
               r[i].witnesses, r[i].violations,
               r[i].discharged ? "true" : "false");
        if (strcmp(r[i].theorem_id, "T6") == 0) {
            printf(",\"median_ns\":%llu,\"p99_ns\":%llu,\"bound_ns\":%llu",
                   (unsigned long long)r[i].t6_median_ns,
                   (unsigned long long)r[i].t6_p99_ns,
                   (unsigned long long)r[i].t6_bound_ns);
        }
        printf("}");
    }
    printf("],\"discharged\":\"%d/4\"},"
           "\"pass\":%s}\n",
           discharged_count,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Formal ledger: %d/4 theorems discharged\n",
                discharged_count);
    }
    return rc;
}
