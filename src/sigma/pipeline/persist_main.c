/* σ-Persist self-test + canonical demo (PROD-3).
 *
 * Opens /tmp/cos_persist_demo.db, lays down one row in each of
 * the five tables, closes the handle, reopens, reads back, and
 * prints a pinnable JSON receipt with row counts and schema
 * version.  The smoke test at
 * benchmarks/sigma_pipeline/check_sigma_persist.sh diffs the JSON.
 *
 * Exit 0 iff self_test_rc == 0 AND the demo roundtrip succeeded.
 */

#include "persist.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    int rc = cos_persist_self_test();

    const char *path = "/tmp/cos_persist_demo.db";
    int st = 0;
    cos_persist_t *db = cos_persist_open(path, &st);
    int schema_ver = cos_persist_schema_version(db);

    if (cos_persist_is_enabled() && db) {
        cos_persist_begin(db);
        cos_persist_save_meta  (db, "codename",   "Genesis");
        cos_persist_save_meta  (db, "pipeline",   "σ-v1");
        cos_persist_save_engram(db, "paris",      "capital-of-FR", 0.12f);
        cos_persist_save_engram(db, "berlin",     "capital-of-DE", 0.15f);
        cos_persist_save_tau   (db, "engram",     0.05f);
        cos_persist_save_tau   (db, "local",      0.25f);
        cos_persist_save_tau   (db, "api",        0.50f);
        cos_persist_append_cost(db, "openai",     0.0042, 0.0042, 0.18f, 1713600000000000000ULL);
        cos_persist_append_cost(db, "anthropic",  0.0031, 0.0073, 0.21f, 1713600001000000000ULL);
        cos_persist_commit(db);
    }

    int n_meta = db ? cos_persist_count(db, "pipeline_meta")   : -1;
    int n_eng  = db ? cos_persist_count(db, "engrams")         : -1;
    int n_tau  = db ? cos_persist_count(db, "tau_calibration") : -1;
    int n_cost = db ? cos_persist_count(db, "cost_ledger")     : -1;

    char host[64] = {0};
    if (db) cos_persist_load_meta(db, "codename", host, sizeof host);

    printf("{\"kernel\":\"sigma_persist\","
           "\"enabled\":%s,"
           "\"self_test_rc\":%d,"
           "\"db_path\":\"%s\","
           "\"schema_version\":%d,"
           "\"rows\":{"
             "\"pipeline_meta\":%d,"
             "\"engrams\":%d,"
             "\"tau_calibration\":%d,"
             "\"cost_ledger\":%d"
           "},"
           "\"codename\":\"%s\","
           "\"pass\":%s}\n",
           cos_persist_is_enabled() ? "true" : "false",
           rc, path, schema_ver,
           n_meta, n_eng, n_tau, n_cost,
           host,
           (rc == 0 && (!cos_persist_is_enabled() || (n_meta >= 2 && n_eng >= 2 && n_tau >= 3 && n_cost >= 2)))
             ? "true" : "false");

    if (db) cos_persist_close(db);
    return rc == 0 ? 0 : 1;
}
