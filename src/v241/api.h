/*
 * v241 σ-API-Surface — unified public rajapinta for
 *   238 kernels with four first-class SDKs and a hard
 *   OpenAI-compatible backward-compat endpoint.
 *
 *   Endpoint manifest (v0, declared in src/v241/api.c):
 *     POST /v1/chat/completions   (OpenAI-compatible)
 *     POST /v1/reason             (multi-path reasoning)
 *     POST /v1/plan               (multi-step planning)
 *     POST /v1/create             (creative generation)
 *     POST /v1/simulate           (domain simulation)
 *     POST /v1/teach              (Socratic mode)
 *     GET  /v1/health             (system status)
 *     GET  /v1/identity           (who am I)
 *     GET  /v1/coherence          (K_eff dashboard)
 *     GET  /v1/audit/stream       (real-time audit)
 *
 *   Every endpoint carries:
 *     - canonical HTTP method
 *     - canonical path under /v1/
 *     - `openai_compatible` flag
 *     - whether the response emits X-COS-* headers
 *       (every endpoint in v0 does)
 *
 *   SDKs (v0 manifest):
 *     python     → pip install creation-os
 *     javascript → npm install creation-os
 *     rust       → cargo add creation-os
 *     go         → go get github.com/spektre-labs/creation-os-go
 *
 *   Backward compatibility:
 *     - /v1/chat/completions accepts any OpenAI-shaped
 *       request unchanged
 *     - response carries `X-COS-Sigma`,
 *       `X-COS-Kernel-Path`, `X-COS-Audit-Chain`
 *
 *   Versioning:
 *     api_version_major == 1,  api_version_minor == 0
 *     /v2/ is reserved for breaking change; /v1/ is
 *     promised stable across kernel rev-ups.
 *
 *   σ_api (surface hygiene):
 *       σ_api = 1 − (endpoints_passing_audit / n_endpoints)
 *     Low σ_api ⇒ every endpoint is declared with method +
 *     path + X-COS headers + compat flag; high ⇒ the surface
 *     is fraying.
 *
 *   Contracts (v0):
 *     1. n_endpoints == 10, enumerated above.
 *     2. Exactly one endpoint sets openai_compatible=true
 *        (`POST /v1/chat/completions`).
 *     3. Every endpoint has a non-empty method, a path
 *        starting with `/v1/`, and emits_cos_headers=true.
 *     4. Methods are drawn only from {GET, POST}.
 *     5. n_sdks == 4 and the four canonical language
 *        names appear verbatim ("python", "javascript",
 *        "rust", "go").
 *     6. api_version_major == 1; api_version_minor >= 0.
 *     7. σ_api ∈ [0, 1] and equals 0.0 when every
 *        endpoint passes the audit.
 *     8. FNV-1a chain replays byte-identically.
 *
 *   v241.1 (named, not implemented): live HTTP router
 *     bound to these descriptors, SDK package-lock
 *     generation, GraphQL mirror, streaming SSE for
 *     /audit/stream.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */

#ifndef COS_V241_API_H
#define COS_V241_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define COS_V241_N_ENDPOINTS 10
#define COS_V241_N_SDKS       4

typedef struct {
    char  method[8];           /* "GET" | "POST" */
    char  path  [48];          /* "/v1/..."      */
    bool  openai_compatible;
    bool  emits_cos_headers;
    bool  audit_ok;
    char  summary[48];
} cos_v241_endpoint_t;

typedef struct {
    char  name   [16];         /* "python" | "javascript" | "rust" | "go" */
    char  install[64];         /* canonical install string */
    bool  maintained;
} cos_v241_sdk_t;

typedef struct {
    cos_v241_endpoint_t endpoints[COS_V241_N_ENDPOINTS];
    int                 n_openai_compat;
    int                 n_audit_ok;

    cos_v241_sdk_t      sdks[COS_V241_N_SDKS];
    int                 n_sdks_maintained;

    int                 api_version_major;
    int                 api_version_minor;
    float               sigma_api;

    bool                chain_valid;
    uint64_t            terminal_hash;
    uint64_t            seed;
} cos_v241_state_t;

void   cos_v241_init(cos_v241_state_t *s, uint64_t seed);
void   cos_v241_run (cos_v241_state_t *s);

size_t cos_v241_to_json(const cos_v241_state_t *s,
                         char *buf, size_t cap);

int    cos_v241_self_test(void);

#ifdef __cplusplus
}
#endif

#endif /* COS_V241_API_H */
