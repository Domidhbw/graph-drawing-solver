
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

mkdir -p results

DUPLICATE_REPAIR_OUTPUT="results/repaired_duplicate_coordinates.json"
VERTEX_ON_EDGE_REPAIR_OUTPUT="results/repaired_vertex_on_edge.json"

echo "Checking that duplicate-coordinate input is initially invalid..."
if ./build/solver testdata/invalid_duplicate_coordinates.json validate; then
    echo "FAILED: duplicate-coordinate input was unexpectedly valid before repair."
    exit 1
fi

echo "Repairing duplicate-coordinate input..."
./build/solver testdata/invalid_duplicate_coordinates.json repair "$DUPLICATE_REPAIR_OUTPUT"

echo "Validating repaired duplicate-coordinate output..."
./build/solver "$DUPLICATE_REPAIR_OUTPUT" validate

echo "Checking that repair mode requires an output path. This must fail..."
if ./build/solver testdata/invalid_duplicate_coordinates.json repair; then
    echo "FAILED: repair mode accepted a missing output path."
    exit 1
fi

echo "Checking that vertex-on-edge input is not falsely repaired by duplicate-coordinate repair..."
rm -f "$VERTEX_ON_EDGE_REPAIR_OUTPUT"

if ./build/solver testdata/invalid_vertex_on_edge.json repair "$VERTEX_ON_EDGE_REPAIR_OUTPUT"; then
    echo "FAILED: vertex-on-edge input was accepted by duplicate-coordinate repair."
    exit 1
fi

if [[ -e "$VERTEX_ON_EDGE_REPAIR_OUTPUT" ]]; then
    echo "FAILED: repair mode wrote an output for an unrepaired vertex-on-edge input."
    exit 1
fi

echo "All repair tests passed."
