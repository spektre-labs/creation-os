# Homebrew tap mirror (`spektre-labs/homebrew-cos`)

This directory is a **copy-paste source** for the separate GitHub repository
**[spektre-labs/homebrew-cos](https://github.com/spektre-labs/homebrew-cos)**.
The canonical application source remains **[spektre-labs/creation-os](https://github.com/spektre-labs/creation-os)**.

## Publish the tap (maintainers)

1. Create (once) the empty GitHub repo `spektre-labs/homebrew-cos` if it does not exist.
2. Copy `Formula/creation-os.rb` from here into that repo on `main`.
3. Replace **`sha256 "PLACEHOLDER"`** with the digest of the published archive:
   - After tagging `vX.Y.Z` on `creation-os`, run  
     `curl -sL "https://github.com/spektre-labs/creation-os/archive/refs/tags/vX.Y.Z.tar.gz" | shasum -a 256`
   - Or: `brew fetch --build-from-source ./Formula/creation-os.rb` and paste the reported checksum.
4. `brew audit --strict Formula/creation-os.rb` and fix any audit notes (license string, etc.).

## User install

```bash
brew tap spektre-labs/cos
brew install creation-os
cos demo --batch
```

## One-liner (also clones/builds on Linux)

See **`scripts/install_cos.sh`** in the main `creation-os` repository.
