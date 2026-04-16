// SPDX-License-Identifier: AGPL-3.0-or-later
// v39 — digital toy model of a ternary crossbar MVM + a crude per-column “hardware σ” counter.
//
// Not a memristor SPICE model: deterministic-ish digital MAC + pseudo noise for Verilator smoke.

`default_nettype none
module ternary_crossbar #(
    parameter int unsigned ROWS = 8,
    parameter int unsigned COLS = 8
) (
    input logic clk,
    input logic [7:0] input_vec[ROWS],
    input logic [1:0] weight_mat[ROWS][COLS],
    output logic [15:0] output_vec[COLS],
    output logic [7:0] sigma_hw[COLS]
);
  genvar gc;
  generate
    for (gc = 0; gc < COLS; gc++) begin : g_cols
      always_ff @(posedge clk) begin
        automatic int signed acc;
        automatic int unsigned noise_acc;
        acc = 0;
        noise_acc = 0;
        for (int r = 0; r < int'(ROWS); r++) begin
          case (weight_mat[r][gc])
            2'b01: acc = acc + int'(signed'(input_vec[r]));
            2'b10: acc = acc - int'(signed'(input_vec[r]));
            default: ;
          endcase
          noise_acc = noise_acc + unsigned'($urandom_range(0, 1));
        end
        if (acc > 32767) begin
          acc = 32767;
        end else if (acc < -32768) begin
          acc = -32768;
        end
        output_vec[gc] <= 16'(acc);
        if (noise_acc > 255) begin
          noise_acc = 255;
        end
        sigma_hw[gc] <= 8'(noise_acc);
      end
    end
  endgenerate
endmodule
`default_nettype wire
