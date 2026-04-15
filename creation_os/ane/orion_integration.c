#include "orion_integration.h"

#include <string.h>

int creation_os_orion_ane_forward_stub(const float *in, float *out, int n_floats) {
    if (!in || !out || n_floats <= 0)
        return -1;
    /* Deterministic pass-through until linked with Orion orion_eval. */
    memcpy(out, in, (size_t)n_floats * sizeof(float));
    return 0;
}
