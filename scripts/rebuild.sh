#!/bin/bash
set -e

PROJECT_ROOT="/home/cc/range-reduce"

cd "$PROJECT_ROOT"

git submodule update --init --recursive

mkdir -p build
cd build
cmake ..
make -j"${nproc}"

clear

echo "build complete!"