
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build"
CONTEST_RUNNER="$BUILD_DIR/contest_runner_probe"
SOLVER="$BUILD_DIR/solver"

OUTPUT_DIR="${OUTPUT_DIR:-$ROOT_DIR/build/benchmark_outputs/contest_skeleton}"
RESULTS_CSV="${RESULTS_CSV:-$OUTPUT_DIR/contest_skeleton_results.csv}"
SUMMARY_CSV="${SUMMARY_CSV:-$OUTPUT_DIR/contest_skeleton_runtime_summary.csv}"
SUMMARY_SCRIPT="$ROOT_DIR/scripts/summaries/summarize_runtime_scaling.py"

SEED="${SEED:-12345}"
ITERATIONS="${ITERATIONS:-200}"

declare -a GRAPHS=()

print_usage() {
    cat <<EOF
Usage:
  $0 [graph1.json graph2.json ...]

Environment:
  SEED=<seed>                default: 12345
  ITERATIONS=<iterations>    default: 200
  OUTPUT_DIR=<dir>           default: build/benchmark_outputs/contest_skeleton
  RESULTS_CSV=<file>         default: <OUTPUT_DIR>/contest_skeleton_results.csv
  SUMMARY_CSV=<file>         default: <OUTPUT_DIR>/contest_skeleton_runtime_summary.csv

Examples:
  $0
  SEED=42 ITERATIONS=500 $0
  $0 testdata/leaf_reduction_star.json testdata/k4_convex.json
EOF
}

collect_default_graphs() {
    mapfile -t GRAPHS < <(
        find "$ROOT_DIR/testdata" -maxdepth 1 -type f -name "*.json" \
            ! -name "invalid_*.json" \
            | sort \
            | head -n 5
    )

    if [[ "${#GRAPHS[@]}" -eq 0 ]]; then
        echo "[contest_skeleton_benchmark] No JSON files found in testdata/" >&2
        exit 1
    fi

    if [[ "${#GRAPHS[@]}" -lt 3 ]]; then
        echo "[contest_skeleton_benchmark] Fewer than 3 test graphs found. Reusing available graphs to create a small 3-instance benchmark."

        local original_count="${#GRAPHS[@]}"
        local index=0

        while [[ "${#GRAPHS[@]}" -lt 3 ]]; do
            GRAPHS+=("${GRAPHS[$((index % original_count))]}")
            index=$((index + 1))
        done
    fi
}

