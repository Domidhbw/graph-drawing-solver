#!/usr/bin/env bash
set -euo pipefail

echo "Running full project test suite"
echo "==============================="
echo

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found."
    exit 1
fi

if ! command -v ninja >/dev/null 2>&1; then
    echo "ERROR: ninja not found."
    exit 1
fi

echo "Configuring project..."
cmake -S . -B build -G Ninja

echo
echo "Building project..."
cmake --build build

echo
echo "Running test scripts..."
echo

run_test_script_if_present() {
    local script_path="$1"

    if [[ ! -f "$script_path" ]]; then
        echo "SKIP: $script_path not found."
        return
    fi

    if [[ ! -x "$script_path" ]]; then
        echo "Making executable: $script_path"
        chmod +x "$script_path"
    fi

    echo
    echo "------------------------------------------------------------"
    echo "RUN: $script_path"
    echo "------------------------------------------------------------"
    "$script_path"
}

run_test_script_if_present "./scripts/tests/run_crossing_tests.sh"
run_test_script_if_present "./scripts/tests/run_validation_tests.sh"
run_test_script_if_present "./scripts/tests/run_repair_tests.sh"
run_test_script_if_present "./scripts/tests/run_delta_tests.sh"
run_test_script_if_present "./scripts/tests/run_incremental_index_tests.sh"
run_test_script_if_present "./scripts/tests/run_preprocessing_tests.sh"
run_test_script_if_present "./scripts/tests/run_sa_index_tests.sh"
run_test_script_if_present "./scripts/tests/run_checkpoint_tests.sh"
run_test_script_if_present "./scripts/tests/run_best_solution_store_tests.sh"
run_test_script_if_present "./scripts/tests/run_checkpointed_sa_tests.sh"
run_test_script_if_present "./scripts/tests/run_contest_runner_tests.sh"
run_test_script_if_present "./scripts/tests/run_fixed_node_tests.sh"
run_test_script_if_present "./scripts/tests/run_timeboxed_contest_runner_tests.sh"
run_test_script_if_present "./scripts/tests/run_adaptive_contest_runner_tests.sh"

echo
echo "Running optional binary smoke checks..."
echo

if [[ -x "./build/ogdf_probe" ]]; then
    echo "RUN: ./build/ogdf_probe"
    ./build/ogdf_probe
else
    echo "SKIP: ./build/ogdf_probe not found or not executable."
fi

echo
echo "==============================="
echo "All available tests passed."
echo "==============================="
