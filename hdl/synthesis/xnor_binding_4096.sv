// SPDX-License-Identifier: AGPL-3.0-or-later
// Combinational XNOR bind + popcount width (Yosys-friendly).
module xnor_binding_4096 (
    input  wire [4095:0] a,
    input  wire [4095:0] b,
    output wire [4095:0] result,
    output wire [12:0]  popcount
);
  assign result = ~(a ^ b);
  assign popcount = $countones(result);
endmodule
