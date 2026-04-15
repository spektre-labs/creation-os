// SPDX-License-Identifier: AGPL-3.0-or-later
// cos_silicon_chip_tb.sv — self-checking bench for formal + agency + commit + sync

`timescale 1ns / 1ps

module cos_silicon_chip_tb;
  reg        clk, rst_n;
  reg  [5:0] fd, td;
  reg        bridge, sim, ctok, sem;
  reg  [5:0] att;
  wire       f_ok, f_bad, f_simbad;
  wire [5:0] f_tag;

  reg  [6:0] traps;
  reg        st, an, amp, urg;
  wire [1:0] ag_ctrl;

  reg  [1:0] c_ctrl;
  reg        c_owner;
  reg  [7:0] c_ctx, c_ign;
  reg        c_reqf, c_model;
  wire       c_allow;

  reg        raw_bit;
  wire       syn_q;

  cos_formal_iron_combo u_formal (
      .from_depth(fd),
      .to_depth(td),
      .bridge_ok(bridge),
      .attempted_limits(att),
      .sim_lane(sim),
      .commit_token(ctok),
      .semantic_claim(sem),
      .stack_step_ok(f_ok),
      .limits_bad(f_bad),
      .sim_commit_forbidden(f_simbad),
      .level_tag(f_tag)
  );

  cos_agency_iron_combo u_agency (
      .trap_mask(traps),
      .state_declared(st),
      .anchor_ok(an),
      .event_state_amp(amp),
      .urgency_only(urg),
      .ctrl(ag_ctrl)
  );

  cos_commit_iron_combo u_commit (
      .ctrl(c_ctrl),
      .owner_ok(c_owner),
      .context_tag(c_ctx),
      .ignored_mask(c_ign),
      .require_falsifier(c_reqf),
      .model_lane(c_model),
      .commit_allowed(c_allow)
  );

  cos_boundary_sync u_sync (
      .clk(clk),
      .rst_n(rst_n),
      .d(raw_bit),
      .q(syn_q)
  );

  task c(input int x);
    begin
      if (!x)
        $fatal(1, "assert failed");
    end
  endtask

  initial clk = 1'b0;
  always #5 clk = ~clk;

  initial begin
    rst_n = 1'b1;
    // formal: same depth
    fd = 6'd2;
    td = 6'd2;
    bridge = 1'b0;
    att = 6'h00;
    sim = 1'b0;
    ctok = 1'b0;
    sem = 1'b0;
    #1;
    c(f_ok == 1);
    c(f_bad == 0);

    // skip +2
    td = 6'd4;
    bridge = 1'b1;
    #1;
    c(f_ok == 0);

    // agency: clean → execute
    traps = 7'd0;
    st = 1'b1;
    an = 1'b1;
    amp = 1'b0;
    urg = 1'b0;
    #1;
    c(ag_ctrl == 2'h0);

    // two traps → stabilize
    traps = 7'h03;
    #1;
    c(ag_ctrl == 2'h2);

    // commit: execute + owner + cil + falsifier
    c_ctrl = 2'h0;
    c_owner = 1'b1;
    c_ctx = 8'h03;
    c_ign = 8'h01;
    c_reqf = 1'b1;
    c_model = 1'b0;
    #1;
    c(c_allow == 1);

    c_model = 1'b1;
    #1;
    c(c_allow == 0);

    // synchronizer (2-flop; async → sampled domain)
    rst_n = 1'b0;
    raw_bit = 1'b0;
    repeat (2)
      @(posedge clk);
    rst_n = 1'b1;
    raw_bit = 1'b1;
    repeat (2)
      @(posedge clk);
    c(syn_q == 1'b1);
    raw_bit = 1'b0;
    repeat (3)
      @(posedge clk);
    c(syn_q == 1'b0);

    $display("cos_silicon_chip_tb: ALL PASS");
    $finish(0);
  end
endmodule
