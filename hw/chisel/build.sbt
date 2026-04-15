// SPDX-License-Identifier: AGPL-3.0-or-later
ThisBuild / scalaVersion := "2.13.12"
ThisBuild / version := "0.1.0"
ThisBuild / organization := "labs.spektre"

/* Chisel 3.6.x: classic ChiselStage.emitVerilog (no host firtool required). */
val chisel3Version = "3.6.1"

lazy val root = (project in file("."))
  .settings(
    name := "spektre-core-chisel",
    libraryDependencies ++= Seq(
      "edu.berkeley.cs" %% "chisel3" % chisel3Version,
      "edu.berkeley.cs" %% "chiseltest" % "0.6.1" % Test
    ),
    scalacOptions ++= Seq(
      "-language:reflectiveCalls",
      "-deprecation",
      "-feature",
      "-Xcheckinit",
      "-Ymacro-annotations"
    ),
    addCompilerPlugin("edu.berkeley.cs" % "chisel3-plugin" % chisel3Version cross CrossVersion.full)
  )
