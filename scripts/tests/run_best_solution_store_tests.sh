
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/best_solution_store_probe" ]]; then
    echo "ERROR: ./build/best_solution_store_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

mkdir -p results

echo "Running best solution store tests..."

echo "Test 1: better second candidate replaces first checkpoint..."
./build/best_solution_store_probe \
    testdata/one_crossing_2_edges.json \
    testdata/triangle_no_crossing.json \
    results/best_store_replace.json \
    1 \
    1 \
    0 \
    0

echo "Test 2: worse second candidate does not replace first checkpoint..."
./build/best_solution_store_probe \
    testdata/triangle_no_crossing.json \
    testdata/one_crossing_2_edges.json \
    results/best_store_keep.json \
    1 \
    0 \
    0 \
    0

echo "Test 3: invalid first candidate is ignored, valid second candidate is stored..."
./build/best_solution_store_probe \
    testdata/invalid_duplicate_coordinates.json \
    testdata/triangle_no_crossing.json \
    results/best_store_invalid_first.json \
    0 \
    1 \
    0 \
    0

echo "All best solution store tests passed."
