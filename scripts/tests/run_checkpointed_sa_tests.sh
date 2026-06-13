
#!/usr/bin/env bash
set -euo pipefail

if [[ ! -x "./build/checkpointed_sa_probe" ]]; then
    echo "ERROR: ./build/checkpointed_sa_probe not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    exit 1
fi

mkdir -p results

TRIANGLE_OUTPUT="results/checkpointed_sa_triangle.json"
ROME_OUTPUT="results/checkpointed_sa_rome_g15_e20.json"
RESUME_OUTPUT="results/checkpointed_sa_resume.json"

rm -f "$TRIANGLE_OUTPUT" "$TRIANGLE_OUTPUT.tmp"
rm -f "$ROME_OUTPUT" "$ROME_OUTPUT.tmp"
rm -f "$RESUME_OUTPUT" "$RESUME_OUTPUT.tmp"

echo "Running checkpointed SA on triangle..."
TRIANGLE_LOG="$(
    ./build/checkpointed_sa_probe \
        testdata/triangle_no_crossing.json \
        "$TRIANGLE_OUTPUT" \
        1 \
        100
)"

echo "$TRIANGLE_LOG"

echo "$TRIANGLE_LOG" | grep -q "Initial candidate:"
echo "$TRIANGLE_LOG" | grep -q "Selected initial layout:"

./build/solver "$TRIANGLE_OUTPUT" validate

if [[ -e "$TRIANGLE_OUTPUT.tmp" ]]; then
    echo "FAILED: triangle temporary checkpoint file still exists."
    exit 1
fi

echo "Running checkpointed SA on Rome graph..."
ROME_LOG="$(
    ./build/checkpointed_sa_probe \
        instances_converted/rome/rome_g15_e20.json \
        "$ROME_OUTPUT" \
        1 \
        100
)"

echo "$ROME_LOG"

echo "$ROME_LOG" | grep -q "Initial candidate:"
echo "$ROME_LOG" | grep -q "Selected initial layout:"

./build/solver "$ROME_OUTPUT" validate

if [[ -e "$ROME_OUTPUT.tmp" ]]; then
    echo "FAILED: Rome temporary checkpoint file still exists."
    exit 1
fi

echo "Creating checkpoint for resume test..."
FIRST_RESUME_LOG="$(
    ./build/checkpointed_sa_probe \
        testdata/k4_convex.json \
        "$RESUME_OUTPUT" \
        7 \
        50
)"

echo "$FIRST_RESUME_LOG"

echo "$FIRST_RESUME_LOG" | grep -q "Resume: no"

./build/solver "$RESUME_OUTPUT" validate

if [[ -e "$RESUME_OUTPUT.tmp" ]]; then
    echo "FAILED: resume temporary checkpoint file still exists after first run."
    exit 1
fi

echo "Continuing from checkpoint with --resume..."
SECOND_RESUME_LOG="$(
    ./build/checkpointed_sa_probe \
        testdata/k4_convex.json \
        "$RESUME_OUTPUT" \
        7 \
        0 \
        --resume
)"

echo "$SECOND_RESUME_LOG"

echo "$SECOND_RESUME_LOG" | grep -q "Resume: yes"
echo "$SECOND_RESUME_LOG" | grep -q "Resumed from checkpoint: yes"

./build/solver "$RESUME_OUTPUT" validate

if [[ -e "$RESUME_OUTPUT.tmp" ]]; then
    echo "FAILED: resume temporary checkpoint file still exists after resume run."
    exit 1
fi

echo "All checkpointed SA tests passed."
