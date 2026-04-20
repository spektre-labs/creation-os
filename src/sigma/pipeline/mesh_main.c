/* σ-Mesh self-test + canonical 4-node routing demo. */

#include "mesh.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    int rc = cos_sigma_mesh_self_test();

    cos_mesh_t mesh;
    cos_mesh_node_t slots[8];
    cos_sigma_mesh_init(&mesh, slots, 8,
                        0.70f,   /* w_sigma   */
                        0.30f,   /* w_latency */
                        100.0f,  /* latency_norm_ms */
                        0.80f,   /* tau_abstain */
                        0.80f);  /* ewma_alpha  */
    cos_sigma_mesh_register(&mesh, "self",   "127.0.0.1:7000", 1, 0.30f,   0.0f);
    cos_sigma_mesh_register(&mesh, "strong", "10.0.0.2:7000",  0, 0.10f,  40.0f);
    cos_sigma_mesh_register(&mesh, "slow",   "10.0.0.3:7000",  0, 0.15f, 300.0f);
    cos_sigma_mesh_register(&mesh, "bad",    "10.0.0.4:7000",  0, 0.90f,  20.0f);

    /* Scenario A: local σ high (0.90) → route to best remote. */
    const cos_mesh_node_t *pick_high = cos_sigma_mesh_route(&mesh, 0.90f, 0.30f, 0);

    /* Scenario B: local σ low (0.10) + allow_self → self. */
    const cos_mesh_node_t *pick_low  = cos_sigma_mesh_route(&mesh, 0.10f, 0.30f, 1);

    /* Scenario C: report a success on "strong" → latency & σ drop. */
    cos_sigma_mesh_report(&mesh, "strong", 0.05f, 30.0f, 0.001, 1);
    int idx_strong = -1; cos_sigma_mesh_find(&mesh, "strong", &idx_strong);

    /* Scenario D: malicious burst on "bad" → σ → ~1.0, routing
     * refuses it. */
    for (int i = 0; i < 3; ++i)
        cos_sigma_mesh_report(&mesh, "bad", 1.0f, 150.0f, 0.0, 0);
    int idx_bad = -1; cos_sigma_mesh_find(&mesh, "bad", &idx_bad);

    /* Scenario E: take every remote down → NULL = ABSTAIN when
     * local σ is also above the abstain floor. */
    cos_sigma_mesh_set_available(&mesh, "strong", 0);
    cos_sigma_mesh_set_available(&mesh, "slow",   0);
    cos_sigma_mesh_set_available(&mesh, "bad",    0);
    cos_sigma_mesh_set_available(&mesh, "self",   0);
    const cos_mesh_node_t *pick_abstain = cos_sigma_mesh_route(&mesh, 0.90f, 0.30f, 0);

    printf("{\"kernel\":\"sigma_mesh\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"count\":%d,"
             "\"weights\":{\"sigma\":%.2f,\"latency\":%.2f},"
             "\"tau_abstain\":%.2f,"
             "\"route_local_high\":\"%s\","
             "\"route_local_low_allow_self\":\"%s\","
             "\"strong_after_success\":{\"sigma\":%.4f,\"latency_ms\":%.2f,"
                                       "\"successes\":%d,\"failures\":%d},"
             "\"bad_after_failures\":{\"sigma\":%.4f,"
                                      "\"successes\":%d,\"failures\":%d},"
             "\"abstain_all_down\":%s"
             "},"
           "\"pass\":%s}\n",
           rc,
           mesh.count,
           (double)mesh.w_sigma, (double)mesh.w_latency,
           (double)mesh.tau_abstain,
           pick_high ? pick_high->node_id : "null",
           pick_low  ? pick_low->node_id  : "null",
           (double)mesh.slots[idx_strong].sigma_capability,
           (double)mesh.slots[idx_strong].latency_ms,
           mesh.slots[idx_strong].n_successes,
           mesh.slots[idx_strong].n_failures,
           (double)mesh.slots[idx_bad].sigma_capability,
           mesh.slots[idx_bad].n_successes,
           mesh.slots[idx_bad].n_failures,
           pick_abstain ? "false" : "true",
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Mesh demo: high→%s low→%s abstain→%s\n",
                pick_high ? pick_high->node_id : "(null)",
                pick_low  ? pick_low->node_id  : "(null)",
                pick_abstain ? pick_abstain->node_id : "(none)");
    }
    return rc;
}
