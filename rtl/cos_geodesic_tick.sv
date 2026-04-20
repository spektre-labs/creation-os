// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_geodesic_tick.sv — one σ relaxation step toward target (minimum-action toy)
//
// σ_next = σ_now − ((σ_now − σ_tgt) >> shift), clamped at σ_tgt. Unsigned only;
//   if already at/below target, hold. Maps to “one geodesic step” in harness algebra.

`timescale 1ns / 1ps

module cos_geodesic_tick (
    input  wire [15:0] sigma_now,
    input  wire [15:0] sigma_tgt,
    input  wire [ 3:0] shift,
    output logic [15:0] sigma_next
);
  logic [16:0] diff;
  logic [16:0] step;
  logic [16:0] raw_next;

  always_comb begin
    if (sigma_now <= sigma_tgt) sigma_next = sigma_now;
    else begin
      diff = sigma_now - sigma_tgt;
      step = diff >> shift;
      raw_next = sigma_now - step;
      if (raw_next < sigma_tgt)
        sigma_next = sigma_tgt;
      else
        sigma_next = raw_next[15:0];
    end
  end
endmodule
