// SPDX-License-Identifier: AGPL-3.0-or-later
// SymbiYosys cover targets for agency triad (not part of Verilator TB lint set).
module cos_agency_iron_cover (
    input wire [6:0] trap_mask,
    input wire       state_declared,
    input wire       anchor_ok,
    input wire       event_state_amp,
    input wire       urgency_only
);
  wire [1:0] ctrl;

  cos_agency_iron_combo u_dut (
      .trap_mask(trap_mask),
      .state_declared(state_declared),
      .anchor_ok(anchor_ok),
      .event_state_amp(event_state_amp),
      .urgency_only(urgency_only),
      .ctrl(ctrl)
  );

  always @(*) begin
    cover (ctrl == 2'h0);
    cover (ctrl == 2'h1);
    cover (ctrl == 2'h2);
  end
endmodule
