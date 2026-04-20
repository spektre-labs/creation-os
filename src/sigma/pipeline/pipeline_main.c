/* sigma_pipeline self-test + canonical end-to-end demo.
 *
 * Drives a deterministic stub generator through the full 15-
 * primitive composition (engram → BitNet-sized generate → σ-gate
 * → RETHINK → escalate → agent → cost-track → engram-store), then
 * prints a JSON summary of the last three turns.  That summary is
 * what `check_sigma_pipeline_compose.sh` pins.
 */

#include "pipeline.h"
#include "reinforce.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Same stub as the self-test: keyed-prefix σ that covers every
 * branch.  Duplicated here (not exported from pipeline.c) so the
 * self-test and the demo can diverge later without coupling. */
static int demo_generate(const char *prompt, int round, void *ctx,
                         const char **out_text, float *out_sigma,
                         double *out_cost_eur) {
    (void)ctx;
    *out_cost_eur = 0.0001;
    if (strncmp(prompt, "low:", 4) == 0) {
        *out_text  = "4"; *out_sigma = 0.05f;
    } else if (strncmp(prompt, "improve:", 8) == 0) {
        float t[] = { 0.55f, 0.45f, 0.35f };
        int r = round < 3 ? round : 2;
        *out_text  = "improving answer"; *out_sigma = t[r];
    } else if (strncmp(prompt, "mid:", 4) == 0) {
        *out_text  = "plateau draft"; *out_sigma = 0.55f;
    } else {
        *out_text  = "default answer"; *out_sigma = 0.30f;
    }
    return 0;
}

static int demo_escalate(const char *prompt, void *ctx,
                         const char **out_text, float *out_sigma,
                         double *out_cost_eur,
                         uint64_t *out_bytes_sent,
                         uint64_t *out_bytes_recv) {
    (void)prompt; (void)ctx;
    *out_text       = "escalated cloud answer";
    *out_sigma      = 0.10f;
    *out_cost_eur   = 0.012;
    *out_bytes_sent = 1024;
    *out_bytes_recv = 4096;
    return 0;
}

int main(int argc, char **argv) {
    int rc = cos_sigma_pipeline_self_test();

    /* Build shared state. */
    enum { N_SLOTS = 16 };
    cos_sigma_engram_entry_t slots[N_SLOTS];
    memset(slots, 0, sizeof(slots));
    cos_sigma_engram_t engram;
    cos_sigma_engram_init(&engram, slots, N_SLOTS, 0.25f, 100, 10);

    cos_sigma_sovereign_t sv;
    cos_sigma_sovereign_init(&sv, 0.85f);
    cos_sigma_agent_t ag;
    cos_sigma_agent_init(&ag, 0.80f, 0.10f);

    cos_sigma_codex_t codex;
    memset(&codex, 0, sizeof(codex));
    int codex_rc = cos_sigma_codex_load(NULL, &codex);

    cos_pipeline_config_t cfg;
    cos_sigma_pipeline_config_defaults(&cfg);
    cfg.tau_accept   = 0.40f;
    cfg.tau_rethink  = 0.60f;
    cfg.max_rethink  = 3;
    cfg.mode         = COS_PIPELINE_MODE_HYBRID;
    cfg.codex        = (codex_rc == 0) ? &codex : NULL;
    cfg.engram       = &engram;
    cfg.sovereign    = &sv;
    cfg.agent        = &ag;
    cfg.generate     = demo_generate;
    cfg.escalate     = demo_escalate;

    /* Three canonical turns: ACCEPT, HIT (same prompt), ESCALATE. */
    cos_pipeline_result_t r1, r2, r3;
    cos_sigma_pipeline_run(&cfg, "low:2+2",   &r1);
    cos_sigma_pipeline_run(&cfg, "low:2+2",   &r2);  /* HIT        */
    cos_sigma_pipeline_run(&cfg, "mid:prove", &r3);  /* escalate   */

    printf("{\"kernel\":\"sigma_pipeline\","
           "\"self_test_rc\":%d,"
           "\"codex\":{\"loaded\":%s,\"hash\":\"%016llx\","
                     "\"chapters\":%d,\"bytes\":%zu},"
           "\"demo\":["
             "{\"turn\":1,\"action\":\"%s\",\"sigma\":%.4f,"
              "\"engram_hit\":%s,\"rethink\":%d,\"escalated\":%s,"
              "\"cost_eur\":%.6f,\"diag\":\"%s\"},"
             "{\"turn\":2,\"action\":\"%s\",\"sigma\":%.4f,"
              "\"engram_hit\":%s,\"rethink\":%d,\"escalated\":%s,"
              "\"cost_eur\":%.6f,\"diag\":\"%s\"},"
             "{\"turn\":3,\"action\":\"%s\",\"sigma\":%.4f,"
              "\"engram_hit\":%s,\"rethink\":%d,\"escalated\":%s,"
              "\"cost_eur\":%.6f,\"diag\":\"%s\"}],"
           "\"ledger\":{\"n_local\":%u,\"n_cloud\":%u,\"n_abstain\":%u,"
                      "\"eur_total\":%.6f},"
           "\"pass\":%s}\n",
           rc,
           (codex_rc == 0) ? "true" : "false",
           (unsigned long long)codex.hash_fnv1a64,
           codex.chapters_found, codex.size,
           cos_sigma_action_label(r1.final_action), r1.sigma,
             r1.engram_hit ? "true" : "false", r1.rethink_count,
             r1.escalated ? "true" : "false", r1.cost_eur,
             r1.diagnostic ? r1.diagnostic : "",
           cos_sigma_action_label(r2.final_action), r2.sigma,
             r2.engram_hit ? "true" : "false", r2.rethink_count,
             r2.escalated ? "true" : "false", r2.cost_eur,
             r2.diagnostic ? r2.diagnostic : "",
           cos_sigma_action_label(r3.final_action), r3.sigma,
             r3.engram_hit ? "true" : "false", r3.rethink_count,
             r3.escalated ? "true" : "false", r3.cost_eur,
             r3.diagnostic ? r3.diagnostic : "",
           sv.n_local, sv.n_cloud, sv.n_abstain,
           sv.eur_local_total + sv.eur_cloud_total,
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr,
            "sigma_pipeline demo:\n"
            "  turn 1 (low:2+2):   %s σ=%.3f %s\n"
            "  turn 2 (low:2+2):   %s σ=%.3f %s\n"
            "  turn 3 (mid:prove): %s σ=%.3f %s\n"
            "  ledger:             %u local · %u cloud · %u abstain · €%.4f\n",
            cos_sigma_action_label(r1.final_action), r1.sigma,
              r1.engram_hit ? "(cache)" : "",
            cos_sigma_action_label(r2.final_action), r2.sigma,
              r2.engram_hit ? "(cache)" : "",
            cos_sigma_action_label(r3.final_action), r3.sigma,
              r3.escalated ? "(cloud)" : "",
            sv.n_local, sv.n_cloud, sv.n_abstain,
            sv.eur_local_total + sv.eur_cloud_total);
    }

    cos_sigma_pipeline_free_engram_values(&engram);
    cos_sigma_codex_free(&codex);
    return rc;
}
