
#!/usr/bin/env bash
set -euo pipefail

OUTPUT_FILE="${OUTPUT_FILE:-results/experiments.csv}"
FR_ITERATIONS="${FR_ITERATIONS:-500}"

read -ra SEED_VALUES <<< "${SEEDS:-1 2 3}"
read -ra SA_ITERATION_VALUES <<< "${SA_ITERATIONS:-1000 5000 10000}"

if [[ ! -x "./build/solver" ]]; then
    echo "ERROR: ./build/solver not found or not executable."
    echo "Run:"
    echo "  cmake -S . -B build -G Ninja"
    echo "  cmake --build build"
    exit 1
fi

mkdir -p "$(dirname "$OUTPUT_FILE")"

TEMP_OUTPUT_DIR="$(dirname "$OUTPUT_FILE")/experiment_layout_outputs"
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

create_and_validate_layout() {
    local graph_file="$1"
    local layout_name="$2"
    local seed="$3"
    local sa_iterations="$4"

    local sanitized_graph
    sanitized_graph="$(sanitize_path_for_filename "$graph_file")"

    local output_path
    output_path="$TEMP_OUTPUT_DIR/${sanitized_graph}_${layout_name}_seed_${seed}_sa_${sa_iterations}.json"

    rm -f "$output_path"

    if ! ./build/solver "$graph_file" "$layout_name" "$output_path" \
        --seed "$seed" \
        --fr-iterations "$FR_ITERATIONS" \
        --sa-iterations "$sa_iterations" \
        > /dev/null 2>&1; then
        echo "0"
        return
    fi

    if [[ ! -f "$output_path" ]]; then
        echo "0"
        return
    fi

    if ./build/solver "$output_path" validate > /dev/null 2>&1; then
        echo "1"
    else
        echo "0"
    fi
}

run_experiment() {
    local graph_file="$1"
    local seed="$2"
    local sa_iterations="$3"

    local start_ms
    local end_ms
    local duration_ms
    local output

    start_ms="$(date +%s%3N)"

    output="$(
        ./build/solver "$graph_file" compare \
            --seed "$seed" \
            --fr-iterations "$FR_ITERATIONS" \
            --sa-iterations "$sa_iterations"
    )"

    local original_valid
    local random_valid
    local circular_valid
    local grid_valid
    local fr_valid
    local sa_valid
    local sa_uniform_valid

    original_valid="$(validate_existing_graph "$graph_file")"
    random_valid="$(create_and_validate_layout "$graph_file" "random" "$seed" "$sa_iterations")"
    circular_valid="$(create_and_validate_layout "$graph_file" "circular" "$seed" "$sa_iterations")"
    grid_valid="$(create_and_validate_layout "$graph_file" "grid" "$seed" "$sa_iterations")"
    fr_valid="$(create_and_validate_layout "$graph_file" "fr" "$seed" "$sa_iterations")"
    sa_valid="$(create_and_validate_layout "$graph_file" "sa" "$seed" "$sa_iterations")"
    sa_uniform_valid="$(create_and_validate_layout "$graph_file" "sa-uniform" "$seed" "$sa_iterations")"

    end_ms="$(date +%s%3N)"
    duration_ms=$((end_ms - start_ms))

    local original_k
    local original_crossings
    local random_k
    local random_crossings
    local circular_k
    local circular_crossings
    local grid_k
    local grid_crossings
    local fr_k
    local fr_crossings
    local sa_k
    local sa_crossings
    local sa_uniform_k
    local sa_uniform_crossings

    original_k="$(extract_value "Original k" "$output")"
    original_crossings="$(extract_value "Original crossings" "$output")"

    random_k="$(extract_value "Random k" "$output")"
    random_crossings="$(extract_value "Random crossings" "$output")"

    circular_k="$(extract_value "Circular k" "$output")"
    circular_crossings="$(extract_value "Circular crossings" "$output")"

    grid_k="$(extract_value "Grid k" "$output")"
    grid_crossings="$(extract_value "Grid crossings" "$output")"

    fr_k="$(extract_value "FR k" "$output")"
    fr_crossings="$(extract_value "FR crossings" "$output")"

    sa_k="$(extract_value "SA k" "$output")"
    sa_crossings="$(extract_value "SA crossings" "$output")"

    sa_uniform_k="$(extract_value "SA uniform k" "$output")"
    sa_uniform_crossings="$(extract_value "SA uniform crossings" "$output")"

    require_value "Original k" "$original_k" "$graph_file" "$output"
    require_value "Original crossings" "$original_crossings" "$graph_file" "$output"

    require_value "Random k" "$random_k" "$graph_file" "$output"
    require_value "Random crossings" "$random_crossings" "$graph_file" "$output"

    require_value "Circular k" "$circular_k" "$graph_file" "$output"
    require_value "Circular crossings" "$circular_crossings" "$graph_file" "$output"

    require_value "Grid k" "$grid_k" "$graph_file" "$output"
    require_value "Grid crossings" "$grid_crossings" "$graph_file" "$output"

    require_value "FR k" "$fr_k" "$graph_file" "$output"
    require_value "FR crossings" "$fr_crossings" "$graph_file" "$output"

    require_value "SA k" "$sa_k" "$graph_file" "$output"
    require_value "SA crossings" "$sa_crossings" "$graph_file" "$output"

    require_value "SA uniform k" "$sa_uniform_k" "$graph_file" "$output"
    require_value "SA uniform crossings" "$sa_uniform_crossings" "$graph_file" "$output"

    echo "$graph_file,$seed,$FR_ITERATIONS,$sa_iterations,$original_k,$original_crossings,$original_valid,$random_k,$random_crossings,$random_valid,$circular_k,$circular_crossings,$circular_valid,$grid_k,$grid_crossings,$grid_valid,$fr_k,$fr_crossings,$fr_valid,$sa_k,$sa_crossings,$sa_valid,$sa_uniform_k,$sa_uniform_crossings,$sa_uniform_valid,$duration_ms" >> "$OUTPUT_FILE"

    echo "OK: graph=$graph_file seed=$seed sa_iterations=$sa_iterations sa_k=$sa_k sa_uniform_k=$sa_uniform_k sa_valid=$sa_valid sa_uniform_valid=$sa_uniform_valid duration_ms=$duration_ms"
}

echo "graph,seed,fr_iterations,sa_iterations,original_k,original_crossings,original_valid,random_k,random_crossings,random_valid,circular_k,circular_crossings,circular_valid,grid_k,grid_crossings,grid_valid,fr_k,fr_crossings,fr_valid,sa_k,sa_crossings,sa_valid,sa_uniform_k,sa_uniform_crossings,sa_uniform_valid,duration_ms" > "$OUTPUT_FILE"

for graph_file in "${GRAPH_VALUES[@]}"; do
    for seed in "${SEED_VALUES[@]}"; do
        for sa_iterations in "${SA_ITERATION_VALUES[@]}"; do
            run_experiment "$graph_file" "$seed" "$sa_iterations"
        done
    done
done

echo
echo "Experiment results written to: $OUTPUT_FILE"
