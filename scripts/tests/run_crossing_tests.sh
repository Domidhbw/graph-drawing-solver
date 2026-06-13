
#!/usr/bin/env bash
set -euo pipefail

run_test() {
    local file="$1"
    local expected="$2"

    local output

    if ! output="$(./build/solver "$file" 2>&1)"; then
        echo "FAILED: $file"
        echo "Solver exited with an error:"
        echo "$output"
        exit 1
    fi

    local actual
    actual="$(echo "$output" | awk '/Crossings:/ { print $2 }')"

    if [[ -z "$actual" ]]; then
        echo "FAILED: $file"
        echo "Could not parse Crossings from solver output."
        echo "$output"
        exit 1
    fi

    if [[ "$actual" != "$expected" ]]; then
        echo "FAILED: $file"
        echo "Expected crossings: $expected"
        echo "Actual crossings:   $actual"
        echo "$output"
        exit 1
    fi

    echo "OK: $file -> $actual"
}

run_test "testdata/no_crossing_2_edges.json" 0
run_test "testdata/one_crossing_2_edges.json" 1
run_test "testdata/shared_endpoint_no_crossing.json" 0
run_test "testdata/triangle_no_crossing.json" 0
run_test "testdata/square_diagonals_one_crossing.json" 1
run_test "testdata/k4_convex.json" 1
run_test "testdata/collinear_overlap.json" 1
run_test "testdata/disconnected_components.json" 1

echo "All crossing tests passed."
