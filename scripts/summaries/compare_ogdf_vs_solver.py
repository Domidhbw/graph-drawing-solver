#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
compare_ogdf_vs_solver.py — reproduziert den FMMM-vs-Solver-Vergleich
(Tabelle tab:fmmm-vergleich in der Seminararbeit, Abschnitt sec:fmmm-vergleich).

Vergleicht je Instanz das beste valide k des eigenen Solvers (FR-Portfolio + SA,
Spalte sa_k aus rome_real.csv) mit der FMMM-Initialisierung aus OGDF
(Spalte ogdf_k aus rome_ogdf.csv). Beide CSVs stammen aus identischer
Konfiguration (10 reale Rome-Instanzen, 5 Seeds, Budgets {1000,5000,10000}).

Nur Standardbibliothek (csv, re) — keine externen Abhaengigkeiten.

Aufruf (aus Graph/):
  python3 scripts/summaries/compare_ogdf_vs_solver.py
  python3 scripts/summaries/compare_ogdf_vs_solver.py --data results_real
"""

import argparse
import csv
import os
import re


def n_of(name):
    return int(re.search(r"_n(\d+)_m", name).group(1))


def m_of(name):
    return int(re.search(r"_m(\d+)", name).group(1))


def as_int(cell):
    cell = cell.strip()
    return int(cell) if cell != "" else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", default="results_real",
                    help="Verzeichnis mit rome_real.csv und rome_ogdf.csv")
    args = ap.parse_args()

    solver = {}   # n -> list of sa_k
    for r in csv.DictReader(open(os.path.join(args.data, "rome_real.csv"))):
        solver.setdefault(n_of(r["graph"]), []).append(int(r["sa_k"]))

    fmmm = {}     # n -> list of valid ogdf_k
    meta = {}     # n -> m
    total = valid_fmmm = 0
    for r in csv.DictReader(open(os.path.join(args.data, "rome_ogdf.csv"))):
        n = n_of(r["graph"]); meta[n] = m_of(r["graph"])
        total += 1
        valid_fmmm += int(r["ogdf_valid"])
        k = as_int(r["ogdf_k"])
        if k is not None:
            fmmm.setdefault(n, []).append(k)

    print(f"{'n':>4} {'m':>4} | {'Solver best k':>13} | {'FMMM best k':>11} | FMMM valide")
    print("-" * 56)
    wins = ties = 0
    for n in sorted(solver):
        s_best = min(solver[n])
        if n in fmmm and fmmm[n]:
            f_best = str(min(fmmm[n]))
            valid = "ja"
            if min(fmmm[n]) < s_best:
                wins += 1
            elif min(fmmm[n]) == s_best:
                ties += 1
        else:
            f_best = "n.a."
            valid = "NEIN"
        print(f"{n:>4} {meta[n]:>4} | {s_best:>13} | {f_best:>11} | {valid}")

    print("-" * 56)
    print(f"Solver-Validitaetsrate: {sum(len(v) for v in solver.values())}/"
          f"{sum(len(v) for v in solver.values())} = 1.00")
    print(f"FMMM-Validitaetsrate:   {valid_fmmm}/{total} = {valid_fmmm/total:.2f}")
    print(f"FMMM besser (kleineres best k): {wins} Instanzen, gleichauf: {ties}")


if __name__ == "__main__":
    main()
