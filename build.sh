#!/usr/bin/env bash
# One-command build for Extendo (macOS / Linux).
# Qt is downloaded automatically by CMake — you do NOT need to install Qt yourself.
#
#   git clone --recursive https://github.com/aidnth14/Extendo.git
#   cd Extendo
#   ./build.sh
#
set -euo pipefail

cd "$(dirname "$0")"

say()  { printf "\033[1;36m==>\033[0m %s\n" "$*"; }
die()  { printf "\033[1;31mERROR:\033[0m %s\n" "$*" >&2; exit 1; }
have() { command -v "$1" >/dev/null 2>&1; }

# ---- 1. prerequisites ---------------------------------------------------------
OS="$(uname -s)"
MISSING=()
for tool in git cmake ninja; do have "$tool" || MISSING+=("$tool"); done
if [ "${#MISSING[@]}" -ne 0 ]; then
    echo "Missing tools: ${MISSING[*]}"
    if [ "$OS" = "Darwin" ]; then
        echo "Install them with:  xcode-select --install && brew install cmake ninja"
    else
        echo "Install them with:  sudo apt install -y build-essential cmake ninja-build git git-lfs"
        echo "(On Linux you also need Vulkan + SDL2 dev headers — see README.md)"
    fi
    die "install the tools above, then re-run ./build.sh"
fi

# ---- 2. submodules ------------------------------------------------------------
if [ -f .gitmodules ]; then
    say "Fetching submodules (one-time, a few minutes)…"
    git submodule update --init --recursive
fi

# ---- 3. configure + build -----------------------------------------------------
ARCH_FLAG=()
BUILD_DIR="build"
if [ "$OS" = "Darwin" ]; then
    HOST_ARCH="$(uname -m)"   # arm64 (Apple Silicon) or x86_64 (Intel)
    ARCH_FLAG=(-DCMAKE_OSX_ARCHITECTURES="$HOST_ARCH")
    BUILD_DIR="build/$HOST_ARCH"
fi

say "Configuring (Qt auto-downloads on first run)…"
cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_LTO=OFF \
    "${ARCH_FLAG[@]}"

say "Building (this is the long part — grab a coffee)…"
cmake --build "$BUILD_DIR"

# ---- 4. package / point to the binary ----------------------------------------
if [ "$OS" = "Darwin" ]; then
    say "Creating the .app bundle…"
    cmake --build "$BUILD_DIR" --target bundle
    APP="$BUILD_DIR/bundle/extendo.app"
    xattr -cr "$APP" 2>/dev/null || true
    say "Done!  Launch with:  open \"$APP\""
else
    say "Done!  Run with:  ./$BUILD_DIR/bin/Release/extendo"
fi
