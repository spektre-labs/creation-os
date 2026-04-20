/* σ-Engram self-test binary entry point. */

#include "engram.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_engram_self_test();

    /* Demo for the smoke script to pin. */
    cos_sigma_engram_entry_t slots[8];
    cos_sigma_engram_t e;
    cos_sigma_engram_init(&e, slots, 8, /*tau_trust=*/0.30f,
                          /*long_ttl=*/100, /*short_ttl=*/10);

    uint64_t k1 = cos_sigma_engram_hash("What is 2+2?");
    uint64_t k2 = cos_sigma_engram_hash("What is the capital of France?");
    uint64_t k3 = cos_sigma_engram_hash("Prove Fermat's last theorem.");

    cos_sigma_engram_put(&e, k1, 0.05f, (void *)"4",     NULL);
    cos_sigma_engram_put(&e, k2, 0.10f, (void *)"Paris", NULL);
    cos_sigma_engram_put(&e, k3, 0.92f, (void *)"(hard)", NULL);

    cos_sigma_engram_entry_t out;
    int hit1 = cos_sigma_engram_get(&e, k1, &out);
    int hit2 = cos_sigma_engram_get(&e, k2, &out);
    int miss = cos_sigma_engram_get(&e,
            cos_sigma_engram_hash("unknown-q"), &out);

    /* Canonical hash values — so the C↔Python parity test can prove
     * FNV-1a-64 byte-identical agreement without exposing the private
     * slot array. */
    uint64_t h_empty = cos_sigma_engram_hash("");
    uint64_t h_a     = cos_sigma_engram_hash("a");
    uint64_t h_hello = cos_sigma_engram_hash("hello");
    uint64_t h_q1    = cos_sigma_engram_hash("What is 2+2?");

    printf("{\"kernel\":\"sigma_engram\","
           "\"self_test_rc\":%d,"
           "\"demo\":{\"cap\":%u,\"count\":%u,"
             "\"hit1\":%d,\"hit2\":%d,\"miss\":%d,"
             "\"n_get_hit\":%u,\"n_get_miss\":%u,"
             "\"n_put\":%u,\"n_evict\":%u,"
             "\"sigma_trust_threshold\":%.4f,"
             "\"long_ttl\":%u,\"short_ttl\":%u},"
           "\"hashes\":{"
             "\"empty\":\"%llu\","
             "\"a\":\"%llu\","
             "\"hello\":\"%llu\","
             "\"q1\":\"%llu\"},"
           "\"pass\":%s}\n",
           rc, e.cap, e.count,
           hit1, hit2, miss,
           e.n_get_hit, e.n_get_miss, e.n_put, e.n_evict,
           (double)e.tau_trust, e.long_ttl, e.short_ttl,
           (unsigned long long)h_empty,
           (unsigned long long)h_a,
           (unsigned long long)h_hello,
           (unsigned long long)h_q1,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Engram demo:\n");
        fprintf(stderr, "  tau_trust=%.2f  long_ttl=%u  short_ttl=%u\n",
                (double)e.tau_trust, e.long_ttl, e.short_ttl);
        fprintf(stderr, "  2+2         σ=0.05 → stored, long TTL\n");
        fprintf(stderr, "  Paris       σ=0.10 → stored, long TTL\n");
        fprintf(stderr, "  Fermat      σ=0.92 → stored, SHORT TTL\n");
        fprintf(stderr, "  count=%u   hits=%u   misses=%u\n",
                e.count, e.n_get_hit, e.n_get_miss);
    }
    return rc;
}
