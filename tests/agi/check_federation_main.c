#include "../../src/sigma/federation.h"

#include <stdlib.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_federation_config d;
    if (cos_federation_default_config(&d) != 0)
        return 1;
    if (d.min_trust_to_accept <= 0.f || d.min_sigma_to_share <= 0.f)
        return 2;
    return cos_federation_self_test() != 0 ? 3 : 0;
}
