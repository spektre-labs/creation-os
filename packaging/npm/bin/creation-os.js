#!/usr/bin/env node
// v155 σ-Publish — npm launcher stub for `npx creation-os`.
//
// v1.0.0: prints a help blurb and points at the canonical install
// paths (brew / docker / git). v1.1.0: auto-fetches the native
// binary for the current platform from the GitHub release and
// caches it at ~/.creation-os/bin/cos.
//
// SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only

"use strict";

const { argv, platform, arch, exit } = process;

function help() {
  console.log(
`creation-os — local OpenAI-compatible runtime with σ-governance

USAGE
    npx creation-os --help
    npx creation-os --install-hint

The npm launcher ships no native binary. For a real install pick one:
    brew install spektre-labs/tap/creation-os
    docker run --rm -p 8080:8080 spektrelabs/creation-os:v1.0.0
    git clone https://github.com/spektre-labs/creation-os && cd creation-os && make merge-gate

Platform : ${platform}
Arch     : ${arch}
Repo     : https://github.com/spektre-labs/creation-os`);
}

const cmd = argv[2] || "--help";
if (cmd === "--help" || cmd === "-h" || cmd === "help") {
  help();
  exit(0);
} else if (cmd === "--install-hint") {
  console.log("brew install spektre-labs/tap/creation-os");
  exit(0);
} else {
  help();
  exit(2);
}
