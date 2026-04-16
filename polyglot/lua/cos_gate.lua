-- SPDX-License-Identifier: AGPL-3.0-or-later
-- lua lua/cos_gate.lua
local root = os.getenv("CREATION_OS_ROOT") or "."
local cmd = string.format('cd "%s" && exec make merge-gate', root:gsub('"', '\\"'))
local r = os.execute(cmd)
if r == true or r == 0 then os.exit(0) end
os.exit(1)
