
#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$ROOT_DIR"

BUILD_DIR="$ROOT_DIR/build"
SOLVER="$BUILD_DIR/solver"

ROUNDTRIP_INPUT="testdata/fixed_nodes_simple.json"
LAYOUT_INPUT="testdata/fixed_nodes_layout_preserve.json"

ROUNDTRIP_OUTPUT="results/fixed_nodes_roundtrip.json"
SUBMISSION_OUTPUT="results/fixed_nodes_submission.json"
GRID_OUTPUT="results/fixed_nodes_grid_preserve.json"
CIRCULAR_OUTPUT="results/fixed_nodes_circular_preserve.json"
FR_OUTPUT="results/fixed_nodes_fr_preserve.json"
SA_OUTPUT="results/fixed_nodes_sa_preserve.json"
SA_INDEX_OUTPUT="results/fixed_nodes_sa_index_preserve.json"

echo "Running fixed node JSON, layout, optimizer, and submission export tests..."

if [[ ! -x "$SOLVER" ]]; then
    echo "ERROR: solver not found or not executable: $SOLVER" >&2
    exit 1
fi

if [[ ! -f "$ROUNDTRIP_INPUT" ]]; then
    echo "ERROR: missing fixed-node roundtrip input: $ROUNDTRIP_INPUT" >&2
    exit 1
fi

if [[ ! -f "$LAYOUT_INPUT" ]]; then
    echo "ERROR: missing fixed-node layout input: $LAYOUT_INPUT" >&2
    exit 1
fi

mkdir -p results

rm -f \
    "$ROUNDTRIP_OUTPUT" \
    "$SUBMISSION_OUTPUT" \
    "$GRID_OUTPUT" \
    "$CIRCULAR_OUTPUT" \
    "$FR_OUTPUT" \
    "$SA_OUTPUT" \
    "$SA_INDEX_OUTPUT"

echo "Validating fixed-node roundtrip input..."
"$SOLVER" "$ROUNDTRIP_INPUT" validate

echo "Writing fixed-node graph through internal solver export..."
"$SOLVER" "$ROUNDTRIP_INPUT" grid "$ROUNDTRIP_OUTPUT" --width 1000 --height 1000

if [[ ! -f "$ROUNDTRIP_OUTPUT" ]]; then
    echo "ERROR: fixed-node roundtrip output was not created: $ROUNDTRIP_OUTPUT" >&2
    exit 1
fi

echo "Validating fixed-node roundtrip output..."
"$SOLVER" "$ROUNDTRIP_OUTPUT" validate

echo "Checking that fixed=true survives internal JSON roundtrip..."
python3 - "$ROUNDTRIP_OUTPUT" <<'PY'
import json
import sys

path = sys.argv[1]

with open(path, "r", encoding="utf-8") as file:
    data = json.load(file)

nodes = data.get("nodes", [])
node_by_id = {node.get("id"): node for node in nodes}

if 0 not in node_by_id:
    raise SystemExit("Missing node id 0 in roundtrip output.")

if node_by_id[0].get("fixed") is not True:
    raise SystemExit("Expected node id 0 to preserve fixed=true.")

if 1 not in node_by_id:
    raise SystemExit("Missing node id 1 in roundtrip output.")

if node_by_id[1].get("fixed", False) is not False:
    raise SystemExit("Expected node id 1 to be non-fixed.")

print("Fixed-node JSON roundtrip preserved fixed flags.")
PY

echo "Writing clean submission export without internal fixed metadata..."
"$SOLVER" "$ROUNDTRIP_OUTPUT" submission "$SUBMISSION_OUTPUT"

if [[ ! -f "$SUBMISSION_OUTPUT" ]]; then
    echo "ERROR: submission output was not created: $SUBMISSION_OUTPUT" >&2
    exit 1
fi

echo "Validating clean submission output..."
"$SOLVER" "$SUBMISSION_OUTPUT" validate

echo "Checking that submission export strips fixed metadata..."
python3 - "$SUBMISSION_OUTPUT" <<'PY'
import json
import sys

path = sys.argv[1]

with open(path, "r", encoding="utf-8") as file:
    data = json.load(file)

for node in data.get("nodes", []):
    if "fixed" in node:
        raise SystemExit(f"Submission output still contains fixed metadata for node {node.get('id')}.")

print("Submission export contains no fixed metadata.")
PY

echo "Validating fixed-node layout input..."
"$SOLVER" "$LAYOUT_INPUT" validate

echo "Running grid layout with fixed node..."
"$SOLVER" "$LAYOUT_INPUT" grid "$GRID_OUTPUT" --width 1000 --height 1000
"$SOLVER" "$GRID_OUTPUT" validate

echo "Running circular layout with fixed node..."
"$SOLVER" "$LAYOUT_INPUT" circular "$CIRCULAR_OUTPUT" --width 1000 --height 1000
"$SOLVER" "$CIRCULAR_OUTPUT" validate

echo "Running FR layout with fixed node..."
"$SOLVER" "$LAYOUT_INPUT" fr "$FR_OUTPUT" --width 1000 --height 1000 --fr-iterations 25
"$SOLVER" "$FR_OUTPUT" validate

echo "Running SA layout with fixed node..."
"$SOLVER" "$LAYOUT_INPUT" sa "$SA_OUTPUT" --width 1000 --height 1000 --seed 12345 --sa-iterations 200
"$SOLVER" "$SA_OUTPUT" validate

echo "Running SA-index layout with fixed node..."
"$SOLVER" "$LAYOUT_INPUT" sa-index "$SA_INDEX_OUTPUT" --width 1000 --height 1000 --seed 12345 --sa-iterations 200
"$SOLVER" "$SA_INDEX_OUTPUT" validate

echo "Checking that fixed node coordinates are preserved by layouts and optimizers..."
python3 - "$LAYOUT_INPUT" "$GRID_OUTPUT" "$CIRCULAR_OUTPUT" "$FR_OUTPUT" "$SA_OUTPUT" "$SA_INDEX_OUTPUT" <<'PY'
import json
import sys

input_path = sys.argv[1]
output_paths = sys.argv[2:]

with open(input_path, "r", encoding="utf-8") as file:
    input_data = json.load(file)

input_nodes = {node["id"]: node for node in input_data["nodes"]}
fixed_node = input_nodes[0]
expected_x = fixed_node["x"]
expected_y = fixed_node["y"]

for output_path in output_paths:
    with open(output_path, "r", encoding="utf-8") as file:
        output_data = json.load(file)

    output_nodes = {node["id"]: node for node in output_data["nodes"]}

    if 0 not in output_nodes:
        raise SystemExit(f"Missing fixed node id 0 in {output_path}")

    node = output_nodes[0]

    if node.get("fixed") is not True:
        raise SystemExit(f"Expected fixed=true for node id 0 in {output_path}")

    if node.get("x") != expected_x or node.get("y") != expected_y:
        raise SystemExit(
            f"Fixed node moved in {output_path}: "
            f"expected ({expected_x}, {expected_y}), "
            f"got ({node.get('x')}, {node.get('y')})"
        )

print("Fixed-node coordinates were preserved by grid, circular, FR, SA, and SA-index.")
PY

echo "All fixed node JSON, layout, optimizer, and submission export tests passed."
