// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_k_eff_bind.sv — K_eff = K · (1 − σ) in Q8.8 fixed point (σ as byte fraction)
//
// σ_q8 = 0 → full K; σ_q8 = 255 → K_eff ≈ 0. Direct bind of K(t)=ρ·I·F story to one multiply.

`timescale 1ns / 1ps

module cos_k_eff_bind (
    input  wire [15:0] K,
    input  wire [ 7:0] sigma_q8,
    output wire [15:0] K_eff
);
  wire [23:0] wide;
  assign wide = K * (24'd256 - {16'd0, sigma_q8});
  assign K_eff = wide[23:8];
endmodule
