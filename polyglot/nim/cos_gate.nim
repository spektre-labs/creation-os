# SPDX-License-Identifier: AGPL-3.0-or-later
# nim c -r nim/cos_gate.nim
import os
let root = getEnv("CREATION_OS_ROOT", ".")
quit execShellCmd("cd " & root & " && exec make merge-gate")
