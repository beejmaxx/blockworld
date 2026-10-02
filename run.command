#!/bin/zsh
set -euo pipefail
cd -- "$(dirname -- "$0")"
cmake --preset dev
cmake --build --preset dev
exec ./build/blockworld.app/Contents/MacOS/blockworld "$@"
