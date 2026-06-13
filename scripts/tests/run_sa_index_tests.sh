
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

SA_INDEX_OUTPUT="results/sa_index_triangle.json"
SA_INDEX_ROME_OUTPUT="results/sa_index_rome_g15_e20.json"

echo "Running SA index smoke test on triangle..."
./build/solver \
    testdata/triangle_no_crossing.json \
    sa-index \
    "$SA_INDEX_OUTPUT" \
    --seed 1 \
    --sa-iterations 100 \
    > /dev/null

./build/solver "$SA_INDEX_OUTPUT" validate

echo "Running SA index smoke test on example graph..."
./build/solver \
    examples/random_sparse_n20.json \
    sa-index \
    "$SA_INDEX_ROME_OUTPUT" \
    --seed 1 \
    --sa-iterations 100 \
    > /dev/null

./build/solver "$SA_INDEX_ROME_OUTPUT" validate

echo "Checking that sa-index appears in usage for invalid invocation..."
USAGE_OUTPUT="$(
    ./build/solver testdata/triangle_no_crossing.json unknown-layout 2>&1 || true
)"

if echo "$USAGE_OUTPUT" | grep -q "sa-index"; then
    echo "Usage contains sa-index."
else
    echo "FAILED: usage does not mention sa-index."
    echo "$USAGE_OUTPUT"
    exit 1
fi

echo "All SA index tests passed."
