#!/usr/bin/env bash
#
# Run the solver over the real Rome benchmark subset and emit result CSVs.
# Mirrors run_core_benchmarks.sh (same seeds / SA iterations / summary path) but
# points at instances_converted/rome_real so the numbers stay comparable.
#
# Prerequisite: datasets downloaded and converted, i.e.
#   python3 scripts/datasets/download_rome_real.py
#   python3 scripts/conversion/convert_edgelist_to_json.py \
#       --input-dir datasets/rome_real --output-dir instances_converted/rome_real
#
set -euo pipefail

OUTPUT_DIR="${OUTPUT_DIR:-results_real}"
OUTPUT_FILE="${OUTPUT_FILE:-$OUTPUT_DIR/rome_real.csv}"
SUMMARY_FILE="${SUMMARY_FILE:-$OUTPUT_DIR/rome_real_summary.csv}"

SEED_VALUES="${SEEDS:-1 2 3 4 5}"
SA_ITERATION_VALUES="${SA_ITERATIONS:-1000 5000 10000}"
FR_ITERATION_VALUE="${FR_ITERATIONS:-500}"

GRAPH_ROOT="${GRAPH_ROOT:-instances_converted/rome_real}"

RUN_EXPERIMENTS_SCRIPT="./scripts/benchmarks/run_experiments.sh"
SUMMARY_SCRIPT="scripts/summaries/summarize_experiments.py"

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:  cmake -S . -B build -G Ninja && cmake --build build"
    exit 1
fi

if [[ ! -x "$RUN_EXPERIMENTS_SCRIPT" ]]; then
    echo "ERROR: $RUN_EXPERIMENTS_SCRIPT not found or not executable."
    exit 1
fi

if [[ ! -d "$GRAPH_ROOT" ]]; then
    echo "ERROR: graph root does not exist: $GRAPH_ROOT"
    echo "Did you run the download + convert steps (see header of this script)?"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

GRAPH_FILES_ARRAY=()
while IFS= read -r graph_file; do
    GRAPH_FILES_ARRAY+=("$graph_file")
done < <(find "$GRAPH_ROOT" -name "*.json" | sort)

if [[ "${#GRAPH_FILES_ARRAY[@]}" -eq 0 ]]; then
    echo "ERROR: no .json graphs found under $GRAPH_ROOT"
    exit 1
fi

GRAPH_FILES_VALUE="$(printf "%s " "${GRAPH_FILES_ARRAY[@]}")"

echo "Running real Rome benchmark"
echo "Graph root:    $GRAPH_ROOT"
echo "Graph count:   ${#GRAPH_FILES_ARRAY[@]}"
echo "Output CSV:    $OUTPUT_FILE"
echo "Summary CSV:   $SUMMARY_FILE"
echo "Seeds:         $SEED_VALUES"
echo "SA iterations: $SA_ITERATION_VALUES"
echo "FR iterations: $FR_ITERATION_VALUE"
echo

GRAPH_FILES="$GRAPH_FILES_VALUE" \
SEEDS="$SEED_VALUES" \
SA_ITERATIONS="$SA_ITERATION_VALUES" \
FR_ITERATIONS="$FR_ITERATION_VALUE" \
OUTPUT_FILE="$OUTPUT_FILE" \
"$RUN_EXPERIMENTS_SCRIPT"

echo
echo "Creating summary..."
python3 "$SUMMARY_SCRIPT" "$OUTPUT_FILE" --output-csv "$SUMMARY_FILE"

echo
echo "Real Rome benchmark completed."
echo "Results: $OUTPUT_FILE"
echo "Summary: $SUMMARY_FILE"
