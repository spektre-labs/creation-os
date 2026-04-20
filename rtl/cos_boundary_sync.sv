// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_boundary_sync.sv — 2-flop synchronizer (async boundary → clock domain)

`timescale 1ns / 1ps

module cos_boundary_sync (
    input  wire clk,
    input  wire rst_n,
    input  wire d,
    output reg  q
);
  reg s1;
  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      s1 <= 1'b0;
      q  <= 1'b0;
    end else begin
      s1 <= d;
      q  <= s1;
    end
  end
endmodule
