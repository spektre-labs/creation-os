// SPDX-License-Identifier: AGPL-3.0-or-later
package spektre.core

import chisel3.stage.ChiselStage

/** sbt "runMain spektre.core.GenVerilog" — emits SpektreCore.v (and firrtl) under target dir */
object GenVerilog extends App {
  val dir = Option(System.getProperty("chiselTargetDir")).getOrElse("generated")
  (new ChiselStage).emitVerilog(new SpektreCore(), Array("--target-dir", dir))
}
