/* TCP probe + optional Ollama /api/tags model pick for cos-chat autodetect.
 *
 * SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 */
#ifndef COS_OLLAMA_DETECT_H
#define COS_OLLAMA_DETECT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Return 0 if TCP connect to host:port succeeds within timeout_ms. */
int cos_tcp_probe_ipv4(const char *host, uint16_t port, int timeout_ms);

/** GET http://host:port/api/tags into buf (HTTP body after headers). Returns 0 on success. */
int cos_ollama_http_get_tags(const char *host, uint16_t port, char *buf, size_t cap);

/** Parse Ollama tags JSON body; prefers gemma3:4b, else first "name":"…". */
void cos_ollama_pick_model_from_tags(const char *http, char *model, size_t mcap);

/**
 * If COS_BITNET_SERVER_EXTERNAL is unset and 127.0.0.1:11434 answers,
 * set COS_BITNET_SERVER_EXTERNAL=1, COS_BITNET_SERVER_PORT=11434,
 * COS_INFERENCE_BACKEND=ollama, and model from tags when chat model unset.
 */
void cos_ollama_autodetect_apply_env(void);

#ifdef __cplusplus
}
#endif
#endif /* COS_OLLAMA_DETECT_H */
