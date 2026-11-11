# S5: headless Chromium (Debian package, SwiftShader WebGL2) + node 22 for the renderer's capture path.
# Network only during the build; every run is --network none --read-only --cap-drop ALL (tools/s5.sh).
FROM node:22-slim
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      chromium fonts-dejavu-core \
 && rm -rf /var/lib/apt/lists/*
USER node
WORKDIR /tmp
