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

/** GET /api/tags on host:port; returns malloc'd first model name or NULL. */
char *cos_ollama_first_model(uint16_t port);

/**
 * If COS_BITNET_SERVER_EXTERNAL is unset, probe 127.0.0.1:8080 (llama-server)
 * then 11434 (Ollama). On match, set COS_BITNET_SERVER_EXTERNAL=1, port,
 * COS_INFERENCE_BACKEND=ollama, and model from Ollama /api/tags when applicable.
 */
void cos_ollama_autodetect_apply_env(void);

/** 1 if host:port accepts TCP within timeout_ms, else 0. */
int cos_http_check_port(const char *host, uint16_t port, int timeout_ms);

/** Apply explicit HTTP backend: "llama-server" (8080) or "ollama" (11434). Returns 0 or -1 if unknown. */
int cos_inference_backend_apply_cli(const char *name);

#ifdef __cplusplus
}
#endif
#endif /* COS_OLLAMA_DETECT_H */
