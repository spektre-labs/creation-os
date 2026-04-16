// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_looplm_drum.sv — weight-tied LoopLM “drum” in silicon (toy Q8)
//
// Core insight: one layer A every clock (same mix coeffs), σ-port on mean(h')
//   after each beat. Exit when mean ≤ sigma_thresh or t reaches max_loops.
// Same physical weights rotated like a washing machine — not 96 distinct layers.

module cos_looplm_drum #(
    parameter int unsigned W = 8
) (
    input  wire                    clk,
    input  wire                    rst_n,
    input  wire                    start,
    input  wire [      W*8-1:0]    din_flat,
    input  wire [            7:0] sigma_thresh,
    input  wire [            4:0] max_loops,  // 1..31
    output logic                   done,
    output logic                   busy,
    output logic [           4:0] loops_used,
    output logic                   exited_early,
    output logic [      W*8-1:0] dout_flat
);
  localparam int unsigned SUMW = 16;

  typedef enum logic [1:0] {
    ST_IDLE,
    ST_RUN,
    ST_FIN
  } st_t;
  st_t st;

  logic [7:0] h[0:W-1];
  logic [7:0] h_next[0:W-1];
  logic [SUMW-1:0] sum_h;
  logic [7:0] scale;
  logic [SUMW-1:0] sum_next;
  logic [7:0] sigma_after;
  logic [4:0] t;
  logic [9:0] mix_line;

  integer j;
  integer jj;

  always_comb begin
    sum_h = '0;
    for (j = 0; j < W; j++) sum_h = sum_h + {{(SUMW - 8) {1'b0}}, h[j]};
    scale = sum_h / W;
    for (j = 0; j < W; j++) begin
      mix_line = 10'(h[j]) * 10'd3 + 10'(scale);
      h_next[j] = mix_line[9:2];
    end
    sum_next = '0;
    for (j = 0; j < W; j++) sum_next = sum_next + {{(SUMW - 8) {1'b0}}, h_next[j]};
    sigma_after = sum_next / W;
  end

  always_ff @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      st <= ST_IDLE;
      done <= 1'b0;
      busy <= 1'b0;
      loops_used <= '0;
      exited_early <= 1'b0;
      t <= '0;
      dout_flat <= '0;
      for (jj = 0; jj < W; jj++) h[jj] <= 8'h00;
    end else begin
      case (st)
        ST_IDLE: begin
          done <= 1'b0;
          if (start) begin
            for (jj = 0; jj < W; jj++) h[jj] <= din_flat[8*jj+:8];
            busy <= 1'b1;
            t <= 5'd0;
            st <= ST_RUN;
          end else busy <= 1'b0;
        end
        ST_RUN: begin
          for (jj = 0; jj < W; jj++) h[jj] <= h_next[jj];
          loops_used <= t + 5'd1;
          if ((sigma_after <= sigma_thresh) || ((t + 5'd1) >= max_loops)) begin
            exited_early <= (sigma_after <= sigma_thresh) && ((t + 5'd1) < max_loops);
            for (jj = 0; jj < W; jj++) dout_flat[8*jj+:8] <= h_next[jj];
            done <= 1'b1;
            busy <= 1'b0;
            st <= ST_FIN;
          end else t <= t + 5'd1;
        end
        ST_FIN: begin
          busy <= 1'b0;
          if (!start) begin
            done <= 1'b0;
            st <= ST_IDLE;
          end
        end
        default: st <= ST_IDLE;
      endcase
    end
  end
endmodule
