// SPDX-License-Identifier: AGPL-3.0-or-later
// Creation OS v37 — σ-on-silicon: multi-head XNOR bind + popcount + variance-based σ-entropy + abstain.
//
// Datapath: r0 (capture on valid_in) → r1 (XNOR + popcount) → r2 (shadow) → outputs + abstain.
// valid_out pulses when the transaction that entered on valid_in completes (##3 clocks).
// N_HEADS must be a power of two (mean/variance use >> $clog2(N_HEADS)).
//
// Protocol: keep valid_in low for at least 3 cycles between pulses so r0 is stable while r1/r2
// advance (formal wrapper assumes spacing). Overlapped tokens are v40+ (skid / second buffer).

`default_nettype none
module sigma_pipeline #(
    parameter int unsigned HV_BITS = 4096,
    parameter int unsigned N_HEADS = 16,
    parameter int unsigned SIGMA_ENTROPY_BITS = 16,
    parameter logic [SIGMA_ENTROPY_BITS-1:0] ABSTAIN_THRESHOLD = 16'h2000
) (
    input logic clk,
    input logic rst_n,
    input logic valid_in,
    input logic [HV_BITS-1:0] q_hv[N_HEADS],
    input logic [HV_BITS-1:0] k_hv[N_HEADS],
    input logic [HV_BITS-1:0] v_hv[N_HEADS],
    output logic [HV_BITS-1:0] result_hv[N_HEADS],
    output logic [12:0] similarity[N_HEADS],
    output logic [SIGMA_ENTROPY_BITS-1:0] sigma_entropy,
    output logic sigma_abstain,
    output logic valid_out
);
  localparam int unsigned POPW = $clog2(HV_BITS + 1);
  localparam int unsigned NH_LOG = $clog2(N_HEADS);

  generate
    if ((1 << NH_LOG) != N_HEADS) begin : gen_nheads_pow2
      initial $fatal(1, "sigma_pipeline: N_HEADS must be a power of two");
    end
  endgenerate

  logic [3:0] valid_sr;
  logic [HV_BITS-1:0] r0_q[N_HEADS];
  logic [HV_BITS-1:0] r0_k[N_HEADS];
  logic [HV_BITS-1:0] r0_v[N_HEADS];

  logic [HV_BITS-1:0] r1_bound[N_HEADS];
  logic [POPW-1:0] r1_pop[N_HEADS];
  logic [HV_BITS-1:0] r1_v[N_HEADS];

  logic [HV_BITS-1:0] r2_bound[N_HEADS];
  logic [POPW-1:0] r2_pop[N_HEADS];
  logic [HV_BITS-1:0] r2_v[N_HEADS];

  logic signed [48:0] var_signed_c;
  logic [SIGMA_ENTROPY_BITS-1:0] variance_u_c;
  logic abstain_c;

  always_comb begin
    automatic logic [31:0] acc;
    automatic logic [63:0] accq;
    automatic logic [31:0] mean_u;
    automatic logic [63:0] e_x2_u;
    automatic logic [63:0] mean_sq_u;
    acc = '0;
    accq = '0;
    for (int i = 0; i < N_HEADS; i++) begin
      automatic logic [63:0] pi;
      pi = 64'(r2_pop[i]);
      acc = acc + 32'(r2_pop[i]);
      accq = accq + (pi * pi);
    end
    mean_u = acc >> NH_LOG;
    e_x2_u = accq >> NH_LOG;
    mean_sq_u = 64'(mean_u) * 64'(mean_u);
    var_signed_c = $signed({1'b0, e_x2_u}) - $signed({1'b0, mean_sq_u});
    if (var_signed_c < 0) begin
      variance_u_c = '0;
    end else if (|(var_signed_c >> SIGMA_ENTROPY_BITS)) begin
      variance_u_c = {SIGMA_ENTROPY_BITS{1'b1}};
    end else begin
      variance_u_c = var_signed_c[SIGMA_ENTROPY_BITS-1:0];
    end
    abstain_c = var_signed_c > $signed({1'b0, ABSTAIN_THRESHOLD});
  end

  logic [3:0] vsr_n;
  assign vsr_n = {valid_sr[2:0], valid_in};

  always_ff @(posedge clk) begin
    if (!rst_n) begin
      valid_sr <= '0;
      valid_out <= 1'b0;
      sigma_abstain <= 1'b0;
      sigma_entropy <= '0;
      for (int hh = 0; hh < N_HEADS; hh++) begin
        r0_q[hh] <= '0;
        r0_k[hh] <= '0;
        r0_v[hh] <= '0;
        r1_bound[hh] <= '0;
        r1_pop[hh] <= '0;
        r1_v[hh] <= '0;
        r2_bound[hh] <= '0;
        r2_pop[hh] <= '0;
        r2_v[hh] <= '0;
        result_hv[hh] <= '0;
        similarity[hh] <= '0;
      end
    end else begin
      valid_sr <= vsr_n;
      valid_out <= vsr_n[3];

      if (valid_in) begin
        for (int h = 0; h < N_HEADS; h++) begin
          r0_q[h] <= q_hv[h];
          r0_k[h] <= k_hv[h];
          r0_v[h] <= v_hv[h];
        end
      end

      for (int h = 0; h < N_HEADS; h++) begin
        automatic logic [HV_BITS-1:0] xn;
        xn = r0_q[h] ^ r0_k[h];
        r1_bound[h] <= ~xn;
        r1_pop[h] <= $countones(~xn);
        r1_v[h] <= r0_v[h];
      end

      for (int h = 0; h < N_HEADS; h++) begin
        r2_bound[h] <= r1_bound[h];
        r2_pop[h] <= r1_pop[h];
        r2_v[h] <= r1_v[h];
      end

      sigma_entropy <= variance_u_c;
      sigma_abstain <= abstain_c;
      for (int h = 0; h < N_HEADS; h++) begin
        result_hv[h] <= r2_bound[h] & r2_v[h];
        similarity[h] <= 13'(r2_pop[h]);
      end
    end
  end

`ifdef FORMAL
  generate
    for (genvar gi = 0; gi < N_HEADS; gi++) begin : g_pop_bound
      always_ff @(posedge clk) begin
        if (rst_n) begin
          assert (r1_pop[gi] <= POPW'(HV_BITS));
          assert (r2_pop[gi] <= POPW'(HV_BITS));
        end
      end
    end
  endgenerate

  assert property (@(posedge clk) disable iff (!rst_n)
    (var_signed_c <= $signed({1'b0, ABSTAIN_THRESHOLD})) |-> !abstain_c);

  assert property (@(posedge clk) disable iff (!rst_n) valid_in |-> ##3 valid_out);
`endif
endmodule
`default_nettype wire
