# Native aarch64 toolchain for the 64-bit port (LP64). The 32-bit and x86_64
# builds use the root Dockerfile's `goldeneye` image instead.
FROM ubuntu:24.04

ARG HOST_UID=1000
ARG HOST_GID=1000

RUN apt-get update && \
    apt-get -y install --no-install-recommends \
        build-essential cmake ninja-build python3 git ca-certificates libsdl2-dev && \
    rm -rf /var/lib/apt/lists/*

RUN if getent passwd "${HOST_UID}" >/dev/null; then \
        userdel -r "$(getent passwd "${HOST_UID}" | cut -d: -f1)" 2>/dev/null || \
        userdel "$(getent passwd "${HOST_UID}" | cut -d: -f1)"; \
    fi && \
    if getent group "${HOST_GID}" >/dev/null; then \
        groupdel "$(getent group "${HOST_GID}" | cut -d: -f1)" 2>/dev/null || true; \
    fi && \
    groupadd -g "${HOST_GID}" dev && \
    useradd -m -s /bin/bash -u "${HOST_UID}" -g "${HOST_GID}" dev
USER dev

WORKDIR /home/dev/project

CMD ["/bin/bash"]
