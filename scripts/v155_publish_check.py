#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
# SPDX-Copyright-Identifier: 2024-2026 Lauri Elias Rainio · Spektre Labs Oy
# Source:        https://github.com/spektre-labs/creation-os-kernel
# Website:       https://spektrelabs.org
# Commercial:    spektre.labs@proton.me
# License docs:  LICENSE · LICENSE-SCSL-1.0.md · LICENSE-AGPL-3.0.txt
"""
v155 σ-Publish — offline packaging-manifest validator.

Runs no network. Uses only the standard library. Verifies that every
publish surface named in `docs/v155/README.md` exists, parses, and
carries the minimum metadata needed for `make check-v155` to be a
truthful pre-flight for the real v155.1 upload path:

    PyPI       python/pyproject.toml          project.name = "creation-os"
    Homebrew   packaging/brew/creation-os.rb  class CreationOs
    Docker     packaging/docker/Dockerfile.release   FROM + EXPOSE
    Docker-src Dockerfile                     top-level build recipe
    HF         packaging/huggingface/*.md     front-matter license
    npm        packaging/npm/package.json     bin.creation-os = *.js

Exit code 0 on all contracts pass, 1 otherwise.

SPDX-License-Identifier: LicenseRef-SCSL-1.0 OR AGPL-3.0-only
"""
from __future__ import annotations

import json
import os
import re
import sys
from pathlib import Path

try:
    import tomllib  # py311+
except ModuleNotFoundError:  # pragma: no cover
    import tomli as tomllib  # type: ignore

ROOT = Path(__file__).resolve().parent.parent
FAILURES: list[str] = []


def fail(code: str, msg: str) -> None:
    FAILURES.append(f"  [{code}] {msg}")


def must_exist(p: Path, code: str) -> bool:
    if not p.exists():
        fail(code, f"missing: {p.relative_to(ROOT)}")
        return False
    return True


def check_pypi() -> None:
    py = ROOT / "python" / "pyproject.toml"
    if not must_exist(py, "P1"):
        return
    data = tomllib.loads(py.read_text(encoding="utf-8"))
    proj = data.get("project") or {}
    if proj.get("name") != "creation-os":
        fail("P2", f"project.name != 'creation-os' (got {proj.get('name')!r})")
    if not proj.get("version"):
        fail("P3", "project.version missing")
    init = ROOT / "python" / "creation_os" / "__init__.py"
    if not must_exist(init, "P4"):
        return
    text = init.read_text(encoding="utf-8")
    if "class COS" not in text:
        fail("P5", "python/creation_os/__init__.py must export COS class")
    recipe = ROOT / "packaging" / "pypi" / "PUBLISH.md"
    if must_exist(recipe, "P6"):
        content = recipe.read_text(encoding="utf-8")
        if "twine upload" not in content:
            fail("P7", "PUBLISH.md must describe `twine upload`")


def check_homebrew() -> None:
    formula = ROOT / "packaging" / "brew" / "creation-os.rb"
    if not must_exist(formula, "H1"):
        return
    text = formula.read_text(encoding="utf-8")
    if not re.search(r"^class CreationOs < Formula", text, re.MULTILINE):
        fail("H2", "Homebrew formula must declare `class CreationOs < Formula`")
    if 'homepage "https://github.com/spektre-labs/creation-os"' not in text:
        fail("H3", "homepage must point to spektre-labs/creation-os")
    if "test do" not in text:
        fail("H4", "formula must ship a `test do` block")


def check_docker() -> None:
    d = ROOT / "packaging" / "docker" / "Dockerfile.release"
    if not must_exist(d, "D1"):
        return
    text = d.read_text(encoding="utf-8")
    if "FROM " not in text:
        fail("D2", "Dockerfile.release missing FROM directive")
    if "EXPOSE 8080" not in text:
        fail("D3", "Dockerfile.release must EXPOSE 8080")
    if "creation_os_server" not in text:
        fail("D4", "Dockerfile.release must ship creation_os_server")
    top = ROOT / "Dockerfile"
    if not must_exist(top, "D5"):
        return


def check_huggingface() -> None:
    hf = ROOT / "packaging" / "huggingface"
    if not must_exist(hf, "F1"):
        return
    expected = {
        "creation-os-benchmark.md",
        "creation-os-corpus-qa.md",
        "bitnet-2b-sigma-lora.md",
    }
    present = {p.name for p in hf.glob("*.md")}
    missing = expected - present
    if missing:
        fail("F2", f"missing HF cards: {sorted(missing)}")
    for name in expected & present:
        text = (hf / name).read_text(encoding="utf-8")
        if not text.startswith("---"):
            fail("F3", f"{name}: missing YAML front-matter")
        if "license:" not in text:
            fail("F4", f"{name}: front-matter must declare license")
        if "spektre-labs/creation-os" not in text:
            fail("F5", f"{name}: must link back to spektre-labs/creation-os")


def check_npm() -> None:
    pj = ROOT / "packaging" / "npm" / "package.json"
    if not must_exist(pj, "N1"):
        return
    try:
        data = json.loads(pj.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        fail("N2", f"package.json failed to parse: {exc}")
        return
    if data.get("name") != "creation-os":
        fail("N3", f"package.json name != 'creation-os' (got {data.get('name')!r})")
    bins = data.get("bin") or {}
    launcher = bins.get("creation-os")
    if not launcher:
        fail("N4", "package.json bin.creation-os missing")
    else:
        lp = ROOT / "packaging" / "npm" / launcher.lstrip("./")
        if not lp.exists():
            fail("N5", f"npm launcher not found at {lp.relative_to(ROOT)}")
        else:
            head = lp.read_text(encoding="utf-8").splitlines()[0] if lp.stat().st_size else ""
            if not head.startswith("#!"):
                fail("N6", f"{lp.relative_to(ROOT)} missing shebang")
    if data.get("version") != "1.0.0":
        fail("N7", f"package.json version != '1.0.0' (got {data.get('version')!r})")


def main() -> int:
    print("v155 publish-check: validating packaging manifests (offline)")
    check_pypi()
    check_homebrew()
    check_docker()
    check_huggingface()
    check_npm()

    if FAILURES:
        print("\nv155 publish-check: FAIL")
        for line in FAILURES:
            print(line)
        return 1
    print("  PyPI       OK  (python/pyproject.toml)")
    print("  Homebrew   OK  (packaging/brew/creation-os.rb)")
    print("  Docker     OK  (packaging/docker/Dockerfile.release + Dockerfile)")
    print("  HuggingFace OK (3 model cards)")
    print("  npm        OK  (packaging/npm/package.json + launcher)")
    print("v155 publish-check: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
