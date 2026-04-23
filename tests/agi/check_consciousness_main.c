/*
 * Consciousness meter harness (eight checks + self-test).
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#include "../../src/sigma/consciousness_meter.h"

#include <stdlib.h>
#include <string.h>

#ifndef CREATION_OS_ENABLE_SELF_TESTS
#error CREATION_OS_ENABLE_SELF_TESTS required
#endif

int main(void)
{
    struct cos_consciousness_state cs;
    float                          ch[4] = {0.4f, 0.41f, 0.39f, 0.40f};
    float                          meta[4] = {0.40f, 0.40f, 0.40f, 0.40f};
    char                          *js;

    if (cos_consciousness_init(NULL) != -1)
        return 60;
    if (cos_consciousness_measure(NULL, 0.5f, ch, NULL, 0.5f) != -1)
        return 61;

    if (cos_consciousness_init(&cs) != 0)
        return 62;

    if (cos_consciousness_measure(&cs, 0.40f, ch, meta, 0.45f) != 0)
        return 63;

    if (cs.phi_proxy < 0.f || cs.phi_proxy > 1.f)
        return 64;

    if (cs.integration < 0.f || cs.integration > 1.f)
        return 65;

    if (cos_consciousness_classify_level(&cs) < 1)
        return 66;

    js = cos_consciousness_to_json(&cs);
    if (!js || strstr(js, "level") == NULL)
        return 67;
    free(js);

    cos_consciousness_print(&cs, stdout);

    return cos_consciousness_self_test() != 0 ? 68 : 0;
}
