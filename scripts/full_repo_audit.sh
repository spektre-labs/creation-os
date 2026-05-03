#!/usr/bin/env bash
# Full repo audit — excludes venv/third_party so C/H wc finishes.
set +e
cd "$(dirname "$0")/.." || exit 1
OUT="${1:-FULL_REPO_AUDIT.txt}"
exec > >(tee "$OUT") 2>&1

# BSD/GNU find: exclude heavy trees from *.c/*.h enumeration and wc
_find_ch() {
  find . -type f \( -name "*.c" -o -name "*.h" \) \
    ! -path "./.git/*" \
    ! -path "./node_modules/*" \
    ! -path "./third_party/*" \
    ! -path "./.sigma_pipeline_venv/*" \
    ! -path "./.build/*" \
    ! -path "./.venv/*" \
    ! -path "./.venv-*/*" \
    ! -path "./.tmp_*/*" \
    ! -path "*/.venv/*" \
    ! -path "*/site-packages/*" \
    ! -path "./.ruff_cache/*" \
    ! -path "./.pytest_cache/*" \
    ! -path "*/__pycache__/*" 2>/dev/null
}

echo "=========================================="
echo "CREATION OS — FULL REPO AUDIT"
echo "=========================================="

echo "=== 1. REPO ROOT ==="
ls -la

echo "=== 2. GIT ==="
git status --short | head -100
git log --oneline -20
git branch -a

echo "=== 3. PUU ==="
find . -maxdepth 3 -type f \
  ! -path "./.git/*" \
  ! -path "./node_modules/*" \
  ! -path "./.venv/*" \
  ! -path "./.venv-*/*" \
  ! -path "*/.venv/*" \
  ! -path "./.sigma_pipeline_venv/*" \
  ! -path "*/__pycache__/*" \
  ! -path "./third_party/*" 2>/dev/null | sort

echo "=== 4. PYTHON MODUULIT + LUOKAT + METODIT ==="
for f in $(find python/cos -name "*.py" | sort); do
  echo "--- $f ---"
  head -5 "$f"
  grep -n "^class \|^    def \|^def " "$f" || true
  echo ""
done

echo "=== 5. TESTIT + FUNKTIOT ==="
for f in $(find tests -name "*.py" | sort); do
  echo "--- $f ---"
  grep -n "^def test_\|^class Test" "$f" || true
done

echo "=== 6. C-TIEDOSTOT ==="
_find_ch | sort
echo "--- sigma_gate.h (python/cos) ---"
cat python/cos/sigma_gate.h 2>/dev/null || true
echo "--- sigma_gate.h (src/v49) ---"
cat src/v49/sigma_gate.h 2>/dev/null || true
echo "--- creation_os_v2.c (head 80) ---"
head -80 creation_os_v2.c 2>/dev/null

echo "=== 7. RIVIMÄÄRÄT ==="
echo "C:"
_find_ch | xargs wc -l 2>/dev/null | tail -1
echo "Python:"
find python/cos -name "*.py" | xargs wc -l 2>/dev/null | tail -1
echo "Tests:"
find tests -name "*.py" | xargs wc -l 2>/dev/null | tail -1

echo "=== 8. KONFIG ==="
cat pyproject.toml
echo "---"
cat AGENTS.md 2>/dev/null
echo "---"
head -100 Makefile

echo "=== 9. MAKEFILE KAIKKI TARGETIT ==="
grep "^[a-zA-Z_-]*:" Makefile

echo "=== 10. CLI ==="
cat python/cos/cli.py

echo "=== 11. __init__.py ==="
cat python/cos/__init__.py

echo "=== 12. sigma_gate.py ==="
cat python/cos/sigma_gate.py

echo "=== 13. STUBIT ==="
grep -rn "pass$" python/cos/ 2>/dev/null | head -200
grep -rn "return 0\.5\|placeholder\|# TODO\|# stub\|NotImplemented" python/cos/ 2>/dev/null | head -200

echo "=== 14. IMPORTIT MODUULIEN VÄLILLÄ ==="
grep -rn "^from cos\.\|^import cos\." python/cos/ 2>/dev/null | sort

echo "=== 15. THRESHOLDIT JA CONFIG-ARVOT ==="
grep -rn "threshold\|tau_accept\|tau_abstain" python/cos/ 2>/dev/null | grep -v __pycache__ | head -100

echo "=== 16. SERVE ENDPOINTS ==="
grep -rn "@app\.\|@router\." python/cos/ 2>/dev/null | head -50

echo "=== 17. IMPORT TESTI ==="
python3 -c "
import importlib, pathlib, sys
sys.path.insert(0, 'python')
ok = 0; fails = []
for f in sorted(pathlib.Path('python/cos').rglob('*.py')):
    if '__pycache__' in str(f): continue
    mod = str(f).replace('python/','').replace('/','.').replace('.py','')
    try: importlib.import_module(mod); ok += 1
    except Exception as e: fails.append(f'{f}: {type(e).__name__}: {e}')
print(f'Import OK: {ok}')
print(f'Import FAIL: {len(fails)}')
for f in fails: print(f'  {f}')
"

echo "=== 18. PYTEST ==="
pip install -e ".[dev]" --break-system-packages 2>/dev/null
PYTHONPATH=python pytest tests/ -v --tb=short 2>&1

echo "=== 19. MAKE CHECK ==="
make check 2>&1 | tail -30

echo "=== 20. MAKE MERGE-GATE ==="
make merge-gate 2>&1

echo "=== 21. DOCS + EXAMPLES + CI + DOCKER ==="
ls -la docs/ examples/ .github/workflows/ docker/ helm/ 2>/dev/null
shopt -s nullglob
for f in docs/*.md; do echo "--- $f ---"; cat "$f"; done
for f in examples/*.py; do echo "--- $f ---"; cat "$f"; done
shopt -u nullglob
cat .github/workflows/*.yml 2>/dev/null
cat Dockerfile 2>/dev/null

echo "=== 22. LISENSSI + SPDX ==="
head -20 LICENSE
echo "Tiedostot ILMAN SPDX headeria (python/cos top-level only):"
grep -rL "SPDX-License-Identifier" python/cos/*.py 2>/dev/null

echo "=== 23. README + CHANGELOG + CONTRIBUTING ==="
cat README.md
echo "---"
cat CHANGELOG.md 2>/dev/null
echo "---"
cat CONTRIBUTING.md 2>/dev/null

echo "=========================================="
echo "RAPORTTI VALMIS"
echo "=========================================="
echo "Full log: $(pwd)/$OUT"
