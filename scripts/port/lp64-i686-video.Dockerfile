# The 32-bit video reference for the 64-bit port (LP64-005 Task 9): the
# `goldeneye` image (root Dockerfile) already builds the i386 SDL and holds
# Mesa's i386 GL and llvmpipe; SDL's offscreen driver also needs the i386
# EGL loader and Mesa's EGL vendor library, which this adds. Build it with
# `python3 scripts/port/lp64_env.py build-images` (after `goldeneye`).
FROM goldeneye

USER root
RUN apt-get update && \
    apt-get -y install --no-install-recommends libegl1:i386 libegl-mesa0:i386 && \
    rm -rf /var/lib/apt/lists/*
USER dev

WORKDIR /home/dev/project

CMD ["/bin/bash"]
