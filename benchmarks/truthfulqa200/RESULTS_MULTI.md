# TruthfulQA-200 — multi-model results

**Status:** template — populate after `run_multi.sh` +:

```bash
python3 benchmarks/truthfulqa200/grade.py multi_metrics \
  --results-a benchmarks/truthfulqa200/results_gemma3_4b.csv \
  --results-b benchmarks/truthfulqa200/results_phi4.csv \
  --label-a gemma3:4b --label-b phi4 \
  --output-md benchmarks/truthfulqa200/RESULTS_MULTI.md
```

If `phi4` does not fit on 8GB RAM, use `MODELS='gemma3:4b llama3.2:3b'` and document the substitution here.
