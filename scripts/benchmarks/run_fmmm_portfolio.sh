#!/usr/bin/env bash
# Evaluate the FMMM-in-portfolio integration (--init-candidate) on the 10 real
# Rome/GDT instances. For each instance: generate the FMMM layout once (OGDF,
# deterministic); then run the solver WITH that candidate over several seeds and
# report the best valid k. Writes results_real/rome_fmmm_portfolio.csv.
set -euo pipefail
BUILD=${BUILD:-build}
ROME=${ROME:-instances_converted/rome_real}
SEEDS=${SEEDS:-"1 2 3"}
ITERS=${ITERS:-5000}
OUT=${OUT:-results_real/rome_fmmm_portfolio.csv}
KPY=scripts/benchmarks/_k_of.py
mkdir -p "$(dirname "$OUT")"
echo "graph,n,fmmm_valid,portfolio_best_k" > "$OUT"
for g in $(ls "$ROME"/*.json | sort); do
  n=$(echo "$g" | grep -o '_n[0-9]*_' | tr -dc 0-9)
  fmmm=/tmp/fmmm_${n}.json
  fv=0
  if "$BUILD/ogdf_layout" "$g" "$fmmm" >/dev/null 2>&1 && [ -s "$fmmm" ]; then fv=1; fi
  best=999
  for s in $SEEDS; do
    out=/tmp/port_${n}_${s}.json
    if [ "$fv" = 1 ]; then
      "$BUILD/solver" "$g" sa "$out" --seed "$s" --sa-iterations "$ITERS" --init-candidate "$fmmm" >/dev/null 2>&1
    else
      "$BUILD/solver" "$g" sa "$out" --seed "$s" --sa-iterations "$ITERS" >/dev/null 2>&1
    fi
    if "$BUILD/solver" "$out" validate >/dev/null 2>&1; then
      k=$(python3 "$KPY" "$out" | grep -o 'k=[0-9]*' | tr -dc 0-9)
      [ "$k" -lt "$best" ] && best=$k
    fi
  done
  echo "$(basename "$g"),$n,$fv,$best" | tee -a "$OUT"
done
echo "== written $OUT =="
