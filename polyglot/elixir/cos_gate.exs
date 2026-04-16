# SPDX-License-Identifier: AGPL-3.0-or-later
# mix run not required — elixir cos_gate.exs
root = System.get_env("CREATION_OS_ROOT") || "."
{_, code} = System.cmd("make", ["merge-gate"], cd: root, stderr_to_stdout: true)
System.halt(code)
