# Pinned runtime baseline of the x86_64 packages: the base of linux-release.Dockerfile, 64-bit only.
FROM ubuntu:22.04@sha256:e0f80c829509ef4df9e76bd8944eb7b8ec25cbb379b112a7f9448b2c77a77c9b
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates cmake git python3 build-essential \
    libgl-dev libcurl4-openssl-dev libx11-dev libxext-dev \
    libxrandr-dev libxcursor-dev libxfixes-dev libxi-dev \
    libxss-dev libasound2-dev libpulse-dev libudev-dev \
    squashfs-tools patchelf file ccache && rm -rf /var/lib/apt/lists/*
WORKDIR /src
