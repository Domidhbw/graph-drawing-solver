#!/usr/bin/env bash
# Smoke test (<= 10 min) for the FMMM-in-portfolio integration (--init-candidate).
# MUST pass before any full evaluation or any "umgesetzt" claim in the thesis.
# Run inside the OGDF devcontainer. Scripts may have CRLF -> run via: bash <(tr -d '\r' < this).
set -euo pipefail

BUILD=${BUILD:-build}
ROME=${ROME:-instances_converted/rome_real}
SOLVER="$BUILD/solver"
OGDF="$BUILD/ogdf_layout"

# n=60: FMMM is valid and reaches k=1 (solver alone: k=3) -> candidate should win.
# n=90: FMMM produces an invalid/no layout -> portfolio must fall back, stay valid.
G60=$(ls "$ROME"/*n60_* | head -1)
G90=$(ls "$ROME"/*n90_* | head -1)

echo "== building =="
cmake --build "$BUILD" --target solver ogdf_layout >/dev/null

run() {  # graph  label
  local g="$1" label="$2"
  "$OGDF" "$g" /tmp/fmmm_$label.json >/tmp/ogdf_$label.log 2>&1 || true
  echo "-- $label: FMMM tool exit logged to /tmp/ogdf_$label.log"
  # Baseline (no candidate) and portfolio (with candidate), seed 1, 5000 iter.
  "$SOLVER" "$g" sa /tmp/base_$label.json --seed 1 --sa-iterations 5000 >/tmp/base_$label.log 2>&1
  if [ -s /tmp/fmmm_$label.json ]; then
    "$SOLVER" "$g" sa /tmp/port_$label.json --seed 1 --sa-iterations 5000 \
      --init-candidate /tmp/fmmm_$label.json >/tmp/port_$label.log 2>&1
  else
    echo "   (no FMMM layout produced -> portfolio == baseline by construction)"
    cp /tmp/base_$label.json /tmp/port_$label.json
  fi
  echo "   baseline   k: $(grep -o '"maxCrossingsPerEdge":[0-9]*' /tmp/base_$label.json | head -1)"
  echo "   portfolio  k: $(grep -o '"maxCrossingsPerEdge":[0-9]*' /tmp/port_$label.json | head -1)"
  "$SOLVER" /tmp/port_$label.json validate >/tmp/val_$label.log 2>&1 \
    && echo "   portfolio VALID" || { echo "   portfolio INVALID -> FAIL"; exit 1; }
}

run "$G60" n60
run "$G90" n90

echo "== PASS: portfolio output valid on both; inspect n60 for k improvement (expect <= baseline) =="
