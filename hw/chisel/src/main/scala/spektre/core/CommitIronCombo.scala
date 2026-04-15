// SPDX-License-Identifier: AGPL-3.0-or-later
package spektre.core

import chisel3._

/** Commit gate — parity with rtl/cos_commit_iron_combo.sv */
class CommitIronCombo extends Module {
  val io = IO(new Bundle {
    val ctrl = Input(UInt(2.W))
    val ownerOk = Input(Bool())
    val contextTag = Input(UInt(8.W))
    val ignoredMask = Input(UInt(8.W))
    val requireFalsifier = Input(Bool())
    val modelLane = Input(Bool())
    val commitAllowed = Output(Bool())
  })

  val cil = io.contextTag.orR && io.ignoredMask.orR
  val falOk = !io.requireFalsifier || io.contextTag(0)
  val exec = io.ctrl === 0.U
  io.commitAllowed := exec && io.ownerOk && cil && falOk && !io.modelLane
}
