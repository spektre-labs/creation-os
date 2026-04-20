/* σ-Agent self-test binary + canonical OODA + gate demo. */

#include "agent.h"

#include <stdio.h>
#include <string.h>

static const char *phase_name(cos_ooda_phase_t p) {
    switch (p) {
        case COS_OODA_OBSERVE: return "OBSERVE";
        case COS_OODA_ORIENT:  return "ORIENT";
        case COS_OODA_DECIDE:  return "DECIDE";
        case COS_OODA_ACT:     return "ACT";
        case COS_OODA_REFLECT: return "REFLECT";
        default:               return "?";
    }
}

static const char *decision_name(cos_agent_decision_t d) {
    switch (d) {
        case COS_AGENT_ALLOW:   return "ALLOW";
        case COS_AGENT_CONFIRM: return "CONFIRM";
        case COS_AGENT_BLOCK:   return "BLOCK";
        default:                return "?";
    }
}

int main(int argc, char **argv) {
    int rc = cos_sigma_agent_self_test();

    /* Canonical demo: base autonomy 0.80, confirm margin 0.10.
     *   σ = 0.10  → effective = 0.72  → READ ALLOW, WRITE ALLOW,
     *                                   NETWORK CONFIRM (0.72 in
     *                                   [0.70, 0.80)), IRREV BLOCK.
     *   σ = 0.50  → effective = 0.40  → READ ALLOW, WRITE BLOCK
     *                                   (0.40 < 0.50, below CONFIRM
     *                                   band), NETWORK BLOCK,
     *                                   IRREV BLOCK. */
    cos_sigma_agent_t a;
    cos_sigma_agent_init(&a, 0.80f, 0.10f);

    cos_agent_decision_t d_low_read    = cos_sigma_agent_gate(&a, COS_ACT_READ,         0.10f);
    cos_agent_decision_t d_low_write   = cos_sigma_agent_gate(&a, COS_ACT_WRITE,        0.10f);
    cos_agent_decision_t d_low_net     = cos_sigma_agent_gate(&a, COS_ACT_NETWORK,      0.10f);
    cos_agent_decision_t d_low_irrev   = cos_sigma_agent_gate(&a, COS_ACT_IRREVERSIBLE, 0.10f);

    cos_agent_decision_t d_hi_read     = cos_sigma_agent_gate(&a, COS_ACT_READ,         0.50f);
    cos_agent_decision_t d_hi_write    = cos_sigma_agent_gate(&a, COS_ACT_WRITE,        0.50f);
    cos_agent_decision_t d_hi_net      = cos_sigma_agent_gate(&a, COS_ACT_NETWORK,      0.50f);

    /* Drive one full OODA cycle to populate calibration counters. */
    cos_sigma_agent_advance(&a, COS_OODA_OBSERVE);
    cos_sigma_agent_advance(&a, COS_OODA_ORIENT);
    cos_sigma_agent_advance(&a, COS_OODA_DECIDE);
    cos_sigma_agent_advance(&a, COS_OODA_ACT);
    cos_sigma_agent_reflect(&a, 0.20f, 0.30f);  /* err 0.10 */
    cos_sigma_agent_advance(&a, COS_OODA_REFLECT);

    printf("{\"kernel\":\"sigma_agent\","
           "\"self_test_rc\":%d,"
           "\"demo\":{"
             "\"base_autonomy\":%.4f,"
             "\"confirm_margin\":%.4f,"
             "\"low_sigma\":{"
                "\"sigma\":0.1000,\"effective\":%.4f,"
                "\"read\":\"%s\",\"write\":\"%s\","
                "\"network\":\"%s\",\"irreversible\":\"%s\"},"
             "\"hi_sigma\":{"
                "\"sigma\":0.5000,\"effective\":%.4f,"
                "\"read\":\"%s\",\"write\":\"%s\",\"network\":\"%s\"},"
             "\"cycles\":%d,"
             "\"calibration_err\":%.4f,"
             "\"phase\":\"%s\","
             "\"action_min_autonomy\":[%.2f,%.2f,%.2f,%.2f]},"
           "\"pass\":%s}\n",
           rc,
           (double)a.base_autonomy, (double)a.confirm_margin,
           (double)(a.base_autonomy * (1.0f - 0.10f)),
           decision_name(d_low_read), decision_name(d_low_write),
           decision_name(d_low_net),  decision_name(d_low_irrev),
           (double)(a.base_autonomy * (1.0f - 0.50f)),
           decision_name(d_hi_read), decision_name(d_hi_write),
           decision_name(d_hi_net),
           a.n_cycles,
           (double)cos_sigma_agent_calibration_err(&a),
           phase_name(a.phase),
           (double)cos_sigma_agent_min_autonomy(COS_ACT_READ),
           (double)cos_sigma_agent_min_autonomy(COS_ACT_WRITE),
           (double)cos_sigma_agent_min_autonomy(COS_ACT_NETWORK),
           (double)cos_sigma_agent_min_autonomy(COS_ACT_IRREVERSIBLE),
           (rc == 0) ? "true" : "false");

    if (argc > 1 && strcmp(argv[1], "--demo") == 0) {
        fprintf(stderr, "σ-Agent demo (base autonomy %.2f, margin %.2f):\n",
            (double)a.base_autonomy, (double)a.confirm_margin);
        fprintf(stderr, "  σ=0.10  READ %s  WRITE %s  NETWORK %s  IRREV %s\n",
            decision_name(d_low_read), decision_name(d_low_write),
            decision_name(d_low_net),  decision_name(d_low_irrev));
        fprintf(stderr, "  σ=0.50  READ %s  WRITE %s  NETWORK %s\n",
            decision_name(d_hi_read), decision_name(d_hi_write),
            decision_name(d_hi_net));
    }
    return rc;
}
