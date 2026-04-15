# Ensimmäinen push: `main` = koko Creation OS -työ

Tavoite: yksi repo ([creation-os](https://github.com/spektre-labs/creation-os)), yksi haara (`main`), yksi `origin` — kaikki tämän tuotteen työ menee sinne.

## Nopein tapa

```bash
export CREATION_OS_SRC="$HOME/Desktop/spektre-protocol/spektre-protocol"
export CREATION_OS_STAGING="$HOME/Desktop/creation-os-staging"

bash "$CREATION_OS_SRC/creation_os/docs/push-to-creation-os.sh"
```

Oletus on **kevyt** (ei `llama.cpp` / `external` / `third_party`, ei `mlx_creation_os/artifacts/`), jotta kone ei jäädy. Täysi kopio myöhemmin:

```bash
CREATION_OS_INCLUDE_VENDOR=1 CREATION_OS_INCLUDE_ARTIFACTS=1 bash "$CREATION_OS_SRC/creation_os/docs/push-to-creation-os.sh"
```

```bash
cd "$CREATION_OS_STAGING"
git remote set-url origin git@github.com:spektre-labs/creation-os.git
git push -u origin main
```

**Vain token (HTTPS):** kun staging-repossa on jo commit, voit pushata näin (token kysytään piilossa, ei jää `origin`-remoteen):

```bash
bash "$CREATION_OS_SRC/creation_os/docs/push-creation-os-with-token.sh"
```

Kevyt ajo kopioi `creation_os/`, `mlx_creation_os/` (ilman `artifacts/`), `core/`, sekä `tests/`, `ios/`, `environment_files/` jos ne on olemassa. Pois jäävät venvit, painot (`*.safetensors`, jne.) ja `.gitignore`-zipit.

Täyteen ajoan (mukana `llama.cpp`, `external`, `third_party`, `artifacts/`) käytä ylempänä olevaa `CREATION_OS_INCLUDE_VENDOR=1 CREATION_OS_INCLUDE_ARTIFACTS=1` -riviä — vain kun kone jaksaa.
