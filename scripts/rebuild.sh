#!/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

git submodule update --init --recursive

if [[ "$(uname)" == "Darwin" ]]; then
  NPROC="$(sysctl -n hw.ncpu)"
else
  NPROC="$(nproc)"
fi

mkdir -p build
cd build
cmake ..
make -j"$NPROC"

clear

echo "build complete!"