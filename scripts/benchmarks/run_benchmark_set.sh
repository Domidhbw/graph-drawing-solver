#!/usr/bin/env bash
set -euo pipefail

BENCHMARK_SET="${BENCHMARK_SET:-benchmark_sets/smoke.txt}"
OUTPUT_DIR="${OUTPUT_DIR:-results/benchmark_sets}"
NAME="${NAME:-$(basename "$BENCHMARK_SET" .txt)}"

OUTPUT_FILE="${OUTPUT_FILE:-$OUTPUT_DIR/${NAME}.csv}"
SUMMARY_FILE="${SUMMARY_FILE:-$OUTPUT_DIR/${NAME}_summary.csv}"
RUNTIME_SUMMARY_FILE="${RUNTIME_SUMMARY_FILE:-$OUTPUT_DIR/${NAME}_runtime.csv}"
BUDGET_SUMMARY_FILE="${BUDGET_SUMMARY_FILE:-$OUTPUT_DIR/${NAME}_by_budget.csv}"

SEED_VALUES="${SEEDS:-1 2 3}"
SA_ITERATION_VALUES="${SA_ITERATIONS:-1000 5000}"
FR_ITERATION_VALUE="${FR_ITERATIONS:-500}"

RUN_EXPERIMENTS_SCRIPT="./scripts/benchmarks/run_experiments.sh"
SUMMARY_SCRIPT="scripts/summaries/summarize_experiments.py"
RUNTIME_SUMMARY_SCRIPT="scripts/summaries/summarize_runtime_scaling.py"
BUDGET_SUMMARY_SCRIPT="scripts/summaries/summarize_experiments_by_budget.py"

print_usage() {
    cat <<USAGE
Usage:
  BENCHMARK_SET=benchmark_sets/smoke.txt $0

Environment:
  BENCHMARK_SET=<file>       default: benchmark_sets/smoke.txt
  OUTPUT_DIR=<dir>           default: results/benchmark_sets
  NAME=<name>                default: basename of BENCHMARK_SET
  SEEDS="<values>"           default: 1 2 3
  SA_ITERATIONS="<values>"   default: 1000 5000
  FR_ITERATIONS=<value>      default: 500

Examples:
  $0
  BENCHMARK_SET=benchmark_sets/core_small.txt $0
  BENCHMARK_SET=benchmark_sets/core_medium.txt SEEDS="1 2 3 4 5" SA_ITERATIONS="1000 5000 10000" $0
USAGE
}

read_benchmark_set() {
    local benchmark_set="$1"
    local -n output_array="$2"

    output_array=()

    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line%%#*}"

        # trim leading whitespace
        line="${line#"${line%%[![:space:]]*}"}"
        # trim trailing whitespace
        line="${line%"${line##*[![:space:]]}"}"

        if [[ -z "$line" ]]; then
            continue
        fi

        output_array+=("$line")
    done < "$benchmark_set"
}

validate_prerequisites() {
    if [[ ! -f "$BENCHMARK_SET" ]]; then
        echo "ERROR: benchmark set not found: $BENCHMARK_SET" >&2
        exit 1
    fi

    if [[ ! -x "./build/solver" ]]; then
        echo "ERROR: ./build/solver not found or not executable." >&2
        echo "Run:" >&2
        echo "  cmake -S . -B build -G Ninja" >&2
        echo "  cmake --build build" >&2
        exit 1
    fi

    if [[ ! -x "$RUN_EXPERIMENTS_SCRIPT" ]]; then
        echo "ERROR: $RUN_EXPERIMENTS_SCRIPT not found or not executable." >&2
        exit 1
    fi

    for script in "$SUMMARY_SCRIPT" "$RUNTIME_SUMMARY_SCRIPT" "$BUDGET_SUMMARY_SCRIPT"; do
        if [[ ! -f "$script" ]]; then
            echo "ERROR: summary script not found: $script" >&2
            exit 1
        fi
    done
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    print_usage
    exit 0
fi

validate_prerequisites

declare -a GRAPH_FILES_ARRAY
read_benchmark_set "$BENCHMARK_SET" GRAPH_FILES_ARRAY

if [[ "${#GRAPH_FILES_ARRAY[@]}" -eq 0 ]]; then
    echo "ERROR: benchmark set contains no graph files: $BENCHMARK_SET" >&2
    exit 1
fi

for graph_file in "${GRAPH_FILES_ARRAY[@]}"; do
    if [[ ! -f "$graph_file" ]]; then
        echo "ERROR: graph listed in benchmark set does not exist: $graph_file" >&2
        exit 1
    fi
done

mkdir -p "$OUTPUT_DIR"

GRAPH_FILES_VALUE="$(printf "%s " "${GRAPH_FILES_ARRAY[@]}")"

echo "Running benchmark set"
echo "Benchmark set:      $BENCHMARK_SET"
echo "Name:               $NAME"
echo "Output CSV:         $OUTPUT_FILE"
echo "Summary CSV:        $SUMMARY_FILE"
echo "Runtime CSV:        $RUNTIME_SUMMARY_FILE"
echo "Budget CSV:         $BUDGET_SUMMARY_FILE"
echo "Seeds:              $SEED_VALUES"
echo "SA iterations:      $SA_ITERATION_VALUES"
echo "FR iterations:      $FR_ITERATION_VALUE"
echo "Graph count:        ${#GRAPH_FILES_ARRAY[@]}"
echo

GRAPH_FILES="$GRAPH_FILES_VALUE" \
SEEDS="$SEED_VALUES" \
SA_ITERATIONS="$SA_ITERATION_VALUES" \
FR_ITERATIONS="$FR_ITERATION_VALUE" \
OUTPUT_FILE="$OUTPUT_FILE" \
"$RUN_EXPERIMENTS_SCRIPT"

echo
echo "Creating summary CSV..."
python3 "$SUMMARY_SCRIPT" "$OUTPUT_FILE" --output-csv "$SUMMARY_FILE"

echo
echo "Creating runtime summary CSV..."
python3 "$RUNTIME_SUMMARY_SCRIPT" "$OUTPUT_FILE" --output-csv "$RUNTIME_SUMMARY_FILE"

echo
echo "Creating budget summary CSV..."
python3 "$BUDGET_SUMMARY_SCRIPT" "$OUTPUT_FILE" --output-csv "$BUDGET_SUMMARY_FILE"

echo
echo "Benchmark set completed."
echo "Results:         $OUTPUT_FILE"
echo "Summary:         $SUMMARY_FILE"
echo "Runtime summary: $RUNTIME_SUMMARY_FILE"
echo "Budget summary:  $BUDGET_SUMMARY_FILE"
