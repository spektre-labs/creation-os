# SPDX-License-Identifier: AGPL-3.0-or-later
# NOTE: This image builds the portable C harness. It does NOT bundle multi‑GB model weights.
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y --no-install-recommends build-essential ca-certificates \
  && rm -rf /var/lib/apt/lists/*
WORKDIR /work
COPY . /work
RUN make standalone-v28
EXPOSE 8080
CMD ["./creation_os_v28", "--server", "--port", "8080"]
