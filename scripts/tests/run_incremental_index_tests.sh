
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/incremental_crossing_index_probe" ]]; then
    echo "ERROR: ./build/incremental_crossing_index_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

echo "Running incremental crossing index tests..."

echo "Test 1: apply move removes one crossing..."
./build/incremental_crossing_index_probe \
    testdata/delta_move_simple.json \
    3 \
    10 \
    20 \
    1 \
    1 \
    0 \
    0

echo "Test 2: apply move keeps one crossing..."
./build/incremental_crossing_index_probe \
    testdata/delta_move_simple.json \
    3 \
    10 \
    -5 \
    1 \
    1 \
    1 \
    1

echo "Test 3: apply move creates one crossing from planar layout..."
./build/incremental_crossing_index_probe \
    testdata/no_crossing_2_edges.json \
    3 \
    10 \
    0 \
    0 \
    0 \
    1 \
    1

echo "Checking that invalid node index fails..."
if ./build/incremental_crossing_index_probe \
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

echo "All incremental crossing index tests passed."
