/* SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
 * sigma_core.v — σ RISC-V custom CFU (soft core, lab).
 *
 * Behavioral reference: src/inference/cos_sigma_mirror.h
 * Verdict order matches python/cos/sigma_gate_core.py (do not edit sigma_gate.h from here).
 *
 * Synthesis: hw/riscv/yosys_sigma_cfu.ys — ``python3 -m cos silicon --synth``
 */
module sigma_rv_cfu (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [6:0]  funct7,
    input  wire [2:0]  funct3,
    input  wire [31:0] rs1,
    input  wire [31:0] rs2,
    output reg  [31:0] rd,
    output reg         done
);

    localparam [31:0] COS_SIGMA_Q16    = 32'd65536;
    localparam [31:0] COS_SIGMA_K_CRIT = 32'd8323; /* round(0.127 * 65536) */
    localparam [31:0] SPIKE_THR        = 32'h0000_CCCC; /* lab LIF threshold */

    reg [31:0] sigma_q;
    reg signed [31:0] d_sigma_q;
    reg [31:0] k_eff_q;
    reg [31:0] lif_vm_q;

    wire [31:0]       new_sigma = rs1[31:0];
    wire [31:0]       k_raw_in  = rs2[31:0];
    wire signed [31:0] d_next   = $signed({1'b0, new_sigma}) - $signed({1'b0, sigma_q});

    wire [63:0] ke_wide;
    wire [31:0] ke_raw;
    assign ke_wide = ({32'd0, COS_SIGMA_Q16} - {32'd0, new_sigma}) * {32'd0, k_raw_in};
    assign ke_raw  = ke_wide[47:16];

    wire [31:0] ke_clamped;
    assign ke_clamped = (ke_raw > (COS_SIGMA_Q16 - 32'd1)) ? (COS_SIGMA_Q16 - 32'd1) : ke_raw[31:0];

    wire signed [31:0] act8 = { {24{rs2[7]}}, rs2[7:0] };

    wire [32:0] spike_sum;
    assign spike_sum = {1'b0, rs1[31:0]} + {1'b0, rs2[31:0]};

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            sigma_q    <= 32'd0;
            d_sigma_q  <= 32'sd0;
            k_eff_q    <= COS_SIGMA_Q16; /* ~1.0 in Q16 lane, matches cos_sigma_q16_init */
            lif_vm_q   <= 32'd0;
            rd         <= 32'd0;
            done       <= 1'b0;
        end else begin
            done <= 1'b1;
            case (funct7)
                /* SIGMA.UPDATE — rs1=new_sigma Q16, rs2=k_raw Q16 */
                7'h01: begin
                    d_sigma_q <= d_next;
                    sigma_q   <= new_sigma;
                    k_eff_q   <= ke_clamped;
                    rd        <= ke_clamped;
                end
                /* SIGMA.GATE — CSR-only in this CFU */
                7'h02: begin
                    if (k_eff_q < COS_SIGMA_K_CRIT)
                        rd <= 32'd2; /* ABSTAIN */
                    else if (d_sigma_q > 0)
                        rd <= 32'd1; /* RETHINK */
                    else
                        rd <= 32'd0; /* ACCEPT */
                end
                /* TERN.MATVEC — one weight: rs1[1:0] code, rs2 int8 activations */
                7'h03: begin
                    case (rs1[1:0])
                        2'b01:   rd <= $unsigned(act8);
                        2'b10:   rd <= - $signed(act8);
                        default: rd <= 32'd0;
                    endcase
                end
                /* SIGMA.CASCADE — placeholder (integration hook) */
                7'h04: begin
                    rd <= 32'd5;
                end
                /* SIGMA.SPIKE — lab integrate-and-fire */
                7'h05: begin
                    if (spike_sum[31:0] > SPIKE_THR) begin
                        rd       <= 32'd1;
                        lif_vm_q <= 32'd0;
                    end else begin
                        rd       <= 32'd0;
                        lif_vm_q <= spike_sum[31:0];
                    end
                end
                default: begin
                    rd   <= 32'd0;
                    done <= 1'b0;
                end
            endcase
        end
    end
endmodule
