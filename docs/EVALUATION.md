# Auswertung

Getestet wurde auf zehn Graphen aus dem Rome-Datensatz mit 10 bis 100 Knoten,
je 15 Läufe pro Instanz und festen Seeds. Die vollständigen Zahlen liegen in
`results/rome_real.csv` (Rohwerte) und `results/rome_real_summary.csv`
(Zusammenfassung).

## Wichtigste Beobachtungen

**Simulated Annealing liefert gültige Layouts mit niedrigem k.** Auf allen zehn
Instanzen war die Validitätsrate 1,00. Das beste erreichte `k` bleibt durchweg
klein und wächst nur langsam mit der Größe – zum Beispiel `k = 0` bei 10 und 20
Knoten, `k = 2` bei 40 Knoten und `k = 3` bei 100 Knoten.

**Das Circular-Startlayout ist als Baseline deutlich schlechter.** Auf dem
40-Knoten-Graphen etwa liegt es bei `k = 27`, während Simulated Annealing auf
`k = 2` kommt; bei 100 Knoten stehen 63 gegen 3. Die Optimierung trägt also den
Großteil der Verbesserung.

**Fruchterman–Reingold erreicht zwar niedrige Kreuzungszahlen, aber selten
gültige Layouts.** In der Messung lag die Validitätsrate von FR auf den meisten
Instanzen bei 0,00 (Knoten landen zu dicht beieinander oder auf Kanten). Genau
deshalb gibt es das Validierungs-Gate: Ein optisch gutes, aber ungültiges Layout
wird nicht als Ergebnis akzeptiert. FR (und FMMM) sind daher als *Startkandidat*
hinter dem Gate wertvoll, nicht als alleinige Lösung.

## Laufzeit

Die Laufzeit steigt mit der Knoten- und Kantenzahl an, weil mehr Kandidatenzüge
nötig sind und jeder Zug mehr Kreuzungen berührt. Die inkrementelle
Kreuzungsbewertung (siehe `docs/ALGORITHM.md`) hält die Kosten pro Zug niedrig,
sodass auch die größeren Instanzen in praktikabler Zeit laufen.

## Reproduktion

Nach dem Konvertieren der Rome-Daten ins JSON-Format (`scripts/conversion/`)
lassen sich die Ergebnisse mit `scripts/benchmarks/run_real_benchmarks.sh`
nachrechnen.
