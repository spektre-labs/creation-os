#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# v157 σ-Community — contribution-infrastructure linter.
#
# Asserts every required community file is present and carries the
# canonical sections. No network, no external tools beyond coreutils.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

die() { echo "v157 lint: FAIL — $*" >&2; exit 1; }

ok() { echo "  OK  $*"; }

echo "v157 contributing lint: validating community infrastructure"

# 1) CONTRIBUTING.md must exist and name merge-gate + CLA + language policy.
[ -f CONTRIBUTING.md ]                    || die "CONTRIBUTING.md missing"
grep -q "merge-gate"        CONTRIBUTING.md || die "CONTRIBUTING.md must mention merge-gate"
grep -q "CLA"               CONTRIBUTING.md || die "CONTRIBUTING.md must mention CLA"
grep -q "LANGUAGE_POLICY"   CONTRIBUTING.md || die "CONTRIBUTING.md must link LANGUAGE_POLICY"
ok "CONTRIBUTING.md"

# 2) GOVERNANCE.md must exist and name BDFL + merge requirements + 1=1.
[ -f GOVERNANCE.md ]                     || die "GOVERNANCE.md missing"
grep -q "BDFL"              GOVERNANCE.md || die "GOVERNANCE.md must name the BDFL role"
grep -q "Merge requirements" GOVERNANCE.md \
  || die "GOVERNANCE.md must declare merge requirements"
grep -q "1 = 1"             GOVERNANCE.md || die "GOVERNANCE.md must state the 1 = 1 invariant"
grep -q "CLA.md"            GOVERNANCE.md || die "GOVERNANCE.md must link CLA.md"
ok "GOVERNANCE.md"

# 3) CLA.md must exist.
[ -f CLA.md ]                             || die "CLA.md missing"
ok "CLA.md"

# 4) Issue templates.
for t in bug_report.md config.yml documentation.md \
         feature_request.md kernel_proposal.md benchmark_submission.md; do
    [ -f ".github/ISSUE_TEMPLATE/$t" ] \
        || die ".github/ISSUE_TEMPLATE/$t missing"
done
ok ".github/ISSUE_TEMPLATE/{bug,feature,kernel,benchmark,documentation}.md"

grep -q "^name:" .github/ISSUE_TEMPLATE/feature_request.md \
    || die "feature_request.md missing YAML front-matter name"
grep -q "σ-contract"   .github/ISSUE_TEMPLATE/feature_request.md \
    || die "feature_request.md must prompt for σ-contract"
grep -q "Merge-gate"   .github/ISSUE_TEMPLATE/feature_request.md \
    || die "feature_request.md must prompt for merge-gate"
grep -q "v0 vs v1 split" .github/ISSUE_TEMPLATE/feature_request.md \
    || die "feature_request.md must prompt for v0/v1 split"

grep -q "^name:"       .github/ISSUE_TEMPLATE/kernel_proposal.md \
    || die "kernel_proposal.md missing YAML front-matter"
grep -q "σ-contract"   .github/ISSUE_TEMPLATE/kernel_proposal.md \
    || die "kernel_proposal.md must ask for σ-contract"

grep -q "tier-0\|tier-1\|tier-2" .github/ISSUE_TEMPLATE/benchmark_submission.md \
    || die "benchmark_submission.md must reference tier-0/1/2"

# 5) Pull request template.
[ -f .github/PULL_REQUEST_TEMPLATE.md ] \
    || die ".github/PULL_REQUEST_TEMPLATE.md missing"
ok ".github/PULL_REQUEST_TEMPLATE.md"

# 6) Good first issues.
[ -f docs/GOOD_FIRST_ISSUES.md ] \
    || die "docs/GOOD_FIRST_ISSUES.md missing"
grep -q "kurtosis"       docs/GOOD_FIRST_ISSUES.md || die "kurtosis issue absent"
grep -q "WebAssembly"    docs/GOOD_FIRST_ISSUES.md || die "wasm issue absent"
grep -q "Phi-3"          docs/GOOD_FIRST_ISSUES.md || die "Phi-3 issue absent"
grep -q "Finnish"        docs/GOOD_FIRST_ISSUES.md || die "Finnish-locale issue absent"
ok "docs/GOOD_FIRST_ISSUES.md"

# 7) Security contact.
[ -f SECURITY.md ] || die "SECURITY.md missing"
ok "SECURITY.md"

echo "v157 contributing lint: OK"
