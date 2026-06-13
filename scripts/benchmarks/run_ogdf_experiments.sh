
#!/usr/bin/env bash
set -euo pipefail

OUTPUT_FILE="${OUTPUT_FILE:-results/ogdf_experiments.csv}"

read -ra SEED_VALUES <<< "${SEEDS:-1 2 3}"
read -ra SA_ITERATION_VALUES <<< "${SA_ITERATIONS:-1000}"

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

if [[ ! -x "./build/ogdf_layout" ]]; then
    echo "ERROR: ./build/ogdf_layout not found or not executable."
    echo "OGDF is optional. Make sure OGDF is installed and CMake built ogdf_layout."
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"

TEMP_OUTPUT_DIR="$(dirname "$OUTPUT_FILE")/ogdf_experiment_layout_outputs"
mkdir -p "$TEMP_OUTPUT_DIR"

if [[ -n "${GRAPH_FILES:-}" ]]; then
    read -ra GRAPH_VALUES <<< "$GRAPH_FILES"
else
    mapfile -t GRAPH_VALUES < <(find testdata -maxdepth 1 -name "*.json" | sort)
fi

extract_value() {
    local label="$1"
    local output="$2"

    echo "$output" | awk -F': ' -v key="$label" '$1 == key { print $2 }'
}

require_value() {
    local name="$1"
    local value="$2"
    local graph_file="$3"
    local output="$4"

    if [[ -z "$value" ]]; then
        echo "FAILED to parse '$name' for $graph_file"
        echo "Solver output:"
        echo "$output"
        exit 1
    fi
}

sanitize_path_for_filename() {
    local value="$1"

    echo "$value" | sed 's#[^A-Za-z0-9._-]#_#g'
}

validate_existing_graph() {
    local graph_file="$1"

    if ./build/solver "$graph_file" validate > /dev/null 2>&1; then
        echo "1"
    else
        echo "0"
    fi
}

read_k_and_crossings() {
    local graph_file="$1"
    local prefix="$2"

    local output
    output="$(./build/solver "$graph_file")"

    local k
    local crossings

    k="$(extract_value "K" "$output")"
    crossings="$(extract_value "Crossings" "$output")"

    require_value "${prefix}_k" "$k" "$graph_file" "$output"
    require_value "${prefix}_crossings" "$crossings" "$graph_file" "$output"

    echo "$k,$crossings"
}

run_experiment() {
    local graph_file="$1"
    local seed="$2"
    local sa_iterations="$3"

    local start_ms
    local end_ms
    local duration_ms

    start_ms="$(date +%s%3N)"

    local sanitized_graph
    sanitized_graph="$(sanitize_path_for_filename "$graph_file")"

    local ogdf_output
    local ogdf_sa_output

    ogdf_output="$TEMP_OUTPUT_DIR/${sanitized_graph}_ogdf_seed_${seed}_sa_${sa_iterations}.json"
    ogdf_sa_output="$TEMP_OUTPUT_DIR/${sanitized_graph}_ogdf_sa_seed_${seed}_sa_${sa_iterations}.json"

    rm -f "$ogdf_output" "$ogdf_sa_output"

    local ogdf_valid="0"
    local ogdf_k=""
    local ogdf_crossings=""

    if ./build/ogdf_layout "$graph_file" "$ogdf_output" > /dev/null 2>&1; then
        ogdf_valid="$(validate_existing_graph "$ogdf_output")"

        if [[ "$ogdf_valid" == "1" ]]; then
            IFS=',' read -r ogdf_k ogdf_crossings <<< "$(read_k_and_crossings "$ogdf_output" "ogdf")"
        fi
    fi

    local ogdf_sa_valid="0"
    local ogdf_sa_k=""
    local ogdf_sa_crossings=""

    if [[ "$ogdf_valid" == "1" ]]; then
        if ./build/solver "$ogdf_output" sa "$ogdf_sa_output" \
            --seed "$seed" \
            --sa-iterations "$sa_iterations" \
            > /dev/null 2>&1; then

            ogdf_sa_valid="$(validate_existing_graph "$ogdf_sa_output")"

            if [[ "$ogdf_sa_valid" == "1" ]]; then
                IFS=',' read -r ogdf_sa_k ogdf_sa_crossings <<< "$(read_k_and_crossings "$ogdf_sa_output" "ogdf_sa")"
            fi
        fi
    fi

    end_ms="$(date +%s%3N)"
    duration_ms=$((end_ms - start_ms))

    echo "$graph_file,$seed,$sa_iterations,$ogdf_k,$ogdf_crossings,$ogdf_valid,$ogdf_sa_k,$ogdf_sa_crossings,$ogdf_sa_valid,$duration_ms" >> "$OUTPUT_FILE"

    echo "OK: graph=$graph_file seed=$seed sa_iterations=$sa_iterations ogdf_k=${ogdf_k:-NA} ogdf_valid=$ogdf_valid ogdf_sa_k=${ogdf_sa_k:-NA} ogdf_sa_valid=$ogdf_sa_valid duration_ms=$duration_ms"
}

echo "graph,seed,sa_iterations,ogdf_k,ogdf_crossings,ogdf_valid,ogdf_sa_k,ogdf_sa_crossings,ogdf_sa_valid,duration_ms" > "$OUTPUT_FILE"

for graph_file in "${GRAPH_VALUES[@]}"; do
    for seed in "${SEED_VALUES[@]}"; do
        for sa_iterations in "${SA_ITERATION_VALUES[@]}"; do
            run_experiment "$graph_file" "$seed" "$sa_iterations"
        done
    done
done

echo
echo "OGDF experiment results written to: $OUTPUT_FILE"
