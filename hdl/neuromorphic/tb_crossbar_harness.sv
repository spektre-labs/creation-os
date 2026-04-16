// SPDX-License-Identifier: AGPL-3.0-or-later
// Verilator-friendly harness for ternary_crossbar (tiny geometry).

`timescale 1ns/1ps
`default_nettype none
module tb_crossbar_harness;
  localparam int unsigned R = 8;
  localparam int unsigned C = 8;

  logic clk;
  logic [7:0] input_vec[R];
  logic [1:0] weight_mat[R][C];
  wire [15:0] output_vec[C];
  wire [7:0] sigma_hw[C];

  ternary_crossbar #(
      .ROWS(R),
      .COLS(C)
  ) dut (
      .clk(clk),
      .input_vec(input_vec),
      .weight_mat(weight_mat),
      .output_vec(output_vec),
      .sigma_hw(sigma_hw)
  );

  initial clk = 1'b0;
  always #5 clk = ~clk;

  integer i, j;
  initial begin
    for (i = 0; i < int'(R); i++) begin
      input_vec[i] = 8'(i + 1);
    end
    for (i = 0; i < int'(R); i++) begin
      for (j = 0; j < int'(C); j++) begin
        weight_mat[i][j] = ((i + j) % 3 == 0) ? 2'b01 : ((i + j) % 3 == 1) ? 2'b10 : 2'b00;
      end
    end
    @(posedge clk);
    @(posedge clk);
    @(posedge clk);
    if (output_vec[0] === 16'hXXXX) begin
      $display("tb_crossbar_harness: FAIL output unknown");
      $fatal(1);
    end
    $display("tb_crossbar_harness: OK out0=%0d sigma0=%0d", output_vec[0], sigma_hw[0]);
    $finish(0);
  end
endmodule
`default_nettype wire
