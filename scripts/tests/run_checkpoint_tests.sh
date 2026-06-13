
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/checkpoint_probe" ]]; then
    echo "ERROR: ./build/checkpoint_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

mkdir -p results

VALID_CHECKPOINT="results/checkpoint_triangle.json"
INVALID_CHECKPOINT="results/checkpoint_invalid_should_not_exist.json"

rm -f "$VALID_CHECKPOINT" "$VALID_CHECKPOINT.tmp"
rm -f "$INVALID_CHECKPOINT" "$INVALID_CHECKPOINT.tmp"

echo "Checkpointing valid graph..."
./build/checkpoint_probe \
    testdata/triangle_no_crossing.json \
    "$VALID_CHECKPOINT"

echo "Validating checkpoint..."
./build/solver "$VALID_CHECKPOINT" validate

if [[ -e "$VALID_CHECKPOINT.tmp" ]]; then
    echo "FAILED: temporary checkpoint file still exists."
    exit 1
fi

echo "Checking that invalid graph is not checkpointed..."
if ./build/checkpoint_probe \
    testdata/invalid_duplicate_coordinates.json \
    "$INVALID_CHECKPOINT"; then
    echo "FAILED: invalid graph was checkpointed."
    exit 1
fi

if [[ -e "$INVALID_CHECKPOINT" ]]; then
    echo "FAILED: invalid checkpoint output was written."
    exit 1
fi

if [[ -e "$INVALID_CHECKPOINT.tmp" ]]; then
    echo "FAILED: invalid checkpoint temporary file was not cleaned up."
    exit 1
fi

echo "All checkpoint tests passed."
