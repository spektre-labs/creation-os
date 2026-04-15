# Git: one remote, one product

**Creation OS** — commit and push **only** to [spektre-labs/creation-os](https://github.com/spektre-labs/creation-os), branch **`main`**.

Paikallinen kansiorakenne voi näyttää monorepolta; ainoa GitHub-remote, jonne tämä työ kuuluu, on **creation-os**. Älä käytä tähän työhön mitään muuta `origin`-osoitetta.

Isot binaarit (mallit, metallib, zip) on jätettävä pois gitistä teknisistä rajoista (ks. `push-to-creation-os.sh` ja `.gitignore`) — ne jaetaan erikseen (LFS / Release / oma säilö).
