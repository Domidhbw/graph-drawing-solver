
#!/usr/bin/env python3

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from statistics import mean


@dataclass
class ExperimentRow:
    graph: str
    seed: int
    fr_iterations: int
    sa_iterations: int

    random_k: int
    circular_k: int
    grid_k: int
    fr_k: int
    sa_k: int

    sa_crossings: int
    sa_valid: bool
    duration_ms: int


@dataclass
class BudgetSummary:
    graph: str
    sa_iterations: int
    runs: int

    best_random_k: int
    best_circular_k: int
    best_grid_k: int
    best_fr_k: int
    best_sa_k: int

    avg_random_k: float
    avg_circular_k: float
    avg_grid_k: float
    avg_fr_k: float
    avg_sa_k: float

    best_baseline_k: int
    improvement_vs_fr_best_k: int
    improvement_vs_best_baseline_k: int

    best_sa_crossings: int
    avg_sa_crossings: float
    sa_valid_rate: float
    avg_duration_ms: float


def parse_int(row: dict[str, str], column: str) -> int:
    value = row.get(column)

    if value is None or value == "":
        raise ValueError(f"Missing column value: {column}")

    return int(value)


def parse_bool(row: dict[str, str], column: str) -> bool:
    value = row.get(column)

    if value == "1":
        return True

    if value == "0":
        return False

    raise ValueError(f"Invalid boolean value for {column}: {value}")


def parse_row(row: dict[str, str]) -> ExperimentRow:
    return ExperimentRow(
        graph=row["graph"],
        seed=parse_int(row, "seed"),
        fr_iterations=parse_int(row, "fr_iterations"),
        sa_iterations=parse_int(row, "sa_iterations"),

        random_k=parse_int(row, "random_k"),
        circular_k=parse_int(row, "circular_k"),
        grid_k=parse_int(row, "grid_k"),
        fr_k=parse_int(row, "fr_k"),
        sa_k=parse_int(row, "sa_k"),

        sa_crossings=parse_int(row, "sa_crossings"),
        sa_valid=parse_bool(row, "sa_valid"),
        duration_ms=parse_int(row, "duration_ms"),
    )


def load_rows(path: Path) -> list[ExperimentRow]:
    rows: list[ExperimentRow] = []

    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)

        for row in reader:
            rows.append(parse_row(row))

    if not rows:
        raise ValueError(f"No rows found in {path}")

    return rows


def group_by_graph_and_budget(
    rows: list[ExperimentRow]
) -> dict[tuple[str, int], list[ExperimentRow]]:
    grouped: dict[tuple[str, int], list[ExperimentRow]] = {}

    for row in rows:
        key = (row.graph, row.sa_iterations)
        grouped.setdefault(key, []).append(row)

    return grouped


def bool_rate(values: list[bool]) -> float:
    if not values:
        return 0.0

    return mean(1.0 if value else 0.0 for value in values)


def summarize_group(
    graph: str,
    sa_iterations: int,
    rows: list[ExperimentRow]
) -> BudgetSummary:
    best_random_k = min(row.random_k for row in rows)
    best_circular_k = min(row.circular_k for row in rows)
    best_grid_k = min(row.grid_k for row in rows)
    best_fr_k = min(row.fr_k for row in rows)
    best_sa_k = min(row.sa_k for row in rows)

    best_baseline_k = min(
        best_random_k,
        best_circular_k,
        best_grid_k,
        best_fr_k,
    )

    return BudgetSummary(
        graph=graph,
        sa_iterations=sa_iterations,
        runs=len(rows),

        best_random_k=best_random_k,
        best_circular_k=best_circular_k,
        best_grid_k=best_grid_k,
        best_fr_k=best_fr_k,
        best_sa_k=best_sa_k,

        avg_random_k=mean(row.random_k for row in rows),
        avg_circular_k=mean(row.circular_k for row in rows),
        avg_grid_k=mean(row.grid_k for row in rows),
        avg_fr_k=mean(row.fr_k for row in rows),
        avg_sa_k=mean(row.sa_k for row in rows),

        best_baseline_k=best_baseline_k,
        improvement_vs_fr_best_k=best_fr_k - best_sa_k,
        improvement_vs_best_baseline_k=best_baseline_k - best_sa_k,

        best_sa_crossings=min(row.sa_crossings for row in rows),
        avg_sa_crossings=mean(row.sa_crossings for row in rows),
        sa_valid_rate=bool_rate([row.sa_valid for row in rows]),
        avg_duration_ms=mean(row.duration_ms for row in rows),
    )


