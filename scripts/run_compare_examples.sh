#!/usr/bin/env bash
set -euo pipefail

run_compare() {
    local file="$1"

    echo
    echo "============================================================"
    echo "Compare: $file"
    echo "============================================================"

    ./build/solver "$file" compare
}

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

run_compare "testdata/no_crossing_2_edges.json"
run_compare "testdata/one_crossing_2_edges.json"
run_compare "testdata/shared_endpoint_no_crossing.json"
run_compare "testdata/triangle_no_crossing.json"
run_compare "testdata/square_diagonals_one_crossing.json"
run_compare "testdata/k4_convex.json"
run_compare "testdata/collinear_overlap.json"
run_compare "testdata/disconnected_components.json"

echo
echo "All compare examples completed."
