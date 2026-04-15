// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_agency_iron_formal.sv — combinational SVA for SymbiYosys BMC
//
// Property: STABILIZE (2) is taken only when amplification, ≥2 traps, or anchor-override fires.

module cos_agency_iron_formal (
    input wire [6:0] trap_mask,
    input wire       state_declared,
    input wire       anchor_ok,
    input wire       event_state_amp,
    input wire       urgency_only
);
  wire [1:0] ctrl;
  wire [2:0] pc;
  wire       anchor_ov;
  wire       stab;

  cos_agency_iron_combo u_dut (
      .trap_mask(trap_mask),
      .state_declared(state_declared),
      .anchor_ok(anchor_ok),
      .event_state_amp(event_state_amp),
      .urgency_only(urgency_only),
      .ctrl(ctrl)
  );

  assign pc = 3'd($countones(trap_mask));
  assign anchor_ov = trap_mask[6];
  assign stab = event_state_amp | (pc >= 3'd2) | anchor_ov;

  always_comb begin
    assert (ctrl <= 2'd2);
    assert ((ctrl != 2'd2) || stab);
  end
endmodule