def summarize(rows: list[ExperimentRow]) -> list[BudgetSummary]:
    grouped = group_by_graph_and_budget(rows)

    summaries = [
        summarize_group(graph, sa_iterations, group_rows)
        for (graph, sa_iterations), group_rows in grouped.items()
    ]

    return sorted(
        summaries,
        key=lambda summary: (summary.graph, summary.sa_iterations)
    )


def format_float(value: float) -> str:
    return f"{value:.2f}"


def write_csv(path: Path, summaries: list[BudgetSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "graph",
        "sa_iterations",
        "runs",

        "best_random_k",
        "best_circular_k",
        "best_grid_k",
        "best_fr_k",
        "best_sa_k",

        "avg_random_k",
        "avg_circular_k",
        "avg_grid_k",
        "avg_fr_k",
        "avg_sa_k",

        "best_baseline_k",
        "improvement_vs_fr_best_k",
        "improvement_vs_best_baseline_k",

        "best_sa_crossings",
        "avg_sa_crossings",
        "sa_valid_rate",
        "avg_duration_ms",
    ]

    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()

        for summary in summaries:
            writer.writerow({
                "graph": summary.graph,
                "sa_iterations": summary.sa_iterations,
                "runs": summary.runs,

                "best_random_k": summary.best_random_k,
                "best_circular_k": summary.best_circular_k,
                "best_grid_k": summary.best_grid_k,
                "best_fr_k": summary.best_fr_k,
                "best_sa_k": summary.best_sa_k,

                "avg_random_k": format_float(summary.avg_random_k),
                "avg_circular_k": format_float(summary.avg_circular_k),
                "avg_grid_k": format_float(summary.avg_grid_k),
                "avg_fr_k": format_float(summary.avg_fr_k),
                "avg_sa_k": format_float(summary.avg_sa_k),

                "best_baseline_k": summary.best_baseline_k,
                "improvement_vs_fr_best_k": summary.improvement_vs_fr_best_k,
                "improvement_vs_best_baseline_k": summary.improvement_vs_best_baseline_k,

                "best_sa_crossings": summary.best_sa_crossings,
                "avg_sa_crossings": format_float(summary.avg_sa_crossings),
                "sa_valid_rate": format_float(summary.sa_valid_rate),
                "avg_duration_ms": format_float(summary.avg_duration_ms),
            })


def print_summary(summaries: list[BudgetSummary]) -> None:
    header = [
        "graph",
        "sa_iterations",
        "runs",
        "best_fr_k",
        "best_sa_k",
        "avg_sa_k",
        "best_baseline_k",
        "improvement_vs_fr_best_k",
        "improvement_vs_best_baseline_k",
        "sa_valid_rate",
        "avg_duration_ms",
    ]

    print(",".join(header))

    for summary in summaries:
        values = [
            summary.graph,
            str(summary.sa_iterations),
            str(summary.runs),
            str(summary.best_fr_k),
            str(summary.best_sa_k),
            format_float(summary.avg_sa_k),
            str(summary.best_baseline_k),
            str(summary.improvement_vs_fr_best_k),
            str(summary.improvement_vs_best_baseline_k),
            format_float(summary.sa_valid_rate),
            format_float(summary.avg_duration_ms),
        ]

        print(",".join(values))


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize solver experiments grouped by graph and SA iteration budget."
    )

    parser.add_argument(
        "input_csv",
        type=Path,
        help="Input CSV produced by scripts/run_experiments.sh."
    )

    parser.add_argument(
        "--output-csv",
        type=Path,
        default=None,
        help="Optional output CSV for grouped summary."
    )

    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    if not args.input_csv.exists():
        raise FileNotFoundError(f"Input CSV does not exist: {args.input_csv}")

    rows = load_rows(args.input_csv)
    summaries = summarize(rows)

    print_summary(summaries)

    if args.output_csv is not None:
        write_csv(args.output_csv, summaries)
        print()
        print(f"Summary written to: {args.output_csv}")


if __name__ == "__main__":
    main()
