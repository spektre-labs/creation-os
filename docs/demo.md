# Demo — Creation OS in 60 seconds (silent)

The release-channel video `docs/demo.mp4` is **attached to each
GitHub Release** rather than tracked in-tree (binary blobs bloat
clones).  The latest release always carries the file at:

```
https://github.com/spektre-labs/creation-os/releases/latest/download/demo.mp4
```

## Storyboard

One continuous screen recording, 60 s, no audio, 1080p:

| t (s) | shot | text overlay |
|---:|---|---|
|  0 – 5  | terminal: `curl -fsSL https://get.creation-os.dev \| bash` | "one command" |
|  5 – 20 | installer scrolls — toolchain check → clone → build → `forty-kernel rollup: 39 PASS 0 FAIL` | "forty falsifiable kernels · 0 fail" |
| 20 – 25 | terminal clears; `creation_os_server` boot line "listening on 127.0.0.1:8080" | "σ-server up" |
| 25 – 30 | browser opens `http://127.0.0.1:8080` — the v108 σ-UI | "local · no cloud" |
| 30 – 40 | user types *"What is 47 × 53?"* · stream answer *"2 491"* · σ-product bar lights green | "σ=0.07 · confident" |
| 40 – 55 | user types *"What will the EUR/USD rate be on 2027-02-01?"* · UI flashes **amber** · reply is `I don't know.` | "σ=0.78 > τ · abstained" |
| 55 – 60 | cursor moves to badge row: *40 kernels · σ-governed · v111.1* | "1 = 1" |

## Reproducing the shots

1. `bash scripts/install.sh`  (or `./scripts/quickstart.sh` if already
   cloned)
2. `creation_os_server --config ~/.creation-os/config.toml`
3. Open `http://127.0.0.1:8080`
4. Prompt 1: `What is 47 × 53?`     → low σ, answer emitted
5. Prompt 2: `What will the EUR/USD rate be on 2027-02-01?` → σ > τ,
   server returns `abstained: true`

## Re-encoding

Source cut lives at `docs/demo/source.mov` on the release builder
(not tracked).  Encode knobs used for the attached `demo.mp4`:

```bash
ffmpeg -i source.mov -an \
    -c:v libx264 -crf 24 -preset slow \
    -vf "scale=1920:-2:flags=lanczos" \
    -movflags +faststart \
    demo.mp4
```

Target ≤ 6 MiB, so the asset fits inline on the GitHub Release page
without LFS.  Silence is intentional: the point of the demo is the
σ-stack moving live, not a voice-over.
