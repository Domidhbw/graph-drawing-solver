
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build"
TEST_OUTPUT_DIR="$ROOT_DIR/build/test_outputs/contest_runner_adaptive_probe"

ADAPTIVE_RUNNER="$BUILD_DIR/contest_runner_adaptive_probe"

GRAPH_ONE="$ROOT_DIR/testdata/leaf_reduction_star.json"
GRAPH_TWO="$ROOT_DIR/testdata/k4_convex.json"

SEED="12345"
ADAPTIVE_ROUNDS="2"
ITERATIONS_PER_ROUND="1"

echo "[adaptive_contest_runner_tests] Cleaning output directory"
rm -rf "$TEST_OUTPUT_DIR"
mkdir -p "$TEST_OUTPUT_DIR"

if [[ ! -x "$ADAPTIVE_RUNNER" ]]; then
    echo "[adaptive_contest_runner_tests] Missing executable: $ADAPTIVE_RUNNER" >&2
    exit 1
fi

if [[ ! -x "$BUILD_DIR/solver" ]]; then
    echo "[adaptive_contest_runner_tests] Missing executable: $BUILD_DIR/solver" >&2
    exit 1
fi

if [[ ! -f "$GRAPH_ONE" ]]; then
    echo "[adaptive_contest_runner_tests] Missing test graph: $GRAPH_ONE" >&2
    exit 1
fi

if [[ ! -f "$GRAPH_TWO" ]]; then
    echo "[adaptive_contest_runner_tests] Missing test graph: $GRAPH_TWO" >&2
    exit 1
fi

echo "[adaptive_contest_runner_tests] Running adaptive contest runner probe"
"$ADAPTIVE_RUNNER" \
    "$TEST_OUTPUT_DIR" \
    "$SEED" \
    "$ADAPTIVE_ROUNDS" \
    "$ITERATIONS_PER_ROUND" \
    "$GRAPH_ONE" \
    "$GRAPH_TWO"

EXPECTED_ONE="$TEST_OUTPUT_DIR/0_leaf_reduction_star.json"
EXPECTED_TWO="$TEST_OUTPUT_DIR/1_k4_convex.json"
SUMMARY_CSV="$TEST_OUTPUT_DIR/adaptive_summary.csv"

echo "[adaptive_contest_runner_tests] Checking produced checkpoints"

if [[ ! -f "$EXPECTED_ONE" ]]; then
    echo "[adaptive_contest_runner_tests] Missing checkpoint: $EXPECTED_ONE" >&2
    exit 1
fi

if [[ ! -f "$EXPECTED_TWO" ]]; then
    echo "[adaptive_contest_runner_tests] Missing checkpoint: $EXPECTED_TWO" >&2
    exit 1
fi

if [[ -e "$EXPECTED_ONE.tmp" ]]; then
    echo "[adaptive_contest_runner_tests] Temporary file still exists: $EXPECTED_ONE.tmp" >&2
    exit 1
fi

if [[ -e "$EXPECTED_TWO.tmp" ]]; then
    echo "[adaptive_contest_runner_tests] Temporary file still exists: $EXPECTED_TWO.tmp" >&2
    exit 1
fi

echo "[adaptive_contest_runner_tests] Validating checkpoints"
"$BUILD_DIR/solver" "$EXPECTED_ONE" validate
"$BUILD_DIR/solver" "$EXPECTED_TWO" validate

echo "[adaptive_contest_runner_tests] Checking telemetry CSV"

if [[ ! -f "$SUMMARY_CSV" ]]; then
    echo "[adaptive_contest_runner_tests] Missing telemetry CSV: $SUMMARY_CSV" >&2
    exit 1
fi

python3 - "$SUMMARY_CSV" <<'PY'
import csv
import sys

path = sys.argv[1]

with open(path, "r", encoding="utf-8", newline="") as file:
    rows = list(csv.DictReader(file))

expected_rows = 4
if len(rows) != expected_rows:
    raise SystemExit(f"Expected {expected_rows} telemetry rows, got {len(rows)}.")

required_columns = {
    "round",
    "graph_index",
    "graph",
    "output",
    "resume",
    "iterations",
    "duration_ms",
    "k",
    "crossings",
    "exists",
    "tmp",
    "valid",
    "reason",
}

missing_columns = required_columns.difference(rows[0].keys())
if missing_columns:
    raise SystemExit(f"Missing telemetry columns: {sorted(missing_columns)}")

bootstrap_rows = [row for row in rows if row["reason"] == "bootstrap"]
adaptive_rows = [row for row in rows if row["reason"] == "worst-score"]

if len(bootstrap_rows) != 2:
    raise SystemExit(f"Expected 2 bootstrap rows, got {len(bootstrap_rows)}.")

if len(adaptive_rows) != 2:
    raise SystemExit(f"Expected 2 adaptive rows, got {len(adaptive_rows)}.")

for row in bootstrap_rows:
    if row["resume"] != "no":
        raise SystemExit(f"Expected resume=no for bootstrap row, got: {row}")

for row in adaptive_rows:
    if row["resume"] != "yes":
        raise SystemExit(f"Expected resume=yes for adaptive row, got: {row}")

for row in rows:
    if row["exists"] != "yes":
        raise SystemExit(f"Expected exists=yes, got row: {row}")

    if row["tmp"] != "no":
        raise SystemExit(f"Expected tmp=no, got row: {row}")

    if row["valid"] != "yes":
        raise SystemExit(f"Expected valid=yes, got row: {row}")

    if row["k"] == "":
        raise SystemExit(f"Missing k value in row: {row}")

    if row["crossings"] == "":
        raise SystemExit(f"Missing crossings value in row: {row}")

    if int(row["iterations"]) < 1:
        raise SystemExit(f"Expected iterations >= 1, got row: {row}")

    if int(row["duration_ms"]) < 0:
        raise SystemExit(f"Expected duration_ms >= 0, got row: {row}")

print("Adaptive telemetry CSV is valid.")
PY

echo "[adaptive_contest_runner_tests] Adaptive contest runner tests passed."
