# SPDX-License-Identifier: AGPL-3.0-or-later
# janet janet/cos_gate.janet
(def root (or (os/getenv "CREATION_OS_ROOT") "."))
(def sh (string "cd \"" root "\" && exec make merge-gate"))
(os/exit (os/shell sh))
