// SPDX-License-Identifier: AGPL-3.0-or-later
package spektre.core

import chisel3._

/**
  * Top integration module: formal + agency + commit lanes.
  * Emits Verilog for Yosys / OpenROAD flows alongside rtl/*.sv hand-written blocks.
  */
class SpektreCore(val w: Int = 6) extends Module {
  val io = IO(new Bundle {
    val formalFromDepth = Input(UInt(w.W))
    val formalToDepth = Input(UInt(w.W))
    val formalBridgeOk = Input(Bool())
    val formalAttemptedLimits = Input(UInt(6.W))
    val formalSimLane = Input(Bool())
    val formalCommitToken = Input(Bool())
    val formalSemanticClaim = Input(Bool())
    val formalStackStepOk = Output(Bool())
    val formalLimitsBad = Output(Bool())
    val formalSimCommitForbidden = Output(Bool())
    val formalLevelTag = Output(UInt(6.W))

    val agencyTrapMask = Input(UInt(7.W))
    val agencyStateDeclared = Input(Bool())
    val agencyAnchorOk = Input(Bool())
    val agencyEventStateAmp = Input(Bool())
    val agencyUrgencyOnly = Input(Bool())
    val agencyCtrl = Output(UInt(2.W))

    val commitOwnerOk = Input(Bool())
    val commitContextTag = Input(UInt(8.W))
    val commitIgnoredMask = Input(UInt(8.W))
    val commitRequireFalsifier = Input(Bool())
    val commitModelLane = Input(Bool())
    val commitAllowed = Output(Bool())
  })

  val formal = Module(new FormalIronCombo(w))
  formal.io.fromDepth := io.formalFromDepth
  formal.io.toDepth := io.formalToDepth
  formal.io.bridgeOk := io.formalBridgeOk
  formal.io.attemptedLimits := io.formalAttemptedLimits
  formal.io.simLane := io.formalSimLane
  formal.io.commitToken := io.formalCommitToken
  formal.io.semanticClaim := io.formalSemanticClaim
  io.formalStackStepOk := formal.io.stackStepOk
  io.formalLimitsBad := formal.io.limitsBad
  io.formalSimCommitForbidden := formal.io.simCommitForbidden
  io.formalLevelTag := formal.io.levelTag

  val agency = Module(new AgencyIronCombo())
  agency.io.trapMask := io.agencyTrapMask
  agency.io.stateDeclared := io.agencyStateDeclared
  agency.io.anchorOk := io.agencyAnchorOk
  agency.io.eventStateAmp := io.agencyEventStateAmp
  agency.io.urgencyOnly := io.agencyUrgencyOnly
  io.agencyCtrl := agency.io.ctrl

  val commit = Module(new CommitIronCombo())
  commit.io.ctrl := agency.io.ctrl
  commit.io.ownerOk := io.commitOwnerOk
  commit.io.contextTag := io.commitContextTag
  commit.io.ignoredMask := io.commitIgnoredMask
  commit.io.requireFalsifier := io.commitRequireFalsifier
  commit.io.modelLane := io.commitModelLane
  io.commitAllowed := commit.io.commitAllowed
}
