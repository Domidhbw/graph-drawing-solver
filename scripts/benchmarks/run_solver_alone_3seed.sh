#!/usr/bin/env bash
# Solver-alone (no FMMM candidate) under the SAME 3-seed/5000-iter protocol as
# run_fmmm_portfolio.sh, so the two columns are a fair A/B comparison.
set -euo pipefail
ROME=instances_converted/rome_real
OUT=results_real/rome_solver_alone_3seed.csv
echo "graph,n,solver_alone_best_k" > "$OUT"
for g in $(ls "$ROME"/*.json | sort); do
  n=$(echo "$g" | grep -o '_n[0-9]*_' | tr -dc 0-9)
  best=999
  for s in 1 2 3; do
    o=/tmp/sa_${n}_${s}.json
    ./build/solver "$g" sa "$o" --seed "$s" --sa-iterations 5000 >/dev/null 2>&1
    if ./build/solver "$o" validate >/dev/null 2>&1; then
      k=$(python3 scripts/benchmarks/_k_of.py "$o" | grep -o 'k=[0-9]*' | tr -dc 0-9)
      [ "$k" -lt "$best" ] && best=$k
    fi
  done
  echo "$(basename "$g"),$n,$best" | tee -a "$OUT"
done
