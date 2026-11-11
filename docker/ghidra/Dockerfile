# Ghidra 12.2 needs JDK 25 (application.java.min=25).
# Ghidra (headless) for the Pen Pen TriIcelon static analysis (session S1).
# Network is used only while building this image. Containers that mount disc
# data run it with --network none --read-only (see tools/ghidra.sh, PLAN 9).
FROM eclipse-temurin:25-jdk-noble

ARG GHIDRA_VERSION=12.2
ARG GHIDRA_DATE=20261016
ARG GHIDRA_SHA256=2c2cd776db3c1f6a097ba418922378ac3649cadd5e844a6f5f088ae628a7be67

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
      curl ca-certificates unzip python3 gcc g++ make \
 && rm -rf /var/lib/apt/lists/*

RUN set -eux; \
    f=ghidra_${GHIDRA_VERSION}_PUBLIC_${GHIDRA_DATE}.zip; \
    curl -fsSL -o /tmp/$f https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_${GHIDRA_VERSION}_build/$f; \
    echo "${GHIDRA_SHA256}  /tmp/$f" | sha256sum -c -; \
    unzip -q /tmp/$f -d /opt; rm /tmp/$f; \
    mv /opt/ghidra_${GHIDRA_VERSION}_PUBLIC /opt/ghidra; \
    ls /opt/ghidra/Ghidra/Features/Decompiler/os/

# The release ships no linux_arm_64 natives (decompiler, sleigh, demangler);
# build them when the image is built for arm64 (Apple silicon hosts). Ghidra
# finds user-built natives in <module>/build/os/<platform>.
RUN set -eux; \
    if [ "$(uname -m)" = aarch64 ]; then \
      cd /opt/ghidra/support/gradle && ./gradlew --no-daemon -q buildNatives; \
      rm -rf /root/.gradle /opt/ghidra/support/gradle/build /opt/ghidra/Ghidra/*/*/build/objs /opt/ghidra/Ghidra/*/*/build/tmp; \
      test -x /opt/ghidra/Ghidra/Features/Decompiler/build/os/linux_arm_64/decompile; \
    fi

# uid 1000 (ubuntu) owns the penpen-work and penpen-ghidra volumes.
USER ubuntu
ENV GHIDRA_HOME=/opt/ghidra
WORKDIR /tmp
