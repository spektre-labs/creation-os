#!/usr/bin/env bash
#
# OMEGA-5 smoke test: σ-arxiv submission manifest.
#
# Pinned facts (per arxiv_main.c + arxiv.c):
#   self_test == 0
#   metadata.title starts with "Creation OS"
#   metadata.orcid == "0009-0006-0903-8541"
#   metadata.category == "cs.LG"
#   metadata.affiliation == "Spektre Labs, Helsinki"
#   metadata.license contains "SCSL-1.0"
#   anchors_present == true
#   anchors contains: docs/papers/creation_os_v1.md,
#                    docs/papers/creation_os_v1.tex,
#                    CHANGELOG.md, include/cos_version.h, LICENSE
#   Each anchor.sha256 is 64 lowercase hex chars and bytes > 0.
#   Cross-check: running `sha256sum` (or shasum -a 256) on each
#   anchor path yields the same hex reported by the kernel.
#   Two invocations produce byte-identical output.
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_arxiv"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_arxiv: non-deterministic output" >&2
  exit 3
}

# Pick the right SHA-256 utility (Linux: sha256sum; macOS: shasum -a 256).
if command -v sha256sum >/dev/null 2>&1; then
  SHA256="sha256sum"
elif command -v shasum >/dev/null 2>&1; then
  SHA256="shasum -a 256"
else
  echo "check_sigma_arxiv: no sha256 utility available" >&2
  exit 4
fi

python3 - "$OUT1" "$SHA256" <<'PY'
import json, re, subprocess, sys

path = sys.argv[1]
sha_cmd = sys.argv[2].split()
with open(path) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_arxiv", doc
assert doc["self_test"] == 0, doc

md = doc["metadata"]
assert md["title"].startswith("Creation OS"), md
assert md["orcid"] == "0009-0006-0903-8541", md
assert md["category"] == "cs.LG", md
assert "Spektre Labs" in md["affiliation"], md
assert "SCSL-1.0" in md["license"], md
assert md["version_tag"].startswith("v2."), md
assert md["doi_reserved"].startswith("10."), md
assert len(md["abstract"]) > 80, md

assert doc["anchors_present"] is True, doc
anchors = {a["path"]: a for a in doc["anchors"]}
required = {
    "docs/papers/creation_os_v1.md",
    "docs/papers/creation_os_v1.tex",
    "CHANGELOG.md",
    "include/cos_version.h",
    "LICENSE",
}
missing = required - set(anchors)
assert not missing, f"missing anchors: {missing}"

hex_re = re.compile(r'^[0-9a-f]{64}$')
for a in doc["anchors"]:
    assert a["exists"] is True, a
    assert a["bytes"] > 0, a
    assert hex_re.match(a["sha256"]), a
    # Cross-check the digest with the system utility.
    out = subprocess.check_output(sha_cmd + [a["path"]]).decode()
    want = out.split()[0]
    assert want == a["sha256"], (a["path"], want, a["sha256"])

print("check_sigma_arxiv: PASS", {
    "anchors":    len(doc["anchors"]),
    "title":      md["title"][:40] + "…",
    "orcid":      md["orcid"],
    "category":   md["category"],
    "version":    md["version_tag"],
})
PY
