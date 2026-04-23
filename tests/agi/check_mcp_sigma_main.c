/* SPDX-License-Identifier: AGPL-3.0-or-later */
#include "sigma_mcp_gate.h"

#include <stdlib.h>

int main(void)
{
    return cos_sigma_mcp_gate_self_test() == 0 ? 0 : 1;
}
