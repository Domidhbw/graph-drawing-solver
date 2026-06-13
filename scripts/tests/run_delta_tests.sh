
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/crossing_delta_probe" ]]; then
    echo "ERROR: ./build/crossing_delta_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

echo "Running crossing delta tests..."

echo "Test 1: moving node removes one crossing..."
./build/crossing_delta_probe \
    testdata/delta_move_simple.json \
    3 \
    10 \
    20 \
    1 \
    1 \
    0 \
    0

echo "Test 2: moving node keeps one crossing..."
./build/crossing_delta_probe \
    testdata/delta_move_simple.json \
    3 \
    10 \
    -5 \
    1 \
    1 \
    1 \
    1

echo "Test 3: moving node creates one crossing from planar layout..."
./build/crossing_delta_probe \
    testdata/no_crossing_2_edges.json \
    3 \
    10 \
    0 \
    0 \
    0 \
    1 \
    1

echo "Checking that invalid node index fails..."
if ./build/crossing_delta_probe \
    testdata/delta_move_simple.json \
    99 \
    10 \
    20 \
    1 \
    1 \
    0 \
    0; then
    echo "FAILED: invalid node index was accepted."
    exit 1
fi

echo "All crossing delta tests passed."
