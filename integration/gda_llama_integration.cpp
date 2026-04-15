/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "gda_ghost_boot.h"
#include <cstdio>

extern "C" void gda_ghost_boot_placeholder(void)
{
    std::puts("gda_llama_integration: stub C linkage entry");
}
