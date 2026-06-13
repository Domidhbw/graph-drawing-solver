
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build"
TEST_OUTPUT_DIR="$ROOT_DIR/build/test_outputs/contest_runner_probe"

CONTEST_RUNNER="$BUILD_DIR/contest_runner_probe"

GRAPH_ONE="$ROOT_DIR/testdata/leaf_reduction_star.json"
GRAPH_TWO="$ROOT_DIR/testdata/leaf_reduction_star.json"

SEED="12345"
ITERATIONS="50"

echo "[contest_runner_tests] Cleaning output directory"
rm -rf "$TEST_OUTPUT_DIR"
mkdir -p "$TEST_OUTPUT_DIR"

if [[ ! -x "$CONTEST_RUNNER" ]]; then
    echo "[contest_runner_tests] Missing executable: $CONTEST_RUNNER" >&2
    exit 1
fi

if [[ ! -f "$GRAPH_ONE" ]]; then
    echo "[contest_runner_tests] Missing test graph: $GRAPH_ONE" >&2
    exit 1
fi

echo "[contest_runner_tests] Running contest runner probe"
"$CONTEST_RUNNER" "$TEST_OUTPUT_DIR" "$SEED" "$ITERATIONS" "$GRAPH_ONE" "$GRAPH_TWO"

EXPECTED_ONE="$TEST_OUTPUT_DIR/0_leaf_reduction_star.json"
EXPECTED_TWO="$TEST_OUTPUT_DIR/1_leaf_reduction_star.json"

echo "[contest_runner_tests] Checking produced checkpoints"

if [[ ! -f "$EXPECTED_ONE" ]]; then
    echo "[contest_runner_tests] Missing checkpoint: $EXPECTED_ONE" >&2
    exit 1
fi

if [[ ! -f "$EXPECTED_TWO" ]]; then
    echo "[contest_runner_tests] Missing checkpoint: $EXPECTED_TWO" >&2
    exit 1
fi

if [[ -e "$EXPECTED_ONE.tmp" ]]; then
    echo "[contest_runner_tests] Temporary file still exists: $EXPECTED_ONE.tmp" >&2
    exit 1
fi

if [[ -e "$EXPECTED_TWO.tmp" ]]; then
    echo "[contest_runner_tests] Temporary file still exists: $EXPECTED_TWO.tmp" >&2
    exit 1
fi

echo "[contest_runner_tests] Validating checkpoints"
"$BUILD_DIR/solver" "$EXPECTED_ONE" validate
"$BUILD_DIR/solver" "$EXPECTED_TWO" validate

echo "[contest_runner_tests] Contest runner probe tests passed."