collect_cli_graphs() {
    for graph in "$@"; do
        if [[ "$graph" = /* ]]; then
            GRAPHS+=("$graph")
        else
            GRAPHS+=("$ROOT_DIR/$graph")
        fi
    done
}

validate_prerequisites() {
    if [[ ! -x "$CONTEST_RUNNER" ]]; then
        echo "[contest_skeleton_benchmark] Missing executable: $CONTEST_RUNNER" >&2
        echo "[contest_skeleton_benchmark] Run first:" >&2
        echo "  cmake -S . -B build -G Ninja" >&2
        echo "  cmake --build build" >&2
        exit 1
    fi

    if [[ ! -x "$SOLVER" ]]; then
        echo "[contest_skeleton_benchmark] Missing executable: $SOLVER" >&2
        echo "[contest_skeleton_benchmark] Run first:" >&2
        echo "  cmake -S . -B build -G Ninja" >&2
        echo "  cmake --build build" >&2
        exit 1
    fi

    if [[ ! -f "$SUMMARY_SCRIPT" ]]; then
        echo "[contest_skeleton_benchmark] Missing summary script: $SUMMARY_SCRIPT" >&2
        exit 1
    fi

    for graph in "${GRAPHS[@]}"; do
        if [[ ! -f "$graph" ]]; then
            echo "[contest_skeleton_benchmark] Missing graph: $graph" >&2
            exit 1
        fi
    done
}

print_configuration() {
    echo "[contest_skeleton_benchmark] Configuration"
    echo "  root:        $ROOT_DIR"
    echo "  runner:      $CONTEST_RUNNER"
    echo "  solver:      $SOLVER"
    echo "  output_dir:  $OUTPUT_DIR"
    echo "  results_csv: $RESULTS_CSV"
    echo "  summary_csv: $SUMMARY_CSV"
    echo "  seed:        $SEED"
    echo "  iterations:  $ITERATIONS"
    echo "  graphs:      ${#GRAPHS[@]}"

    local index=0
    for graph in "${GRAPHS[@]}"; do
        echo "    [$index] $graph"
        index=$((index + 1))
    done
}

make_output_path_for_graph() {
    local index="$1"
    local graph="$2"
    local file_name
    local stem

    file_name="$(basename "$graph")"
    stem="${file_name%.*}"

    if [[ -z "$stem" ]]; then
        stem="graph"
    fi

    printf '%s/%s_%s.json' "$OUTPUT_DIR" "$index" "$stem"
}

extract_value() {
    local label="$1"
    local output="$2"

    echo "$output" | awk -F': ' -v key="$label" '$1 == key { print $2 }'
}

require_value() {
    local name="$1"
    local value="$2"
    local file="$3"
    local output="$4"

    if [[ -z "$value" ]]; then
        echo "[contest_skeleton_benchmark] Failed to parse '$name' for $file" >&2
        echo "[contest_skeleton_benchmark] Solver output:" >&2
        echo "$output" >&2
        exit 1
    fi
}

score_checkpoint() {
    local output_file="$1"
    local compare_output
    local checkpoint_k
    local checkpoint_crossings

    compare_output="$("$SOLVER" "$output_file" compare --seed "$SEED" --sa-iterations 0)"

    checkpoint_k="$(extract_value "Original k" "$compare_output")"
    checkpoint_crossings="$(extract_value "Original crossings" "$compare_output")"

    require_value "Original k" "$checkpoint_k" "$output_file" "$compare_output"
    require_value "Original crossings" "$checkpoint_crossings" "$output_file" "$compare_output"

    printf '%s,%s' "$checkpoint_k" "$checkpoint_crossings"
}

validate_checkpoint() {
    local output_file="$1"

    if "$SOLVER" "$output_file" validate > /dev/null 2>&1; then
        echo "1"
    else
        echo "0"
    fi
}

run_benchmark() {
    rm -rf "$OUTPUT_DIR"
    mkdir -p "$OUTPUT_DIR"

    local start_ms
    local end_ms
    local duration_ms

    start_ms="$(date +%s%3N)"

    "$CONTEST_RUNNER" "$OUTPUT_DIR" "$SEED" "$ITERATIONS" "${GRAPHS[@]}"

    end_ms="$(date +%s%3N)"
    duration_ms=$((end_ms - start_ms))

    write_results_csv "$duration_ms"
}

write_results_csv() {
    local total_duration_ms="$1"
    local graph_count="${#GRAPHS[@]}"
    local duration_per_graph_ms

    if [[ "$graph_count" -eq 0 ]]; then
        duration_per_graph_ms=0
    else
        duration_per_graph_ms=$((total_duration_ms / graph_count))
    fi

    echo "[contest_skeleton_benchmark] Writing result CSV: $RESULTS_CSV"

    echo "graph,seed,sa_iterations,sa_k,sa_crossings,sa_valid,duration_ms,output_file" > "$RESULTS_CSV"

    local index=0
    for graph in "${GRAPHS[@]}"; do
        local output_file
        local tmp_file
        local valid
        local score
        local checkpoint_k
        local checkpoint_crossings

        output_file="$(make_output_path_for_graph "$index" "$graph")"
        tmp_file="${output_file}.tmp"

        if [[ ! -f "$output_file" ]]; then
            echo "[contest_skeleton_benchmark] Missing output file: $output_file" >&2
            exit 1
        fi

        if [[ -e "$tmp_file" ]]; then
            echo "[contest_skeleton_benchmark] Temporary file still exists: $tmp_file" >&2
            exit 1
        fi

        valid="$(validate_checkpoint "$output_file")"
        score="$(score_checkpoint "$output_file")"

        checkpoint_k="${score%,*}"
        checkpoint_crossings="${score#*,}"

        echo "$graph,$SEED,$ITERATIONS,$checkpoint_k,$checkpoint_crossings,$valid,$duration_per_graph_ms,$output_file" >> "$RESULTS_CSV"

        echo "[contest_skeleton_benchmark] CSV row: graph=$graph k=$checkpoint_k crossings=$checkpoint_crossings valid=$valid output=$output_file"

        index=$((index + 1))
    done
}

write_runtime_summary() {
    echo
    echo "[contest_skeleton_benchmark] Creating runtime summary: $SUMMARY_CSV"

    python3 "$SUMMARY_SCRIPT" "$RESULTS_CSV" --output-csv "$SUMMARY_CSV"
}

print_result_overview() {
    echo
    echo "[contest_skeleton_benchmark] Produced outputs"
    find "$OUTPUT_DIR" -maxdepth 1 -type f -name "*.json" | sort

    echo
    echo "[contest_skeleton_benchmark] Results CSV"
    cat "$RESULTS_CSV"

    echo
    echo "[contest_skeleton_benchmark] Runtime summary CSV"
    cat "$SUMMARY_CSV"

    echo
    echo "[contest_skeleton_benchmark] Benchmark completed successfully."
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    print_usage
    exit 0
fi

if [[ "$#" -gt 0 ]]; then
    collect_cli_graphs "$@"
else
    collect_default_graphs
fi

validate_prerequisites
print_configuration
run_benchmark
write_runtime_summary
print_result_overview
