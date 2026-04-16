// SPDX-License-Identifier: AGPL-3.0-or-later
// SymbiYosys harness for sigma_pipeline (small geometry for BMC).

`default_nettype none
module sigma_pipeline_formal;
  localparam int unsigned HV_BITS = 32;
  localparam int unsigned N_HEADS = 4;
  localparam logic [15:0] ABSTAIN_THRESHOLD = 16'h0080;

  reg clk;
  reg rst_n;
  reg valid_in;
  reg [HV_BITS-1:0] q_hv[N_HEADS];
  reg [HV_BITS-1:0] k_hv[N_HEADS];
  reg [HV_BITS-1:0] v_hv[N_HEADS];

  wire [HV_BITS-1:0] result_hv[N_HEADS];
  wire [12:0] similarity[N_HEADS];
  wire [15:0] sigma_entropy;
  wire sigma_abstain;
  wire valid_out;

  sigma_pipeline #(
      .HV_BITS(HV_BITS),
      .N_HEADS(N_HEADS),
      .SIGMA_ENTROPY_BITS(16),
      .ABSTAIN_THRESHOLD(ABSTAIN_THRESHOLD)
  ) dut (
      .clk(clk),
      .rst_n(rst_n),
      .valid_in(valid_in),
      .q_hv(q_hv),
      .k_hv(k_hv),
      .v_hv(v_hv),
      .result_hv(result_hv),
      .similarity(similarity),
      .sigma_entropy(sigma_entropy),
      .sigma_abstain(sigma_abstain),
      .valid_out(valid_out)
  );

  initial clk = 1'b0;
  always #1 clk = ~clk;

  initial rst_n = 1'b1;
  initial valid_in = 1'b0;

  always @(posedge clk) begin
    assume (rst_n);
    // One valid pulse every >=4 cycles so r0 stays stable through r1/r2.
    assume property (valid_in |=> (!valid_in) [* 3]);
  end
endmodule
`default_nettype wire
