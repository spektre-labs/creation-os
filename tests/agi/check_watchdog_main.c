#include "../../src/sigma/coherence_watchdog.h"
#include "../../src/sigma/state_ledger.h"

#include <stdlib.h>

int main(void)
{
    if (cos_cw_self_test() != 0)
        return 1;
    if (cos_cw_stop() != -1)
        return 2;
    struct cos_cw_config c;
    cos_cw_default_config(&c);
    cos_cw_install_config(&c);

    struct cos_state_ledger ok;
    cos_state_ledger_init(&ok);
    ok.k_eff              = 0.9f;
    ok.sigma_mean_session = 0.2f;
    if (cos_cw_check(&ok) != 0)
        return 3;

    struct cos_state_ledger bad;
    cos_state_ledger_init(&bad);
    bad.k_eff              = 0.1f;
    bad.sigma_mean_session = 0.9f;
    if (cos_cw_check(&bad) != 0)
        return 4;
    cos_cw_default_config(&c);
    cos_cw_install_config(&c);
    return 0;
}
