# Graph Drawing Solver

Ein C++20-Solver, der die Knotenpositionen eines Graphen so optimiert, dass die
maximale Anzahl Kreuzungen pro Kante minimal wird:

    k = max_e crossings(e)   ->   min

Der Solver liest einen Graphen als JSON, erzeugt mehrere Startlayouts
(Circular, Random, Grid, Fruchterman–Reingold, optional FMMM über OGDF),
verbessert sie mit Simulated Annealing hinter einem Validierungs-Gate und
schreibt das beste **gültige** Layout zurück als JSON.

## Build

Voraussetzungen: CMake ≥ 3.20, Ninja, ein C++20-Compiler, `nlohmann_json`.
OGDF ist optional und wird nur für die FMMM-Layouts gebraucht.

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

Eine fertige Toolchain liegt unter `.devcontainer/` (VS Code / Docker).

## Benutzung

```bash
# Simulated Annealing, Ergebnis nach result.json
build/solver examples/random_sparse_n20.json sa result.json --seed 123 --sa-iterations 50000

# Startlayouts vergleichen
build/solver examples/cube_q3.json compare --seed 123 --fr-iterations 1000

# Mit externem Startkandidaten (z. B. ein FMMM-Layout)
build/solver graph.json sa result.json --init-candidate fmmm.json
```

Optionen: `--seed`, `--width`, `--height`, `--fr-iterations`,
`--sa-iterations`, `--init-candidate`.

JSON-Format (Eingabe):

```json
{
  "nodes": [ { "id": 0, "x": 0.0, "y": 0.0 } ],
  "edges": [ { "source": 0, "target": 1 } ]
}
```

## Tests

```bash
scripts/tests/run_all_tests.sh
```

## Projektstruktur

- `src/` – Solverkern: Geometrie, Layouts, SA-Optimierung, Validierung, IO
- `scripts/` – Tests, Datensatz-Konvertierung, Benchmarks, Visualisierung
- `testdata/`, `examples/` – Test-Fixtures und kleine Beispielgraphen
- `results/` – Messergebnisse auf den Rome-Instanzen (siehe `docs/EVALUATION.md`)
- `docs/` – Kurzbeschreibung von Algorithmus und Auswertung

## Datensätze

Die [Rome-/GDT-Testsuiten](https://graphdrawing.unipg.it/data.html) sind nicht Teil des Repositorys. Sie stammen vom
offiziellen Graph-Drawing-Datensatz, mit den Skripten unter
`scripts/conversion/` lassen sie sich ins erwartete JSON-Format bringen, danach
können die Benchmark-Skripte unter `scripts/benchmarks/` darauf laufen.


