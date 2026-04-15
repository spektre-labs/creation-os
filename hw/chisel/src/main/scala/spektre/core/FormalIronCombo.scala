// SPDX-License-Identifier: AGPL-3.0-or-later
package spektre.core

import chisel3._

/** Combinational formal gate — parity with rtl/cos_formal_iron_combo.sv */
class FormalIronCombo(val w: Int = 6) extends Module {
  val io = IO(new Bundle {
    val fromDepth = Input(UInt(w.W))
    val toDepth = Input(UInt(w.W))
    val bridgeOk = Input(Bool())
    val attemptedLimits = Input(UInt(6.W))
    val simLane = Input(Bool())
    val commitToken = Input(Bool())
    val semanticClaim = Input(Bool())
    val stackStepOk = Output(Bool())
    val limitsBad = Output(Bool())
    val simCommitForbidden = Output(Bool())
    val levelTag = Output(UInt(6.W))
  })

  val retreatOrSame = io.toDepth <= io.fromDepth
  val delta = io.toDepth - io.fromDepth
  val advanceOne = delta === 1.U
  io.stackStepOk := retreatOrSame || (advanceOne && io.bridgeOk)

  val forbiddenMask = "h3f".U(6.W)
  io.limitsBad := (io.attemptedLimits & forbiddenMask).orR || io.semanticClaim
  io.simCommitForbidden := io.simLane && io.commitToken

  val fd = io.fromDepth
  io.levelTag := Mux(
    fd === 0.U,
    0.U(6.W),
    Mux(
      fd === 1.U,
      1.U(6.W),
      Mux(
        fd === 2.U,
        2.U(6.W),
        Mux(
          fd === 3.U,
          3.U(6.W),
          Mux(
            fd < 32.U,
            (4.U(6.W) + (fd - 4.U(6.W))).asTypeOf(UInt(6.W)),
            (4.U(6.W) + (32.U(6.W) - 5.U(6.W))).asTypeOf(UInt(6.W))
          )
        )
      )
    )
  )
}
