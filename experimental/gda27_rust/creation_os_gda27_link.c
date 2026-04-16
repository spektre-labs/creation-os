/* SPDX-License-Identifier: AGPL-3.0-or-later
 * Thin TU to keep the Rust staticlib linked (symbols are otherwise unused in tiny demos).
 */
#include <stdint.h>

extern int creation_os_gda27_rust_encode(uint32_t v, char *out, int cap);
extern uint32_t creation_os_gda27_rust_decode(const char *s);

void creation_os_gda27_link_force(void)
{
    char tmp[8];
    (void)creation_os_gda27_rust_encode(0u, tmp, (int)sizeof(tmp));
    (void)creation_os_gda27_rust_decode("0");
}
