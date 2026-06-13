
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build"
TEST_OUTPUT_DIR="$ROOT_DIR/build/test_outputs/contest_runner_timeboxed_probe"

TIMEBOXED_RUNNER="$BUILD_DIR/contest_runner_timeboxed_probe"

GRAPH_ONE="$ROOT_DIR/testdata/leaf_reduction_star.json"
GRAPH_TWO="$ROOT_DIR/testdata/k4_convex.json"

SEED="12345"
TOTAL_SECONDS="1"

echo "[timeboxed_contest_runner_tests] Cleaning output directory"
rm -rf "$TEST_OUTPUT_DIR"
mkdir -p "$TEST_OUTPUT_DIR"

if [[ ! -x "$TIMEBOXED_RUNNER" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing executable: $TIMEBOXED_RUNNER" >&2
    exit 1
fi

if [[ ! -x "$BUILD_DIR/solver" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing executable: $BUILD_DIR/solver" >&2
    exit 1
fi

if [[ ! -f "$GRAPH_ONE" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing test graph: $GRAPH_ONE" >&2
    exit 1
fi

if [[ ! -f "$GRAPH_TWO" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing test graph: $GRAPH_TWO" >&2
    exit 1
fi

echo "[timeboxed_contest_runner_tests] Running initial timeboxed contest runner probe"
"$TIMEBOXED_RUNNER" "$TEST_OUTPUT_DIR" "$SEED" "$TOTAL_SECONDS" "$GRAPH_ONE" "$GRAPH_TWO"

EXPECTED_ONE="$TEST_OUTPUT_DIR/0_leaf_reduction_star.json"
EXPECTED_TWO="$TEST_OUTPUT_DIR/1_k4_convex.json"
SUMMARY_CSV="$TEST_OUTPUT_DIR/timeboxed_summary.csv"

echo "[timeboxed_contest_runner_tests] Checking produced checkpoints after initial run"

if [[ ! -f "$EXPECTED_ONE" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing checkpoint: $EXPECTED_ONE" >&2
    exit 1
fi

if [[ ! -f "$EXPECTED_TWO" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing checkpoint: $EXPECTED_TWO" >&2
    exit 1
fi

if [[ -e "$EXPECTED_ONE.tmp" ]]; then
    echo "[timeboxed_contest_runner_tests] Temporary file still exists: $EXPECTED_ONE.tmp" >&2
    exit 1
fi

if [[ -e "$EXPECTED_TWO.tmp" ]]; then
    echo "[timeboxed_contest_runner_tests] Temporary file still exists: $EXPECTED_TWO.tmp" >&2
    exit 1
fi

echo "[timeboxed_contest_runner_tests] Validating checkpoints after initial run"
"$BUILD_DIR/solver" "$EXPECTED_ONE" validate
"$BUILD_DIR/solver" "$EXPECTED_TWO" validate

echo "[timeboxed_contest_runner_tests] Running timeboxed contest runner probe with --resume"
RESUME_LOG="$(
    "$TIMEBOXED_RUNNER" "$TEST_OUTPUT_DIR" "$SEED" "$TOTAL_SECONDS" "$GRAPH_ONE" "$GRAPH_TWO" --resume
)"

echo "$RESUME_LOG"

echo "$RESUME_LOG" | grep -q "resume=yes"

RESUME_COUNT="$(printf '%s\n' "$RESUME_LOG" | grep -c "Resumed from checkpoint: yes" || true)"
if [[ "$RESUME_COUNT" -lt 2 ]]; then
    echo "[timeboxed_contest_runner_tests] Expected at least 2 resumed checkpoints, got $RESUME_COUNT" >&2
    exit 1
fi

echo "[timeboxed_contest_runner_tests] Checking produced checkpoints after resume run"

if [[ ! -f "$EXPECTED_ONE" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing checkpoint after resume: $EXPECTED_ONE" >&2
    exit 1
fi

if [[ ! -f "$EXPECTED_TWO" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing checkpoint after resume: $EXPECTED_TWO" >&2
    exit 1
fi

if [[ -e "$EXPECTED_ONE.tmp" ]]; then
    echo "[timeboxed_contest_runner_tests] Temporary file still exists after resume: $EXPECTED_ONE.tmp" >&2
    exit 1
fi

if [[ -e "$EXPECTED_TWO.tmp" ]]; then
    echo "[timeboxed_contest_runner_tests] Temporary file still exists after resume: $EXPECTED_TWO.tmp" >&2
    exit 1
fi

echo "[timeboxed_contest_runner_tests] Validating checkpoints after resume run"
"$BUILD_DIR/solver" "$EXPECTED_ONE" validate
"$BUILD_DIR/solver" "$EXPECTED_TWO" validate

echo "[timeboxed_contest_runner_tests] Checking telemetry CSV"

if [[ ! -f "$SUMMARY_CSV" ]]; then
    echo "[timeboxed_contest_runner_tests] Missing telemetry CSV: $SUMMARY_CSV" >&2
    exit 1
fi

python3 - "$SUMMARY_CSV" <<'PY'
import csv
import sys

path = sys.argv[1]

with open(path, "r", encoding="utf-8", newline="") as file:
    rows = list(csv.DictReader(file))

if len(rows) != 2:
    raise SystemExit(f"Expected 2 telemetry rows, got {len(rows)}.")

required_columns = {
    "graph",
    "output",
    "allocated_seconds",
    "iterations",
    "duration_ms",
    "resume",
    "k",
    "crossings",
    "exists",
    "tmp",
    "valid",
}

missing_columns = required_columns.difference(rows[0].keys())
if missing_columns:
    raise SystemExit(f"Missing telemetry columns: {sorted(missing_columns)}")

for row in rows:
    if row["resume"] != "yes":
        raise SystemExit(f"Expected resume=yes in telemetry after resume run, got row: {row}")

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

print("Telemetry CSV is valid.")
PY

echo "[timeboxed_contest_runner_tests] Timeboxed contest runner tests passed."
