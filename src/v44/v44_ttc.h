/* SPDX-License-Identifier: AGPL-3.0-or-later
 * v44 lab: optional v41 test-time compute handoff when syndrome lands in “cite” band.
 */
#ifndef CREATION_OS_V44_TTC_H
#define CREATION_OS_V44_TTC_H

#include "sigma_proxy.h"

/** If first pass suggests CITE, retry once with a TTC-style prompt prefix; keep lower epistemic aggregate. */
int v44_sigma_proxy_with_ttc(const v44_engine_config_t *eng, const char *prompt, const v44_sigma_config_t *cfg,
    v44_proxy_response_t *out);

#endif /* CREATION_OS_V44_TTC_H */
