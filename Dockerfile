# syntax=docker/dockerfile:1.6
# SPDX-License-Identifier: AGPL-3.0-or-later
#
# Creation OS — v107 distribution · Docker image.
#
# Builds the forty-kernel stack + v106 σ-server in a clean Debian
# environment, then copies the three binaries and the v108 web/ root
# into a small runtime image.  No model weights are baked in — mount
# them at runtime:
#
#   docker build -t spektrelabs/creation-os:latest .
#   docker run --rm -p 8080:8080 \
#              -v $HOME/.creation-os:/root/.creation-os \
#              spektrelabs/creation-os:latest
#
# Expected on success: an HTTP σ-server listening on :8080, with
# /v1/chat/completions, /v1/reason, /v1/models and the σ-UI at /.

# ------------ builder ----------------------------------------------------
FROM debian:bookworm-slim AS builder

RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        git \
        make \
        && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . /src

# Build everything we ship.  `cos` aggregates the CLI; standalone-v106
# is the σ-server; standalone is the portable reference kernel.  All
# are single-binary outputs at the repo root.
RUN make -j"$(nproc)" cos                \
 && make -j"$(nproc)" standalone-v106    \
 && make -j"$(nproc)" standalone         \
 && strip cos creation_os_server creation_os || true

# ------------ runtime ----------------------------------------------------
FROM debian:bookworm-slim AS runtime

RUN apt-get update -qq && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        && rm -rf /var/lib/apt/lists/*

RUN useradd --create-home --shell /bin/bash cos
USER cos
WORKDIR /home/cos

COPY --from=builder /src/cos                /usr/local/bin/cos
COPY --from=builder /src/creation_os_server /usr/local/bin/creation_os_server
COPY --from=builder /src/creation_os        /usr/local/bin/creation_os
COPY --from=builder /src/web                /usr/local/share/creation-os/web

ENV COS_V106_WEB_ROOT=/usr/local/share/creation-os/web
EXPOSE 8080

HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -fsS http://127.0.0.1:8080/health || exit 1

ENTRYPOINT ["/usr/local/bin/creation_os_server"]
CMD ["--host", "0.0.0.0", "--port", "8080", "--web-root", "/usr/local/share/creation-os/web"]
