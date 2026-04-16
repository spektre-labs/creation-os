// SPDX-License-Identifier: AGPL-3.0-or-later
// Optional Metal living-weights kernel (compile to metallib out-of-tree).

#include <metal_stdlib>
using namespace metal;

kernel void creation_os_living_weights_apply(
    device const uchar *reputation [[buffer(0)]],
    device float *logits [[buffer(1)]],
    constant float &scale [[buffer(2)]],
    uint id [[thread_position_in_grid]])
{
    uchar h = reputation[id];
    uint pc = popcount((uint)h);
    logits[id] += ((float)pc - 4.0f) * scale;
}

