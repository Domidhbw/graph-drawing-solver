
#!/usr/bin/env bash
set -euo pipefail

OUTPUT_DIR="${OUTPUT_DIR:-results}"
OUTPUT_FILE="${OUTPUT_FILE:-$OUTPUT_DIR/core_instances.csv}"
SUMMARY_FILE="${SUMMARY_FILE:-$OUTPUT_DIR/core_instances_summary.csv}"

SEED_VALUES="${SEEDS:-1 2 3 4 5}"
SA_ITERATION_VALUES="${SA_ITERATIONS:-1000 5000 10000}"
FR_ITERATION_VALUE="${FR_ITERATIONS:-500}"

INCLUDE_ADVERSARIAL="${INCLUDE_ADVERSARIAL:-0}"

RUN_EXPERIMENTS_SCRIPT="./scripts/benchmarks/run_experiments.sh"
SUMMARY_SCRIPT="scripts/summaries/summarize_experiments.py"

GRAPH_ROOTS=(
    "instances_converted/rome"
    "instances_converted/random"
    "instances_converted/structured"
)

if [[ "$INCLUDE_ADVERSARIAL" == "1" ]]; then
    GRAPH_ROOTS+=("instances_converted/adversarial")
fi

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

if [[ ! -x "$RUN_EXPERIMENTS_SCRIPT" ]]; then
    echo "ERROR: $RUN_EXPERIMENTS_SCRIPT not found or not executable."
    exit 1
fi

if [[ ! -f "$SUMMARY_SCRIPT" ]]; then
    echo "ERROR: $SUMMARY_SCRIPT not found."
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

GRAPH_FILES_ARRAY=()

for root in "${GRAPH_ROOTS[@]}"; do
    if [[ ! -d "$root" ]]; then
        echo "WARNING: graph root does not exist, skipping: $root"
        continue
    fi

    while IFS= read -r graph_file; do
        GRAPH_FILES_ARRAY+=("$graph_file")
    done < <(find "$root" -name "*.json" | sort)
done

if [[ "${#GRAPH_FILES_ARRAY[@]}" -eq 0 ]]; then
    echo "ERROR: no graph files found."
    echo "Checked roots:"
    for root in "${GRAPH_ROOTS[@]}"; do
        echo "  $root"
    done
    exit 1
fi

GRAPH_FILES_VALUE="$(printf "%s " "${GRAPH_FILES_ARRAY[@]}")"

echo "Running core benchmark"
echo "Output CSV:      $OUTPUT_FILE"
echo "Summary CSV:     $SUMMARY_FILE"
echo "Seeds:           $SEED_VALUES"
echo "SA iterations:   $SA_ITERATION_VALUES"
echo "FR iterations:   $FR_ITERATION_VALUE"
echo "Graph count:     ${#GRAPH_FILES_ARRAY[@]}"
echo "Include adversarial: $INCLUDE_ADVERSARIAL"
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
echo "Core benchmark completed."
echo "Results: $OUTPUT_FILE"
echo "Summary: $SUMMARY_FILE"
