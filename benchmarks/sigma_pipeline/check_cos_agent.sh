#!/usr/bin/env bash
# cos-agent smoke test (A6): dry-run + execute across task matrix.
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./cos-agent
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

# 1) list-and-count: 2-step plan, OK, two ALLOW decisions.
OUT1="$("$BIN" --json "list-and-count:.")"; echo "  · OK: $OUT1"
grep -q '"status":"OK"'            <<<"$OUT1" || { echo "s1 status" >&2; exit 3; }
grep -q '"n_steps":2'              <<<"$OUT1" || { echo "s1 steps"  >&2; exit 4; }
grep -q '"tool":"file_read"'       <<<"$OUT1" || { echo "s1 read"   >&2; exit 5; }
grep -q '"tool":"calculator"'      <<<"$OUT1" || { echo "s1 calc"   >&2; exit 6; }
grep -q '"decision":"ALLOW"'       <<<"$OUT1" || { echo "s1 allow"  >&2; exit 7; }

# 2) fail task: 1-step plan, ABORTED, rc=-1, done=false.
OUT2="$("$BIN" --json "fail:bad" || true)"; echo "  · FAIL: $OUT2"
grep -q '"status":"ABORTED"'       <<<"$OUT2" || { echo "s2 aborted" >&2; exit 8; }
grep -q '"rc":-1'                  <<<"$OUT2" || { echo "s2 rc"      >&2; exit 9; }
grep -q '"done":false'             <<<"$OUT2" || { echo "s2 done"    >&2; exit 10; }

# 3) noisy task: REPLAN, σ_post=0.90, step still completed.
OUT3="$("$BIN" --json "noisy:x" || true)"; echo "  · REPLAN: $OUT3"
grep -q '"status":"REPLAN"'        <<<"$OUT3" || { echo "s3 replan"  >&2; exit 11; }
grep -q '"sigma_post":0.9000'      <<<"$OUT3" || { echo "s3 sigma"   >&2; exit 12; }

# 4) rm without approve: IRREV force-downgrade → CONFIRM band at
#    base 0.80 is [0.85, 0.95) and effective is 0.76 → BLOCK.
OUT4="$("$BIN" --json "rm:/tmp/x" || true)"; echo "  · BLOCK:  $OUT4"
grep -q '"status":"BLOCKED"'       <<<"$OUT4" || { echo "s4 blocked"  >&2; exit 13; }
grep -q '"decision":"BLOCK"'       <<<"$OUT4" || { echo "s4 decision" >&2; exit 14; }

# 5) rm WITH approve-each: base autonomy = 1.0 → IRREV lands in
#    CONFIRM; confirmed=1 promotes CONFIRM → executor runs; OK.
OUT5="$("$BIN" --json --approve-each "rm:/tmp/x")"; echo "  · CONFIRM:$OUT5"
grep -q '"status":"OK"'            <<<"$OUT5" || { echo "s5 status"   >&2; exit 15; }
grep -q '"decision":"CONFIRM"'     <<<"$OUT5" || { echo "s5 decision" >&2; exit 16; }

# 6) fetch with --local-only: NETWORK exceeds max_risk=WRITE →
#    plan compile refuses → n_steps=0.
OUT6="$("$BIN" --json --local-only "fetch:http://x" || true)"; echo "  · LOCAL:  $OUT6"
grep -q '"n_steps":0'              <<<"$OUT6" || { echo "s6 n_steps" >&2; exit 17; }
grep -q '"compile_rc":-1'          <<<"$OUT6" || { echo "s6 compile" >&2; exit 18; }

# 7) unknown task: rc=3.
"$BIN" --json "dance" > /tmp/cos_agent_unknown.json 2>/dev/null && rc=0 || rc=$?
echo "  · UNKNOWN rc=$rc"
[[ $rc -eq 3 ]] || { echo "unknown rc != 3" >&2; exit 19; }

# 8) banner-only: emit config + exit 0.
OUT8="$("$BIN" --banner-only --local-only --max-steps 3 --max-cost 0.5 --approve-each)"
echo "  · BANNER: $OUT8"
grep -q '"banner":true'            <<<"$OUT8" || { echo "s8 banner"   >&2; exit 20; }
grep -q '"max_steps":3'            <<<"$OUT8" || { echo "s8 steps"    >&2; exit 21; }
grep -q '"max_cost_eur":0.5000'    <<<"$OUT8" || { echo "s8 cost"     >&2; exit 22; }
grep -q '"local_only":true'        <<<"$OUT8" || { echo "s8 local"    >&2; exit 23; }
grep -q '"approve_each":true'      <<<"$OUT8" || { echo "s8 approve"  >&2; exit 24; }

# 9) dry-run: compile + show plan, don't execute → sigma_post stays 1.0
#    (initial value; executor never ran).
OUT9="$("$BIN" --json --dry-run "list-and-count:.")"; echo "  · DRY:    $OUT9"
grep -q '"dry_run":true'           <<<"$OUT9" || { echo "s9 dry"      >&2; exit 25; }
grep -q '"sigma_post":1.0000'      <<<"$OUT9" || { echo "s9 sigma"    >&2; exit 26; }
grep -q '"status":"OK"'            <<<"$OUT9" || { echo "s9 status"   >&2; exit 27; }

echo "check-cos-agent: PASS (9 scenarios: compose plan + gate + execute + budget + flags)"
