
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/leaf_reducer_probe" ]]; then
    echo "ERROR: ./build/leaf_reducer_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

if [[ ! -x "./build/leaf_reinsertion_probe" ]]; then
    echo "ERROR: ./build/leaf_reinsertion_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

echo "Running preprocessing tests..."

echo "Test 1: star graph removes four leaves..."
./build/leaf_reducer_probe \
    testdata/leaf_reduction_star.json \
    1 \
    0 \
    4

echo "Test 2: triangle has no leaves..."
./build/leaf_reducer_probe \
    testdata/triangle_no_crossing.json \
    3 \
    3 \
    0

echo "Test 3: star graph reinserts four leaves..."
./build/leaf_reinsertion_probe \
    testdata/leaf_reduction_star.json \
    1 \
    0 \
    4

echo "Test 4: triangle reinsertion is a no-op..."
./build/leaf_reinsertion_probe \
    testdata/triangle_no_crossing.json \
    3 \
    3 \
    0

echo "All preprocessing tests passed."
