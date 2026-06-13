
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

VALID_OUTPUT="results/validation_valid_output.json"
FORBIDDEN_OUTPUT="results/validation_should_not_be_written.json"

echo "Creating valid output..."
./build/solver testdata/one_crossing_2_edges.json circular "$VALID_OUTPUT" > /dev/null

echo "Validating valid output..."
./build/solver "$VALID_OUTPUT" validate

echo "Validating duplicate-coordinate input. This must fail..."
if ./build/solver testdata/invalid_duplicate_coordinates.json validate; then
    echo "FAILED: duplicate-coordinate input was accepted."
    exit 1
fi

echo "Validating vertex-on-edge input. This must fail..."
if ./build/solver testdata/invalid_vertex_on_edge.json validate; then
    echo "FAILED: vertex-on-edge input was accepted."
    exit 1
fi

echo "Validating that validate mode rejects an output path. This must fail..."
rm -f "$FORBIDDEN_OUTPUT"

if ./build/solver "$VALID_OUTPUT" validate "$FORBIDDEN_OUTPUT"; then
    echo "FAILED: validate mode accepted an output JSON path."
    exit 1
fi

if [[ -e "$FORBIDDEN_OUTPUT" ]]; then
    echo "FAILED: validate mode created an output file."
    exit 1
fi

echo "All validation tests passed."
