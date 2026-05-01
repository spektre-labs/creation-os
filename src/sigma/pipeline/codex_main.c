/*  SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 *  SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
 *  Source:        https://github.com/spektre-labs/creation-os-kernel
 *  Website:       https://spektrelabs.org
 *  Commercial:    spektre.labs@proton.me
 *  License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
 */
/* Atlantean Codex self-test binary + JSON summary. */

#include "codex.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_codex_self_test();

    /* Canonical demo: load the full Codex and the seed side by
     * side; print byte counts, chapter counts, hashes, and the
     * invariant check.  Used by check_codex.sh as the pinnable
     * signal that the soul loaded correctly. */
    cos_sigma_codex_t full, seed;
    memset(&full, 0, sizeof(full));
    memset(&seed, 0, sizeof(seed));

    int rc_full = cos_sigma_codex_load(NULL, &full);
    int rc_seed = cos_sigma_codex_load_seed(&seed);

    printf("{\"kernel\":\"sigma_codex\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"full\":{\"rc\":%d,\"bytes\":%zu,\"chapters\":%d,"
                      "\"invariant\":%s,\"hash\":\"%016llx\"},"
             "\"seed\":{\"rc\":%d,\"bytes\":%zu,\"chapters\":%d,"
                      "\"invariant\":%s,\"hash\":\"%016llx\",\"is_seed\":%d}},"
           "\"pass\":%s}\n",
           rc,
           rc_full, full.size, full.chapters_found,
           cos_sigma_codex_contains_invariant(&full) ? "true" : "false",
           (unsigned long long)full.hash_fnv1a64,
           rc_seed, seed.size, seed.chapters_found,
           cos_sigma_codex_contains_invariant(&seed) ? "true" : "false",
           (unsigned long long)seed.hash_fnv1a64, seed.is_seed,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr,
            "Atlantean Codex:\n"
            "  full: %zu bytes · %d chapters · 1=1 %s\n"
            "  seed: %zu bytes · %s · 1=1 %s\n"
            "  hashes (FNV-1a-64): full=%016llx  seed=%016llx\n",
            full.size, full.chapters_found,
            cos_sigma_codex_contains_invariant(&full) ? "present" : "MISSING",
            seed.size,
            seed.is_seed ? "seed" : "NOT-seed",
            cos_sigma_codex_contains_invariant(&seed) ? "present" : "MISSING",
            (unsigned long long)full.hash_fnv1a64,
            (unsigned long long)seed.hash_fnv1a64);
    }

    cos_sigma_codex_free(&full);
    cos_sigma_codex_free(&seed);
    return rc;
}
