/* SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Creation OS v61 — example tool compiled to wasm32 for Wasmtime
 * sandbox execution.  The contract: the tool has NO direct syscalls
 * beyond what the Wasmtime host explicitly allows (it only sees what
 * σ-Shield authorized).
 *
 *   Build (WASI-SDK):
 *     $(WASI_CC) -O2 -o example_tool.wasm example_tool.c
 *   Run under Wasmtime (ambient-authority disabled by default):
 *     wasmtime run \
 *       --disable-cache \
 *       --wasi-modules=wasi-common \
 *       --dir . example_tool.wasm
 *
 * On hosts without wasmtime/WASI-SDK, `make wasm-sandbox` SKIPs.
 */
#include <stdio.h>
#include <string.h>

/* The tool's job is deliberately trivial: echo one sanitised string.
 * The sandbox contract is the point, not the work. */
int main(int argc, char **argv)
{
    const char *msg = (argc >= 2) ? argv[1] : "σ-Citadel WASM sandbox OK";
    /* Length clamp; a malicious caller cannot overflow. */
    char buf[256];
    size_t n = strnlen(msg, sizeof(buf) - 1);
    memcpy(buf, msg, n);
    buf[n] = '\0';
    printf("[tool] %s\n", buf);
    return 0;
}
