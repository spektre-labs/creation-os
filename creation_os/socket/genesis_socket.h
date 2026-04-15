#ifndef CREATION_OS_GENESIS_SOCKET_H
#define CREATION_OS_GENESIS_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Binary wire (no JSON):
 * Request:  [4 bytes BE: payload_len][N: query bytes]
 * Response: [1 byte: tier][4 bytes BE: sigma][N: answer bytes]
 * Default path: getenv CREATION_OS_GENESIS_SOCK or /var/run/creation.sock;
 * if bind fails, /tmp/creation.sock (non-root).
 */
int genesis_socket_serve_forever(const char *path_override, const char *bbhash_mmap_path);

#ifdef __cplusplus
}
#endif

#endif
