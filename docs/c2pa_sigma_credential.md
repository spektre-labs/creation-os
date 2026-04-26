# C2PA-oriented σ-credentials (`ai.creation-os.sigma`)

Creation OS can emit a **machine-readable JSON sidecar** next to model output. It is shaped for use as a **custom C2PA assertion** (`ai.creation-os.sigma`): provenance tooling can carry *where* content came from (C2PA ecosystem), while the sidecar carries *how uncertain / how gated* the answer was (σ, action, verification method, receipt hashes).

This is **not** a C2PA manifest signer in-tree. Signing stays with your C2PA toolchain; we provide the assertion payload and CLI helpers.

## Commands

- `cos stamp --file PATH [--sigma F | --prompt TEXT]` → writes `PATH.cos.json` (see `make cos-stamp`).
- `cos validate PATH.cos.json` → schema-oriented checks (label, σ range, receipt hash hex).
- `cos chat ... --stamp [OUT.json]` → after each turn, writes the sidecar (default `cos-chat-turn.cos.json`).

## Evidence discipline

Numeric quality claims (AUROC, proof counts, harness scores) belong in **measured** bundles per `docs/CLAIM_DISCIPLINE.md`. The JSON `proofs.repository` block lists **in-repo harness names** only, not certification outcomes.

## EU AI Act

Article **50** (transparency for certain AI-generated content) expects **detectable, machine-readable** signals. A `*.cos.json` sidecar is one way to expose σ-gate metadata alongside human-readable chat output. Map other articles via `cos compliance` / `cos_eu_check()`.

## Python bridge

`tools/c2pa_integration.py` loads a sidecar and, if the optional `c2pa` package is installed, can attach the same object as a custom assertion. Otherwise it prints a hint and does nothing.
