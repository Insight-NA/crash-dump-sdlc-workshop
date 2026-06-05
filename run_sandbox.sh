#!/usr/bin/env bash
# run_sandbox.sh — build ea-sandbox and launch it interactively (macOS / Linux).
#
# Usage:
#   ./run_sandbox.sh                          # rope scene, seed 42
#   ./run_sandbox.sh --scene cloth            # different scene
#   ./run_sandbox.sh --scene storm --seed 7   # custom scene + seed
#   ./run_sandbox.sh --headless --frames 60   # headless CI mode
#
# All extra arguments are forwarded to ea-sandbox unchanged.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ── vcpkg ────────────────────────────────────────────────────────────────────
if [[ -z "${VCPKG_ROOT:-}" ]]; then
    if [[ -d "$HOME/vcpkg" ]]; then
        export VCPKG_ROOT="$HOME/vcpkg"
    else
        echo "ERROR: VCPKG_ROOT is not set and ~/vcpkg does not exist." >&2
        echo "       Clone vcpkg: git clone https://github.com/microsoft/vcpkg ~/vcpkg && ~/vcpkg/bootstrap-vcpkg.sh" >&2
        exit 1
    fi
fi

# ── configure (only if needed) ───────────────────────────────────────────────
if [[ "$(uname -s)" == "Darwin" ]]; then
    PRESET="macos-arm64"
else
    PRESET="default-debug"
fi

if [[ ! -f "build/CMakeCache.txt" ]]; then
    echo "==> Configuring (preset: $PRESET) …"
    cmake --preset "$PRESET"
else
    echo "==> build/CMakeCache.txt found — skipping configure (delete build/ to force reconfigure)"
fi

# ── build ────────────────────────────────────────────────────────────────────
echo "==> Building ea-sandbox …"
cmake --build build --target ea-sandbox

# ── run ──────────────────────────────────────────────────────────────────────
if [[ "$(uname -s)" == "Darwin" ]]; then
    APP="build/apps/sandbox/ea-sandbox.app"
    BIN="$APP/Contents/MacOS/ea-sandbox"

    # Headless / screenshot mode: invoke binary directly (no GUI activation needed)
    if printf '%s\n' "$@" | grep -qE '^(--headless|--screenshot)$'; then
        echo "==> Running headless: $BIN $*"
        exec "$BIN" "$@"
    fi

    # Interactive: use `open` so Launch Services gives the process foreground GUI status.
    # Extra --args are forwarded.
    OPEN_ARGS=()
    for arg in "$@"; do
        OPEN_ARGS+=("$arg")
    done

    # Default scene + seed when none supplied
    if [[ ${#OPEN_ARGS[@]} -eq 0 ]]; then
        OPEN_ARGS=(--seed 42 --scene rope)
    fi

    echo "==> Launching: open $APP --args ${OPEN_ARGS[*]}"
    exec open "$APP" --args "${OPEN_ARGS[@]}"
else
    BIN="build/apps/sandbox/ea-sandbox"
    LAUNCH_ARGS=("$@")
    if [[ ${#LAUNCH_ARGS[@]} -eq 0 ]]; then
        LAUNCH_ARGS=(--seed 42 --scene rope)
    fi
    echo "==> Running: $BIN ${LAUNCH_ARGS[*]}"
    exec "$BIN" "${LAUNCH_ARGS[@]}"
fi
