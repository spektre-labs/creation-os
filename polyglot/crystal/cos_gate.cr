# SPDX-License-Identifier: AGPL-3.0-or-later
# crystal run crystal/cos_gate.cr
root = ENV.fetch("CREATION_OS_ROOT", ".")
st = Process.run("make", ["merge-gate"], chdir: root, output: Process::Redirect::Inherit, error: Process::Redirect::Inherit)
exit st.success? ? 0 : (st.exit_code || 1)
