# Pinned runtime baseline: all shipped ELF files are built on Ubuntu 22.04.
FROM ubuntu:22.04@sha256:e0f80c829509ef4df9e76bd8944eb7b8ec25cbb379b112a7f9448b2c77a77c9b
ENV DEBIAN_FRONTEND=noninteractive
RUN dpkg --add-architecture i386 && apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates cmake git python3 build-essential gcc-multilib g++-multilib \
    libgl-dev:i386 libcurl4-openssl-dev:i386 libx11-dev:i386 libxext-dev:i386 \
    libxrandr-dev:i386 libxcursor-dev:i386 libxfixes-dev:i386 libxi-dev:i386 \
    libxss-dev:i386 libasound2-dev:i386 libpulse-dev:i386 libudev-dev:i386 \
    squashfs-tools patchelf file ccache && rm -rf /var/lib/apt/lists/*
WORKDIR /src
