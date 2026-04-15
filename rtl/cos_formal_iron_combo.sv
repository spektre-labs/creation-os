// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_formal_iron_combo.sv — formal stack + limits + sim≠commit (combinational)

module cos_formal_iron_combo #(
    parameter int unsigned W = 6
) (
    input  wire [W-1:0] from_depth,
    input  wire [W-1:0] to_depth,
    input  wire         bridge_ok,
    input  wire [5:0]   attempted_limits,
    input  wire         sim_lane,
    input  wire         commit_token,
    input  wire         semantic_claim,
    output wire           stack_step_ok,
    output wire           limits_bad,
    output wire           sim_commit_forbidden,
    output wire [5:0]   level_tag
);

  wire retreat_or_same = (to_depth <= from_depth);
  wire [W-1:0] delta   = to_depth - from_depth;
  wire advance_one     = (delta == {{(W-1){1'b0}}, 1'b1});
  assign stack_step_ok = retreat_or_same | (advance_one & bridge_ok);

  localparam logic [5:0] FORBIDDEN_MASK = 6'h3F;
  assign limits_bad = |(attempted_limits & FORBIDDEN_MASK) | semantic_claim;
  assign sim_commit_forbidden = sim_lane & commit_token;

  reg [5:0] tag_r;
  always_comb begin
    case (from_depth)
      6'd0: tag_r = 6'd0;
      6'd1: tag_r = 6'd1;
      6'd2: tag_r = 6'd2;
      6'd3: tag_r = 6'd3;
      default: begin
        if (from_depth < 6'd32)
          tag_r = 6'd4 + (from_depth - 6'd4);
        else
          tag_r = 6'd4 + (6'd32 - 6'd5);
      end
    endcase
  end
  assign level_tag = tag_r;

endmodule
