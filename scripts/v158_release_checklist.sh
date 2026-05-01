#!/usr/bin/env bash
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
# v158 σ-v1.0 — release-checklist validator.
#
# Parses docs/RELEASE_v1_0.md and asserts every engineering (A),
# documentation (B), and packaging (C) item is either ticked or
# backed by a present artefact. Release-day (D) and communication
# (E) items are intentionally left unchecked in-tree; they are
# human-driven actions the BDFL performs when publishing v1.0.0.
#
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
set -euo pipefail

FILE=docs/RELEASE_v1_0.md

die() { echo "v158 release-checklist: FAIL — $*" >&2; exit 1; }
ok()  { echo "  OK  $*"; }

echo "v158 release-checklist: validating $FILE"
[ -f "$FILE" ] || die "missing $FILE"

# Required sections.
grep -q "^## Release coordinates"        "$FILE" || die "Release coordinates missing"
grep -q "^## Release checklist"          "$FILE" || die "Release checklist missing"
grep -q "^### A\. Engineering gate"      "$FILE" || die "A. Engineering gate missing"
grep -q "^### B\. Documentation gate"    "$FILE" || die "B. Documentation gate missing"
grep -q "^### C\. Packaging gate"        "$FILE" || die "C. Packaging gate missing"
grep -q "^### D\. Release-day actions"   "$FILE" || die "D. Release-day actions missing"
grep -q "^### E\. Communication"         "$FILE" || die "E. Communication missing"
grep -q "^## Release message"            "$FILE" || die "Release message missing"
ok "all 8 required sections present"

# Release coordinates
grep -q 'v1\.0\.0'                    "$FILE" || die "tag v1.0.0 missing"
grep -q 'release/v1\.0\.0'            "$FILE" || die "branch release/v1.0.0 missing"
grep -q 'docs/papers/creation_os_v1\.md' "$FILE" || die "paper link missing"
grep -q 'SCSL-1\.0'                   "$FILE" || die "SCSL-1.0 license missing"
grep -q 'AGPL-3\.0-only'              "$FILE" || die "AGPL-3.0-only license missing"
ok "release coordinates complete"

# A block — all 7 items must be [x]-ticked in tree.
for a in A1 A2 A3 A4 A5 A6 A7; do
    grep -q "^- \[x\] $a\\." "$FILE" \
        || die "Engineering gate item $a is not ticked"
done
ok "A1..A7 (engineering gate) all ticked"

# B block — B1..B6 must be ticked.
for b in B1 B2 B3 B4 B5 B6; do
    grep -q "^- \[x\] $b\\." "$FILE" \
        || die "Documentation gate item $b is not ticked"
done
ok "B1..B6 (documentation gate) all ticked"

# C block — C1..C5 must be ticked.
for c in C1 C2 C3 C4 C5; do
    grep -q "^- \[x\] $c\\." "$FILE" \
        || die "Packaging gate item $c is not ticked"
done
ok "C1..C5 (packaging gate) all ticked"

# D and E blocks — the BDFL-driven sections — must be enumerated
# (each expected id appears as a bullet) but must NOT be ticked
# in-tree. If anyone ticks them without running the human action,
# this gate flags it.
for d in D1 D2 D3 D4 D5 D6 D7 D8 D9 D10; do
    grep -q "^- \[.\] $d\\." "$FILE" \
        || die "Release-day item $d missing"
    if grep -q "^- \[x\] $d\\." "$FILE"; then
        die "Release-day $d must not be ticked in-tree (BDFL-driven)"
    fi
done
ok "D1..D10 (release-day) enumerated and unticked"

for e in E1 E2 E3 E4 E5 E6; do
    grep -q "^- \[.\] $e\\." "$FILE" \
        || die "Communication item $e missing"
    if grep -q "^- \[x\] $e\\." "$FILE"; then
        die "Communication $e must not be ticked in-tree (BDFL-driven)"
    fi
done
ok "E1..E6 (communication) enumerated and unticked"

# Back-link cross-check: every artefact promised by B + C exists.
[ -f docs/papers/creation_os_v1.md ] || die "B1: paper missing"
grep -q "^## v1\\.0\\.0" CHANGELOG.md || die "B2: CHANGELOG.md lacks v1.0.0 section"
[ -f GOVERNANCE.md ]                    || die "B4: GOVERNANCE.md missing"
[ -f docs/GOOD_FIRST_ISSUES.md ]        || die "B5: GOOD_FIRST_ISSUES missing"
for d in v154 v155 v156 v157 v158; do
    [ -f "docs/$d/README.md" ] \
        || die "B6: docs/$d/README.md missing"
done
ok "B1..B6 artefacts present on disk"

[ -f python/pyproject.toml ]                           || die "C1: pyproject.toml missing"
[ -f packaging/brew/creation-os.rb ]                   || die "C2: brew formula missing"
[ -f packaging/docker/Dockerfile.release ]             || die "C3: release Dockerfile missing"
for c in creation-os-benchmark.md creation-os-corpus-qa.md bitnet-2b-sigma-lora.md; do
    [ -f "packaging/huggingface/$c" ] || die "C4: hf card $c missing"
done
[ -f packaging/npm/package.json ]                      || die "C5: npm package.json missing"
[ -f packaging/npm/bin/creation-os.js ]                || die "C5: npm launcher missing"
ok "C1..C5 artefacts present on disk"

# Release message sanity.
grep -q 'pip install creation-os'       "$FILE" || die "release message missing `pip install`"
grep -q '153 kernels\.'                 "$FILE" || die "release message missing `153 kernels.`"
grep -q 'github\.com/spektre-labs/creation-os' "$FILE" \
    || die "release message missing canonical URL"
ok "canonical release message intact"

echo "v158 release-checklist: OK"
