/*
 * v169 σ-Ontology — CLI entry point.
 *
 *   creation_os_v169_ontology --self-test
 *   creation_os_v169_ontology                  # full JSON
 *   creation_os_v169_ontology --owl            # OWL-lite-JSON
 *   creation_os_v169_ontology --query PRED [OBJ]
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#include "ontology.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--self-test") == 0) {
        int rc = cos_v169_self_test();
        if (rc == 0) puts("v169 self-test: ok");
        else         fprintf(stderr, "v169 self-test failed at stage %d\n", rc);
        return rc;
    }

    cos_v169_ontology_t o;
    cos_v169_init(&o, 0x169FACADE0169ULL);
    cos_v169_extract_all(&o);
    cos_v169_type_entities(&o);

    if (argc >= 2 && strcmp(argv[1], "--owl") == 0) {
        char buf[16384];
        size_t n = cos_v169_owl_lite_to_json(&o, buf, sizeof(buf));
        if (n == 0) { fprintf(stderr, "v169: owl overflow\n"); return 2; }
        fwrite(buf, 1, n, stdout); putchar('\n');
        return 0;
    }

    if (argc >= 3 && strcmp(argv[1], "--query") == 0) {
        const char *pred = argv[2];
        const char *obj  = argc >= 4 ? argv[3] : NULL;
        int ids[COS_V169_N_ENTRIES];
        int hits = cos_v169_query(&o, pred, obj, ids, COS_V169_N_ENTRIES);
        printf("{\"kernel\":\"v169\",\"event\":\"query\","
               "\"predicate\":\"%s\",\"object\":%s%s%s,"
               "\"hits\":%d,\"entry_ids\":[",
               pred,
               obj ? "\"" : "", obj ? obj : "null", obj ? "\"" : "",
               hits);
        for (int i = 0; i < hits && i < COS_V169_N_ENTRIES; ++i)
            printf("%s%d", i == 0 ? "" : ",", ids[i]);
        puts("]}");
        return 0;
    }

    char buf[32768];
    size_t n = cos_v169_to_json(&o, buf, sizeof(buf));
    if (n == 0) { fprintf(stderr, "v169: json overflow\n"); return 2; }
    fwrite(buf, 1, n, stdout); putchar('\n');
    return 0;
}
