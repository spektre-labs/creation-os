# HYPERVEC — swarm orbits in ℝⁿ (toy) + optional materialization

This is **not** a second kernel. It is a **stdlib-only** Python orchestrator that:

1. **Shards** flagship C sources into byte ranges.
2. Spawns a **process swarm** (one “agent” per shard).
3. Each agent emits a fixed-length **vector** derived deterministically from its shard (SHA-256 → floats).
4. The conductor **superposes** vectors (element-wise sum) and reports an **L2 energy** — a meaningless-but-deterministic scalar you can use as a cheap “hypervector fingerprint” before touching `make`.

## Orbital tech (web, 2025–2026)

These are real stacks worth watching if you want *actually* next-level compilation or parallelism beside our portable C gate:

| Stack | What it is | Link |
|-------|------------|------|
| **MoonBit** | AI-native / cloud-edge language, WASM + JS + native backends, tiny WASM, fast compile | [https://www.moonbitlang.com/](https://www.moonbitlang.com/) · [https://docs.moonbitlang.com/en/latest/](https://docs.moonbitlang.com/en/latest/) |
| **Bend + HVM2** | Massively parallel high-level language; GPU-oriented runtimes; interaction nets | [https://github.com/HigherOrderCO/Bend](https://github.com/HigherOrderCO/Bend) · [https://github.com/HigherOrderCO/HVM2](https://github.com/HigherOrderCO/HVM2) |

Neither is vendored here — this folder stays **zero extra pip deps**.

## Run

From `creation-os/` (use **`-m`** so multiprocessing stays pickle-safe on macOS spawn builds):

```bash
python3 -m hypervec.swarm --agents 32 --dims 128
# After the swarm, run the real gate once:
HYPERVEC_MATERIALIZE=1 python3 -m hypervec.swarm --agents 16 --dims 64
```

Or via Just: `just hypervec-swarm` / `just hypervec-materialize`.

## Contract

- **Make remains law.** Swarm math is side-channel telemetry.
- If `HYPERVEC_MATERIALIZE=1`, we still invoke **`make merge-gate` exactly once** at the end (same bar as always).
