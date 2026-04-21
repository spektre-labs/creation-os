/* ULTRA-10 — one binary runs every ULTRA kernel self-test (integration smoke). */

#include "../decode/selective.h"
#include "../learn/continuous_learn.h"
#include "../metacog/introspection.h"
#include "../metrics/energy_metric.h"
#include "../moe/sigma_router.h"
#include "../nas/sigma_search.h"
#include "../physics/coherence.h"
#include "../symbolic/neuro_sym.h"
#include "../world/jepa.h"

#include <stdio.h>
#include <string.h>

static const char *ultra_stack_ascii(void) {
    return
        "ULTRA-10 reference stack (documentation string)\n"
        "  Input -> Codex -> metacog -> engram -> sigma-MoE router\n"
        "  -> JEPA world -> neuro-symbolic gate -> selective decode\n"
        "  -> BitNet -> logprob sigma -> conformal gate -> RETHINK\n"
        "  -> ACCEPT/ABSTAIN/ESCALATE -> engram + distill\n"
        "  -> continuous learn -> coherence -> output (+ cost, reasoning/J)\n";
}

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "--describe") == 0) {
        printf("%s", ultra_stack_ascii());
        return 0;
    }
    int rc = 0;
    rc |= cos_ultra_sigma_router_self_test();
    rc |= cos_ultra_jepa_self_test();
    rc |= cos_ultra_selective_self_test();
    rc |= cos_ultra_neuro_sym_self_test();
    rc |= cos_ultra_metacog_self_test();
    rc |= cos_ultra_sigma_search_self_test();
    rc |= cos_ultra_energy_metric_self_test();
    rc |= cos_ultra_continuous_learn_self_test();
    rc |= cos_ultra_coherence_self_test();
    if (rc != 0) {
        fprintf(stderr, "creation_os_ultra_bundle: self-test failed (rc=%d)\n", rc);
        return 1;
    }
    printf("{\"kernel\":\"ultra_bundle\",\"self_test_rc\":0,\"pass\":true}\n");
    return 0;
}
