# Tools image for the Pen Pen TriIcelon survey. The disc image is only ever
# handled in containers started with --network none (see docs/port/PLAN.md).
FROM ubuntu:24.04
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      7zip python3 binutils-sh4-linux-gnu file xxd bsdextrautils \
 && rm -rf /var/lib/apt/lists/*

USER ubuntu
WORKDIR /work
