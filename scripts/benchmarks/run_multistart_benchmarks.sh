#!/usr/bin/env bash
#
# Multi-start benchmark for the real Rome/GDT instances.
#
# Compares single-pass SA against multi-start SA at the SAME total iteration
# budget: the budget is split across N independent passes (--restarts N) and the
# global best is kept. This isolates the effect of restart diversity on the mean
# k and on the best-vs-mean spread observed in the parameter study.
#
# Prerequisites: build/ contains `checkpointed_sa_probe` and `solver`, and
# instances_converted/rome_real/ contains the converted GDT instances.
#
# Usage:
#   ./scripts/benchmarks/run_multistart_benchmarks.sh
#   TOTAL_ITERS=10000 RESTARTS_SET="1 5 10" SEEDS="1 2 3" \
#     ./scripts/benchmarks/run_multistart_benchmarks.sh

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build}"
GRAPH_DIR="${GRAPH_DIR:-instances_converted/rome_real}"
OUTPUT_CSV="${OUTPUT_CSV:-results_real/rome_multistart.csv}"
WORK_DIR="${WORK_DIR:-results_real/multistart_tmp}"
TOTAL_ITERS="${TOTAL_ITERS:-10000}"
RESTARTS_SET="${RESTARTS_SET:-1 5 10}"
SEEDS="${SEEDS:-1 2 3}"

PROBE="$BUILD_DIR/checkpointed_sa_probe"
SOLVER="$BUILD_DIR/solver"

if [[ ! -x "$PROBE" ]]; then
  echo "ERROR: missing executable: $PROBE (build the project first)" >&2
  exit 1
fi
if [[ ! -x "$SOLVER" ]]; then
  echo "ERROR: missing executable: $SOLVER (build the project first)" >&2
  exit 1
fi
if [[ ! -d "$GRAPH_DIR" ]]; then
  echo "ERROR: graph root does not exist: $GRAPH_DIR" >&2
  exit 1
fi

mkdir -p "$(dirname "$OUTPUT_CSV")" "$WORK_DIR"
echo "graph,n,m,seed,restarts,total_iters,k,crossings,valid,duration_ms" > "$OUTPUT_CSV"

extract_value() { # <output> <label>
  printf '%s\n' "$1" | sed -n "s/^$2: \\([0-9][0-9]*\\)$/\\1/p" | head -n1
}

for graph in $(ls "$GRAPH_DIR"/*.json | sort); do
  base="$(basename "$graph")"
  n="$(printf '%s' "$base" | sed -n 's/.*_n\([0-9][0-9]*\)_.*/\1/p')"
  m="$(printf '%s' "$base" | sed -n 's/.*_m\([0-9][0-9]*\)\.json/\1/p')"

  for restarts in $RESTARTS_SET; do
    for seed in $SEEDS; do
      out="$WORK_DIR/${base%.json}_s${seed}_r${restarts}.json"
      rm -f "$out" "$out.tmp"

      start_ns="$(date +%s%N)"
      if "$PROBE" "$graph" "$out" "$seed" "$TOTAL_ITERS" --restarts "$restarts" >/dev/null 2>&1; then
        end_ns="$(date +%s%N)"
        duration_ms=$(( (end_ns - start_ns) / 1000000 ))
        score_out="$("$SOLVER" "$out")"
        k="$(extract_value "$score_out" K)"
        crossings="$(extract_value "$score_out" Crossings)"
        echo "$base,$n,$m,$seed,$restarts,$TOTAL_ITERS,${k:-NA},${crossings:-NA},yes,$duration_ms" >> "$OUTPUT_CSV"
        echo "OK  n=$n seed=$seed restarts=$restarts k=${k:-NA} ${duration_ms}ms"
      else
        end_ns="$(date +%s%N)"
        duration_ms=$(( (end_ns - start_ns) / 1000000 ))
        echo "$base,$n,$m,$seed,$restarts,$TOTAL_ITERS,NA,NA,no,$duration_ms" >> "$OUTPUT_CSV"
        echo "FAIL n=$n seed=$seed restarts=$restarts" >&2
      fi
    done
  done
done

echo "Wrote $OUTPUT_CSV"
