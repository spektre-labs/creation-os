#include <metal_stdlib>
using namespace metal;

#define SK9_LW_BODY                                                           \
    if (tid >= vocab) {                                                       \
        return;                                                               \
    }                                                                         \
    uchar r = rep[tid];                                                       \
    uint  c = popcount((uint)r);                                              \
    log[tid] += ((float)c - 4.0f) * 0.5f;

kernel void sk9_lw_bias(
    device const uchar *rep    [[buffer(0)]],
    device float       *log   [[buffer(1)]],
    constant uint      &vocab  [[buffer(2)]],
    uint                   tid [[thread_position_in_grid]]) {
    SK9_LW_BODY
}

kernel void living_weights_apply(
    device const uchar *rep    [[buffer(0)]],
    device float       *log   [[buffer(1)]],
    constant uint      &vocab  [[buffer(2)]],
    uint                   tid [[thread_position_in_grid]]) {
    SK9_LW_BODY
}
