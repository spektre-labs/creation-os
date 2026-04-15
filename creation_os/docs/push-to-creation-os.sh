#!/usr/bin/env bash
# Build a clean creation-os repo (main) from a mixed local tree. Push yourself: git push -u origin main
# Default is LIGHT: no llama.cpp/external/third_party, no mlx artifacts/ — keeps Mac responsive.
# Full tree: CREATION_OS_INCLUDE_VENDOR=1 CREATION_OS_INCLUDE_ARTIFACTS=1 bash ...
set -euo pipefail

SRC="${CREATION_OS_SRC:-$HOME/Desktop/spektre-protocol/spektre-protocol}"
STAGING="${CREATION_OS_STAGING:-$HOME/Desktop/creation-os-staging}"
REPO_URL="${CREATION_OS_REPO_URL:-https://github.com/spektre-labs/creation-os.git}"

# Lower priority so UI stays usable (macOS nice).
RSYNC() { nice -n 19 rsync "$@"; }

EXCLUDES=(
  --exclude='.venv/'
  --exclude='venv/'
  --exclude='__pycache__/'
  --exclude='.pytest_cache/'
  --exclude='node_modules/'
  --exclude='.DS_Store'
  --exclude='*.o'
  --exclude='*.pyc'
  --exclude='*.safetensors'
  --exclude='*.gguf'
  --exclude='*.metallib'
)

if [[ ! -d "$SRC/creation_os" || ! -d "$SRC/mlx_creation_os" ]]; then
  echo "SRC must contain creation_os/ and mlx_creation_os/ (got: $SRC)" >&2
  exit 1
fi

echo "=== Creation OS staging (light by default) ==="
echo "SRC=$SRC"
echo "STAGING=$STAGING"

rm -rf "$STAGING"
git clone "$REPO_URL" "$STAGING"
cd "$STAGING"

mkdir -p creation_os mlx_creation_os core llama.cpp external third_party tests ios environment_files

MLX_EXCLUDES=("${EXCLUDES[@]}")
if [[ "${CREATION_OS_INCLUDE_ARTIFACTS:-}" != "1" ]]; then
  MLX_EXCLUDES+=(--exclude='artifacts/')
  echo "(mlx: skipping artifacts/ — set CREATION_OS_INCLUDE_ARTIFACTS=1 to copy)"
fi

echo "[1/4] rsync creation_os/ ..."
RSYNC -a "${EXCLUDES[@]}" "$SRC/creation_os/" creation_os/

echo "[2/4] rsync mlx_creation_os/ ..."
RSYNC -a "${MLX_EXCLUDES[@]}" "$SRC/mlx_creation_os/" mlx_creation_os/

echo "[3/4] rsync core/ ..."
RSYNC -a "${EXCLUDES[@]}" "$SRC/core/" core/

if [[ "${CREATION_OS_INCLUDE_VENDOR:-}" == "1" ]]; then
  echo "[4/4] rsync vendor trees (slow, heavy disk) ..."
  for d in llama.cpp external third_party; do
    if [[ -d "$SRC/$d" ]]; then
      RSYNC -a "${EXCLUDES[@]}" "$SRC/$d/" "$d/"
    fi
  done
else
  echo "[4/4] skip llama.cpp, external, third_party (CREATION_OS_INCLUDE_VENDOR=1 to include)"
fi

for d in tests ios environment_files; do
  if [[ -d "$SRC/$d" ]]; then
    echo "rsync $d/ ..."
    RSYNC -a "${EXCLUDES[@]}" "$SRC/$d/" "$d/"
  fi
done

for f in "install_genesis 2.sh" "start_genesis 2" "spektre-rpc-node 2"; do
  if [[ -f "$SRC/$f" ]]; then cp "$SRC/$f" .; fi
done

cat > .gitignore << 'EOF'
.DS_Store
.venv/
venv/
__pycache__/
*.pyc
.pytest_cache/
node_modules/
creation_os_v1.zip
creation_os_weights.bin
*.zip
EOF

cat > README.md << 'EOF'
# Creation OS

Spektre Creation OS / Genesis — engineering lives here. Default branch: `main`.

Weight and model blobs are excluded from git (see `.gitignore`); ship via Releases, LFS, or your artifact store.
EOF

git add -A
git status
git commit -m "Import Creation OS tree (main)" || { echo "Nothing to commit?"; exit 1; }
echo ""
echo "Done. Push:"
echo "  cd \"$STAGING\""
echo "  git remote set-url origin git@github.com:spektre-labs/creation-os.git"
echo "  git push -u origin main"
echo ""
echo "Full vendor + artifacts later:"
echo "  CREATION_OS_INCLUDE_VENDOR=1 CREATION_OS_INCLUDE_ARTIFACTS=1 bash .../push-to-creation-os.sh"
