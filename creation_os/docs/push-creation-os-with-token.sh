#!/usr/bin/env bash
# Push ONLY to spektre-labs/creation-os (HTTPS + PAT). Token is read hidden, not echoed.
# Prerequisites: run push-to-creation-os.sh first so STAGING exists and has commits.
# Security: do not paste your PAT into chat or commit it. Prefer SSH keys when you can.
set -euo pipefail

STAGING="${CREATION_OS_STAGING:-$HOME/Desktop/creation-os-staging}"
REPO_PATH="spektre-labs/creation-os.git"

if [[ ! -d "$STAGING/.git" ]]; then
  echo "Ei git-repoo: $STAGING" >&2
  echo "Aja ensin: bash \"\$CREATION_OS_SRC/creation_os/docs/push-to-creation-os.sh\"" >&2
  exit 1
fi

printf 'GitHub Personal Access Token (piilotettu, Enter lopettaa): '
read -rs GHTOKEN
echo

if [[ -z "${GHTOKEN}" ]]; then
  echo "Tyhjä token, keskeytetään." >&2
  exit 1
fi

# oauth2:<PAT> on GitHubin yleinen HTTPS-malli; token ei jää remote-URL:iin pysyvästi
URL="https://oauth2:${GHTOKEN}@github.com/${REPO_PATH}"
unset GHTOKEN

git -C "$STAGING" push "$URL" HEAD:main

echo "Valmis: push tehty kohteeseen creation-os (main)."
