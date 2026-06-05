#!/usr/bin/env bash
# Capture a Linux core file for BUG-001.
#
# Requires: g++ 13+ or clang 17+, gdb (to convert core to minidump if desired).

set -euo pipefail
cd "$(dirname "$0")"

ROOT="$(cd ../../.. && pwd)"

g++ -std=c++20 -fno-exceptions -fno-rtti -O0 -g \
    -I"${ROOT}/include" \
    "${ROOT}/src/engine_demo/allocator.cpp" \
    repro.cpp \
    -o repro

ulimit -c unlimited
echo "Running repro; a core file may be written to /var/lib/systemd/coredump/ or PWD."
./repro || true

if [[ -f core ]]; then
    mv core BUG-001.core.local
    echo "Core saved as BUG-001.core.local (gitignored)."
fi
