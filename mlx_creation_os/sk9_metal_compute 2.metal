/*
 * Creation OS v7 — SNL-shaped parallel uint32 ops (GPU). ! ≈ popcount, ^, &
 * MSL; compiled to sk9_metal_compute.metallib by build_amx_native.sh
 */
#include <metal_stdlib>
using namespace metal;

static inline uint popcnt_u32(uint x) {
    x = x - ((x >> 1) & 0x55555555u);
    x = (x & 0x33333333u) + ((x >> 2) & 0x33333333u);
    x = (x + (x >> 4)) & 0x0F0F0F0Fu;
    return (x * 0x01010101u) >> 24;
}

kernel void snl_symbol_parallel(
    device const uint32_t *lane_a [[buffer(0)]],
    device const uint32_t *lane_b [[buffer(1)]],
    device uint32_t *out_pop [[buffer(2)]],
    device uint32_t *out_xor [[buffer(3)]],
    device uint32_t *out_and [[buffer(4)]],
    constant uint &n [[buffer(5)]],
    uint idx [[thread_position_in_grid]]) {
    if (idx >= n)
        return;
    uint a = lane_a[idx];
    uint b = lane_b[idx];
    out_pop[idx] = popcnt_u32(a);
    out_xor[idx] = a ^ b;
    out_and[idx] = a & b;
}
