#!/usr/bin/env bash
# A private virtual sink for Linux browser audits; never change host audio.
set -euo pipefail
if [[ ! -f /.dockerenv ]]; then
    printf 'Run this helper inside the isolated browser-audio image.\n' >&2
    exit 1
fi
export PULSE_SERVER=unix:/tmp/midas-audit-pulse.sock
pulseaudio --daemonize=yes --exit-idle-time=-1 --log-target=stderr \
    --load='module-native-protocol-unix auth-anonymous=1 socket=/tmp/midas-audit-pulse.sock' \
    --load='module-null-sink sink_name=midas-audit'
exec "$@"
