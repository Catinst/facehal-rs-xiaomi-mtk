#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
mkdir -p out/tests
cargo test --locked
cxx=${CXX_HOST:-g++}
"$cxx" -std=c++17 -O0 -pthread -Wall -Wextra -Werror -I native \
    native/AlgorithmThreadBridge.cpp tests/thread_owner_test.cpp -o out/tests/thread-owner
out/tests/thread-owner
"$cxx" -std=c++17 -O0 -Wall -Wextra -Werror -I tests/include -I native \
    tests/preview_geometry_test.cpp -o out/tests/preview-geometry
out/tests/preview-geometry
