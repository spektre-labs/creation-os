# v155 — σ-Publish · PyPI + Homebrew + Docker + Hugging Face + npm

**Validator:** [`scripts/v155_publish_check.py`](../../scripts/v155_publish_check.py) · **Make:** `check-v155`

## Problem

Shipping a 153-kernel, dual-license, σ-governed local AGI runtime
to five different package ecosystems is an assembly-line: each
target has its own manifest format, its own metadata schema, and
its own failure mode. The failure mode we protect against in v155
is **silent drift**: a rename in `python/pyproject.toml` that the
Homebrew formula forgets to mirror, a Docker tag that the HF
model card still names in its reproducibility block, a `bin`
entry in `package.json` that points at a launcher which no longer
exists on disk.

## σ-innovation

Every publish surface is validated **offline**, by a stdlib-only
Python script that parses the manifest, asserts the required
metadata fields, and cross-checks every back-link.

| Surface | Artefact | Primary contract |
|---|---|---|
| **PyPI** | [`python/pyproject.toml`](../../python/pyproject.toml) + [`python/creation_os/__init__.py`](../../python/creation_os/__init__.py) + [`packaging/pypi/PUBLISH.md`](../../packaging/pypi/PUBLISH.md) | `project.name == "creation-os"` · exports `class COS` · `PUBLISH.md` describes `twine upload` |
| **Homebrew** | [`packaging/brew/creation-os.rb`](../../packaging/brew/creation-os.rb) | `class CreationOs < Formula` · canonical homepage · `test do` block |
| **Docker** | [`packaging/docker/Dockerfile.release`](../../packaging/docker/Dockerfile.release) + top-level [`Dockerfile`](../../Dockerfile) | `FROM` + `EXPOSE 8080` + `creation_os_server` |
| **Hugging Face** | [`packaging/huggingface/creation-os-benchmark.md`](../../packaging/huggingface/creation-os-benchmark.md), [`creation-os-corpus-qa.md`](../../packaging/huggingface/creation-os-corpus-qa.md), [`bitnet-2b-sigma-lora.md`](../../packaging/huggingface/bitnet-2b-sigma-lora.md) | YAML front-matter · `license:` field · link back to `spektre-labs/creation-os` |
| **npm** | [`packaging/npm/package.json`](../../packaging/npm/package.json) + [`packaging/npm/bin/creation-os.js`](../../packaging/npm/bin/creation-os.js) | `name == "creation-os"` · `version == "1.0.0"` · `bin.creation-os` exists on disk with a shebang |

## Merge-gate

`make check-v155` runs `scripts/v155_publish_check.py` with no
network. Exit code 0 iff every row above passes. The script's
failures are keyed by contract id (`P1..P7, H1..H4, D1..D5, F1..F5,
N1..N7`) so a regression tells you exactly which manifest is out
of sync.

## v155.0 vs v155.1

* **v155.0** — offline manifests + offline validator. No network,
  no `twine`, no `brew`, no `docker`, no `hf`, no `npm`.
* **v155.1** — the actual publish path. Requires `$PYPI_API_TOKEN`,
  `$HF_TOKEN`, a signed Docker Hub login, and a tap PR on
  `homebrew-spektre-labs`. The order is fixed:
  1. `twine upload dist/*` from `python/`.
  2. `brew bump-formula-pr spektre-labs/tap/creation-os`.
  3. `docker buildx build -f packaging/docker/Dockerfile.release
     --platform linux/amd64,linux/arm64 --push` to
     `spektrelabs/creation-os:v1.0.0` + `:latest`.
  4. `hf upload` the three model cards.
  5. `npm publish` the launcher from `packaging/npm/`.

Every step in v155.1 is gated on v155.0 passing first (`make
check-v155`).

## Honest claims

* **v1.0.0 does NOT upload trained LoRA weights** to Hugging Face.
  The `bitnet-2b-sigma-lora` card is a *planned* v1.1 deliverable
  with the training contracts spelled out; the card itself is
  published at v1.0.0, the weights are not. This is disclosed in
  the card front-matter and in `docs/RELEASE_v1_0.md` (section D7).
* **The npm launcher ships no native binary.** On first invocation
  it is designed to fetch the platform-appropriate `cos` binary
  from the GitHub release. For v1.0.0 the launcher only prints an
  install hint; the fetch-and-cache logic lands in v1.1.
* **All five manifests are versioned together.** Any PR that bumps
  one without the others fails the merge-gate.
