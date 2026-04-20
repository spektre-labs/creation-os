# v111.2 adaptive σ-RAG demo (fixture-based)

> **Demo, not a benchmark.**  Reports how a σ-gated RAG
> policy would behave on the pre-registered 50 % test
> split, using the Vovk–Gammerman conformal τ from the
> 50 % calibration split.  Numbers below are reproducible
> from existing sidecars with zero network I/O.

| task | signal routed to | τ (α=0.05) | n_test | answer-direct | retrieve | **retrievals saved** | acc on direct-only | baseline acc (always-direct) |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| `arc_challenge` | `sigma_product` | 0.1268 | 586 | 545 | 41 | **93.0%** | 0.462 | 0.454 |
| `arc_easy` | `entropy` | 0.2460 | 1188 | 1127 | 61 | **94.9%** | 0.749 | 0.747 |
| `truthfulqa_mc2` | `sigma_max_token` | 0.4153 | 409 | 365 | 44 | **89.2%** | 0.468 | 0.450 |
| `hellaswag` | `entropy` | 0.2875 | 373 | 355 | 18 | **95.2%** | 0.482 | 0.469 |

### Caveats

- **Numbers depend on BitNet-b1.58-2B-4T's calibration.**  A different base model (e.g. Llama-3-8B) would give a different conformal τ and a different retrievals-saved percentage on the same tasks.
- **Retrieve branch is assumed-oracle.**  The accuracy reported is the accuracy **on the abstained-from subset** (answered-direct only), not the end-to-end accuracy of a full RAG stack.  In a live deployment you must add a real retriever and measure the retrieval-augmented accuracy too.
- **Distribution shift matters.**  Per `docs/v111/CONFORMAL_GUARANTEE.md`, the ≥1-α coverage bound holds only on exchangeable draws from the calibration distribution.

### Reproduce

```bash
.venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --lock      # once
.venv-bitnet/bin/python benchmarks/v111/preregister_adaptive.py --analyse   # test-split matrix + τ
.venv-bitnet/bin/python benchmarks/v111/adaptive_rag_demo.py                # this table
```
