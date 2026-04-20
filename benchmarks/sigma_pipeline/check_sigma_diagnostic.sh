#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."

BIN=./creation_os_sigma_diagnostic
[[ -x "$BIN" ]] || { echo "missing $BIN" >&2; exit 2; }

OUT="$("$BIN")"
echo "  · summary: $OUT"

grep -q '"self_test_rc":0' <<<"$OUT" || { echo "rc != 0"       >&2; exit 3; }
grep -q '"pass":true'      <<<"$OUT" || { echo "pass != true"  >&2; exit 4; }

# Demo: 4-token vocab, logprobs {0, 0, 0, -10} → three-way tie.
# Expected (computed analytically, see header docstring):
#   p ≈ (0.333, 0.333, 0.333, 1.5e-5)
#   H ≈ 1.0986 (ln 3)
#   factor_entropy = H / ln 4 ≈ 0.7925
#   factor_gap     ≈ 1.0
#   factor_maxprob ≈ 0.6667
#   σ              ≈ 0.8197
#   effective_k    = round(exp(1.0986)) = 3
#   counterfactual lifting top-1 to 0.90 → σ much lower.
grep -q '"vocab_size":4'      <<<"$OUT" || { echo "vocab_size"      >&2; exit 5; }
grep -q '"effective_k":3'     <<<"$OUT" || { echo "effective_k"     >&2; exit 6; }
grep -Eq '"factor_gap":(0.999[0-9]|1.0000)' <<<"$OUT" || { echo "factor_gap" >&2; exit 7; }
grep -Eq '"factor_maxprob":0.66[0-9][0-9]'  <<<"$OUT" || { echo "factor_maxprob" >&2; exit 8; }
grep -Eq '"factor_entropy":0.79[0-9][0-9]'  <<<"$OUT" || { echo "factor_entropy" >&2; exit 9; }
grep -Eq '"sigma":0.81[0-9][0-9]'           <<<"$OUT" || { echo "sigma"  >&2; exit 10; }
grep -Eq '"max_prob":0.333[0-9]'            <<<"$OUT" || { echo "max_prob" >&2; exit 11; }
grep -q '"counterfactual":{"target":0.9000,'<<<"$OUT" || { echo "cf target" >&2; exit 12; }

cf_sigma=$(sed -E 's/.*"counterfactual":\{[^}]*"sigma":([0-9.]+).*/\1/' <<<"$OUT")
awk "BEGIN{exit !($cf_sigma < 0.30)}" \
  || { echo "cf_sigma ($cf_sigma) ≥ 0.30 (counterfactual should collapse σ)" >&2; exit 13; }

echo "check-sigma-diagnostic: PASS (three-way tie σ ≈ 0.82 → counterfactual σ = $cf_sigma at p_top1=0.90)"
