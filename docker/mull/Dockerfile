# Pen Pen S12: Mull 0.37.0 (mutation testing as a clang pass plugin) and the clang it matches (LLVM 18.1.3, Ubuntu
# 24.04's clang-18), plus gcc for the rest of the native build. Network only while this image builds; containers from
# it run with --network none --read-only --cap-drop ALL --security-opt no-new-privileges (tools/s3.sh mull).
FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl python3 clang-18 llvm-18 gcc libc6-dev \
    && rm -rf /var/lib/apt/lists/*
ARG MULL_BASE=Mull-0.37.0-ubuntu-aarch64-24.04.deb
ARG MULL_DEB=Mull-18-0.37.0-LLVM-18.1.3-ubuntu-aarch64-24.04.deb
RUN cd /tmp && curl -sSLO https://github.com/mull-project/mull/releases/download/0.37.0/$MULL_BASE \
    && curl -sSLO https://github.com/mull-project/mull/releases/download/0.37.0/$MULL_DEB && sha256sum /tmp/*.deb \
    && apt-get update && apt-get install -y --no-install-recommends /tmp/$MULL_BASE /tmp/$MULL_DEB \
    && rm -rf /var/lib/apt/lists/* /tmp/*.deb && dpkg -L mull mull-18 | grep -E 'bin/|\.so|\.dylib'
