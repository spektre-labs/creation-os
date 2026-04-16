// SPDX-License-Identifier: AGPL-3.0-or-later
// Creation OS v38 — Tiny Tapeout–style σ demo tile (XNOR + popcount + abstain).
//
// Pins: ui_in[0]=MOSI, ui_in[1]=SCK (MOSI sampled on SCK rising), ui_in[7:2] ignored.
// uo_out[0]=MISO, uo_out[7]=abstain (latched), uo_out[6:1]=0.
// Phases: HV_BITS SCK rises load Q, HV_BITS load K, one wait cycle, compute, then 14 output
// bits on MISO MSB-first ({abstain, popcount[12:0]}). abstain=1 when popcount < HV_BITS/2.
// Requires ena=1. Default HV_BITS=4096; override for fast sim (e.g. -G HV_BITS=128).

`default_nettype none
module tt_um_sigma_tile #(
    parameter int unsigned HV_BITS = 4096
) (
    input  wire [7:0] ui_in,
    output wire [7:0] uo_out,
    input  wire [7:0] uio_in,
    output wire [7:0] uio_out,
    output wire [7:0] uio_oe,
    input  wire       ena,
    input  wire       clk,
    input  wire       rst_n
);
  localparam int unsigned POPW = $clog2(HV_BITS + 1);
  localparam int unsigned IDXW = $clog2(HV_BITS);

  assign uio_out = 8'h00;
  assign uio_oe  = 8'h00;
  /* verilator lint_off UNUSED */
  wire unused_uio = ^{uio_in};
  /* verilator lint_on UNUSED */

  reg sck_d;
  wire sck_rise = ui_in[1] & ~sck_d;

  typedef enum logic [2:0] {
    ST_IDLE = 3'd0,
    ST_Q = 3'd1,
    ST_K = 3'd2,
    ST_KPAD = 3'd3,
    ST_COMP = 3'd4,
    ST_OUT = 3'd5
  } st_t;

  st_t st;
  reg [IDXW-1:0] bits_have;
  reg [HV_BITS-1:0] q_shift;
  reg [HV_BITS-1:0] k_shift;
  reg [POPW-1:0] pop_r;
  reg abstain_r;
  reg [13:0] out_pkt;
  reg [3:0] out_idx;

  reg miso_r;
  assign uo_out[0]   = miso_r;
  assign uo_out[7]   = abstain_r;
  assign uo_out[6:1] = 6'h00;

  wire [HV_BITS-1:0] xn;
  wire [POPW-1:0] pop_w;
  wire [12:0] pop13;
  wire abst_w;
  wire [13:0] out_pack;
  assign xn = q_shift ^ k_shift;
  assign pop_w = $countones(~xn);
  assign pop13 = 13'(pop_w);
  assign abst_w = (pop_w < POPW'(HV_BITS / 2));
  assign out_pack = {abst_w, pop13};

  always @(posedge clk or negedge rst_n) begin
    if (!rst_n) begin
      sck_d <= 1'b0;
      st <= ST_IDLE;
      bits_have <= {IDXW{1'b0}};
      q_shift <= {HV_BITS{1'b0}};
      k_shift <= {HV_BITS{1'b0}};
      pop_r <= {POPW{1'b0}};
      abstain_r <= 1'b0;
      out_pkt <= 14'd0;
      out_idx <= 4'd0;
      miso_r <= 1'b0;
    end else begin
      sck_d <= ui_in[1];

      if (!ena) begin
        st <= ST_IDLE;
        bits_have <= {IDXW{1'b0}};
        miso_r <= 1'b0;
      end else begin
        case (st)
          ST_IDLE: begin
            if (sck_rise) begin
              st <= ST_Q;
              q_shift <= {{HV_BITS - 1{1'b0}}, ui_in[0]};
              bits_have <= {{IDXW - 1{1'b0}}, 1'b1};
            end
          end
          ST_Q: begin
            if (sck_rise) begin
              q_shift <= {q_shift[HV_BITS-2:0], ui_in[0]};
              if (bits_have == IDXW'(HV_BITS - 1)) begin
                st <= ST_K;
                k_shift <= {HV_BITS{1'b0}};
                bits_have <= {IDXW{1'b0}};
              end else begin
                bits_have <= bits_have + IDXW'(1);
              end
            end
          end
          ST_K: begin
            if (sck_rise) begin
              k_shift <= {k_shift[HV_BITS-2:0], ui_in[0]};
              if (bits_have == IDXW'(HV_BITS - 1)) begin
                st <= ST_KPAD;
              end else begin
                bits_have <= bits_have + IDXW'(1);
              end
            end
          end
          ST_KPAD: begin
            st <= ST_COMP;
          end
          ST_COMP: begin
            pop_r <= pop_w;
            abstain_r <= abst_w;
            out_pkt <= out_pack;
            out_idx <= 4'd0;
            miso_r <= out_pack[13];
            st <= ST_OUT;
          end
          ST_OUT: begin
            if (sck_rise) begin
              if (out_idx == 4'd13) begin
                st <= ST_IDLE;
                miso_r <= 1'b0;
                bits_have <= {IDXW{1'b0}};
              end else begin
                miso_r <= out_pkt[12 - out_idx];
                out_idx <= out_idx + 4'd1;
              end
            end
          end
          default: st <= ST_IDLE;
        endcase
      end
    end
  end
endmodule
`default_nettype wire
