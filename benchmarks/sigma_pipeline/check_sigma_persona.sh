#!/usr/bin/env bash
#
# FINAL-2 smoke test: σ-Persona — adaptive user profile.
#
# Pinned facts (from persona_main.c transcript):
#   self_test_rc             == 0
#   turns                    == 6
#   language                 == "fi"                 (Finnish detected turn 4)
#   expertise_level          == "beginner"           ("too technical" fired)
#   last_feedback            == 6                    (COS_PERSONA_FB_TOO_CASUAL)
#   verbosity                ≈ 0.50 ± 0.1
#   formality                == 0.60                 (one "too casual" nudge)
#   domain "sigma" mentions  == 2                    (two queries)
#   domain "gate"  mentions  == 2
#   envelope contains "[persona]"  and the Finnish language tag
#
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN="./creation_os_sigma_persona"
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT1=$(mktemp); OUT2=$(mktemp)
trap 'rm -f "$OUT1" "$OUT2"' EXIT

"$BIN" > "$OUT1"
"$BIN" > "$OUT2"
diff -u "$OUT1" "$OUT2" >/dev/null || {
  echo "check_sigma_persona: non-deterministic output" >&2
  exit 3
}

python3 - "$OUT1" <<'PY'
import json, sys
with open(sys.argv[1]) as f:
    doc = json.load(f)

assert doc["kernel"] == "sigma_persona", doc
assert doc["self_test"] == 0, doc
assert doc["turns"] == 6, doc

env = doc["envelope"]
assert env.startswith("[persona]"), env
assert "language=fi" in env, env
assert "expertise=beginner" in env, env

prof = doc["profile"]
assert prof["language"] == "fi", prof
assert prof["expertise_level"] == "beginner", prof
assert prof["last_feedback"] == 6, prof  # COS_PERSONA_FB_TOO_CASUAL
assert 0.40 <= prof["verbosity"] <= 0.60, prof
assert 0.55 <= prof["formality"] <= 0.65, prof

# Domains are sorted by (mentions desc, name asc).  "sigma" and "gate"
# should lead the board, both with 2 mentions.
by_name = {d["domain"]: d for d in prof["domains"]}
assert by_name["sigma"]["mentions"] == 2, prof
assert by_name["gate"]["mentions"]  == 2, prof

# Weights are a valid normalised distribution (non-negative, ≤ 1.01 after fp).
ssum = 0.0
for d in prof["domains"]:
    assert d["weight"] >= 0.0, d
    ssum += d["weight"]
assert 0.99 <= ssum <= 1.01, ssum

# Top-2 (by sort) must carry the highest mention counts.
top_mentions = [prof["domains"][i]["mentions"] for i in (0, 1)]
assert top_mentions[0] == 2 and top_mentions[1] == 2, top_mentions

print("check_sigma_persona: PASS", {
    "verbosity": round(prof["verbosity"], 3),
    "formality": round(prof["formality"], 3),
    "top_domain": prof["domains"][0]["domain"],
})
PY
