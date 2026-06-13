# Algorithmus

## Ziel

Gesucht ist eine Knotenplatzierung mit gerade gezeichneten Kanten, die

    k = max_e crossings(e)

minimiert – die maximale Zahl an Kreuzungen, an der eine einzelne Kante
beteiligt ist. Bei Gleichstand in `k` entscheidet die Gesamtzahl der Kreuzungen
als sekundäres Kriterium.

## Pipeline

1. **Einlesen** des Graphen aus JSON (`src/io`).
2. **Startlayouts** (`src/layout`): Circular, Random, Grid und
   Fruchterman–Reingold. Optional kommt über OGDF ein FMMM-Layout als weiterer
   Kandidat hinzu (`--init-candidate`).
3. **Optimierung** mit Simulated Annealing (`src/optimization`): In jedem
   Schritt wird ein Knoten verschoben und die Änderung von `k` bewertet.
   Verschlechterungen werden mit temperaturabhängiger Wahrscheinlichkeit
   akzeptiert; die Temperatur sinkt geometrisch. Das beste bisher gesehene
   gültige Layout wird festgehalten.
4. **Validierung** (`src/validation`): Ein Layout ist nur gültig, wenn keine
   zwei Knoten zusammenfallen und kein Knoten auf einer fremden Kante liegt.
   Ungültige Zwischenlösungen werden nie als Ergebnis übernommen
   (Validierungs-Gate). Ein einfacher Repair-Schritt versucht, kleinere
   Verletzungen zu beheben.
5. **Ausgabe** des besten gültigen Layouts als JSON.

## Bewertung der Kreuzungen

Die Kreuzungszahl wird geometrisch über echte Segmentschnitte bestimmt
(`src/geometry`). Damit nicht nach jedem Zug alle Kantenpaare neu geprüft werden
müssen, pflegt der `IncrementalCrossingIndex` die Statistik inkrementell: Nur
die Kanten am bewegten Knoten ändern sich, der Rest bleibt gültig. Das senkt die
Kosten pro Zug deutlich und macht größere Instanzen zugänglich.

## Vorverarbeitung

Blätter (Knoten vom Grad 1) tragen nie zu Kreuzungen bei. Der `LeafReducer`
entfernt sie vor der Optimierung und der `LeafReinserter` fügt sie danach wieder
ein. Das verkleinert die eigentliche Suchaufgabe ohne Qualitätsverlust.

## CLI-Modi

`solver <graph.json> <modus> [output.json] [optionen]` – die wichtigsten Modi
sind `sa` (Optimierung), `compare` (Startlayouts gegenüberstellen) und
`validate`. Alle Läufe sind über `--seed` reproduzierbar.
