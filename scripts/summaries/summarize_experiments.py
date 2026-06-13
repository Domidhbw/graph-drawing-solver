
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

    original_k: int
    original_crossings: int
    original_valid: bool

    random_k: int
    random_crossings: int
    random_valid: bool

    circular_k: int
    circular_crossings: int
    circular_valid: bool

    grid_k: int
    grid_crossings: int
    grid_valid: bool

    fr_k: int
    fr_crossings: int
    fr_valid: bool

    sa_k: int
    sa_crossings: int
    sa_valid: bool

    duration_ms: int


@dataclass
class GraphSummary:
    graph: str
    runs: int

    original_k: int
    original_crossings: int
    original_valid_rate: float

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

    random_valid_rate: float
    circular_valid_rate: float
    grid_valid_rate: float
    fr_valid_rate: float
    sa_valid_rate: float

    best_sa_crossings: int
    avg_sa_crossings: float

    avg_duration_ms: float


def parse_int(row: dict[str, str], column: str) -> int:
    value = row.get(column)

    if value is None or value == "":
        raise ValueError(f"Missing column value: {column}")

    return int(value)


def parse_bool(row: dict[str, str], column: str) -> bool:
    value = row.get(column)

    if value is None or value == "":
        raise ValueError(f"Missing column value: {column}")

    if value == "1":
        return True

    if value == "0":
        return False

    raise ValueError(f"Invalid boolean value for {column}: {value}")


def bool_rate(values: list[bool]) -> float:
    if not values:
        return 0.0

    return mean(1.0 if value else 0.0 for value in values)


def parse_row(row: dict[str, str]) -> ExperimentRow:
    return ExperimentRow(
        graph=row["graph"],
        seed=parse_int(row, "seed"),
        fr_iterations=parse_int(row, "fr_iterations"),
        sa_iterations=parse_int(row, "sa_iterations"),

        original_k=parse_int(row, "original_k"),
        original_crossings=parse_int(row, "original_crossings"),
        original_valid=parse_bool(row, "original_valid"),

        random_k=parse_int(row, "random_k"),
        random_crossings=parse_int(row, "random_crossings"),
        random_valid=parse_bool(row, "random_valid"),

        circular_k=parse_int(row, "circular_k"),
        circular_crossings=parse_int(row, "circular_crossings"),
        circular_valid=parse_bool(row, "circular_valid"),

        grid_k=parse_int(row, "grid_k"),
        grid_crossings=parse_int(row, "grid_crossings"),
        grid_valid=parse_bool(row, "grid_valid"),

        fr_k=parse_int(row, "fr_k"),
        fr_crossings=parse_int(row, "fr_crossings"),
        fr_valid=parse_bool(row, "fr_valid"),

        sa_k=parse_int(row, "sa_k"),
        sa_crossings=parse_int(row, "sa_crossings"),
        sa_valid=parse_bool(row, "sa_valid"),

        duration_ms=parse_int(row, "duration_ms"),
    )


def load_experiments(path: Path) -> list[ExperimentRow]:
    rows: list[ExperimentRow] = []

    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)

        for row in reader:
            rows.append(parse_row(row))

    if not rows:
        raise ValueError(f"No experiment rows found in {path}")

    return rows


def group_by_graph(rows: list[ExperimentRow]) -> dict[str, list[ExperimentRow]]:
    grouped: dict[str, list[ExperimentRow]] = {}

    for row in rows:
        grouped.setdefault(row.graph, []).append(row)

    return grouped


def summarize_graph(graph: str, rows: list[ExperimentRow]) -> GraphSummary:
    return GraphSummary(
        graph=graph,
        runs=len(rows),

        original_k=rows[0].original_k,
        original_crossings=rows[0].original_crossings,
        original_valid_rate=bool_rate([row.original_valid for row in rows]),

        best_random_k=min(row.random_k for row in rows),
        best_circular_k=min(row.circular_k for row in rows),
        best_grid_k=min(row.grid_k for row in rows),
        best_fr_k=min(row.fr_k for row in rows),
        best_sa_k=min(row.sa_k for row in rows),

        avg_random_k=mean(row.random_k for row in rows),
        avg_circular_k=mean(row.circular_k for row in rows),
        avg_grid_k=mean(row.grid_k for row in rows),
        avg_fr_k=mean(row.fr_k for row in rows),
        avg_sa_k=mean(row.sa_k for row in rows),

        random_valid_rate=bool_rate([row.random_valid for row in rows]),
        circular_valid_rate=bool_rate([row.circular_valid for row in rows]),
        grid_valid_rate=bool_rate([row.grid_valid for row in rows]),
        fr_valid_rate=bool_rate([row.fr_valid for row in rows]),
        sa_valid_rate=bool_rate([row.sa_valid for row in rows]),

        best_sa_crossings=min(row.sa_crossings for row in rows),
        avg_sa_crossings=mean(row.sa_crossings for row in rows),

        avg_duration_ms=mean(row.duration_ms for row in rows),
    )


def summarize(rows: list[ExperimentRow]) -> list[GraphSummary]:
    grouped = group_by_graph(rows)

    summaries = [
        summarize_graph(graph, graph_rows)
        for graph, graph_rows in grouped.items()
    ]

    return sorted(summaries, key=lambda summary: summary.graph)


def format_float(value: float) -> str:
    return f"{value:.2f}"


def format_rate(value: float) -> str:
    return f"{value:.2%}"


def print_summary(summaries: list[GraphSummary]) -> None:
    header = [
        "graph",
        "runs",
        "original_valid_rate",
        "sa_valid_rate",
        "original_k",
        "best_random_k",
        "best_circular_k",
        "best_grid_k",
        "best_fr_k",
        "best_sa_k",
        "avg_sa_k",
        "original_crossings",
        "best_sa_crossings",
        "avg_sa_crossings",
        "avg_duration_ms",
    ]

    print(",".join(header))

    for summary in summaries:
        values = [
            summary.graph,
            str(summary.runs),
            format_rate(summary.original_valid_rate),
            format_rate(summary.sa_valid_rate),
            str(summary.original_k),
            str(summary.best_random_k),
            str(summary.best_circular_k),
            str(summary.best_grid_k),
            str(summary.best_fr_k),
            str(summary.best_sa_k),
            format_float(summary.avg_sa_k),
            str(summary.original_crossings),
            str(summary.best_sa_crossings),
            format_float(summary.avg_sa_crossings),
            format_float(summary.avg_duration_ms),
        ]

        print(",".join(values))


def write_summary_csv(path: Path, summaries: list[GraphSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "graph",
        "runs",
        "original_k",
        "original_crossings",
        "original_valid_rate",
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
        "random_valid_rate",
        "circular_valid_rate",
        "grid_valid_rate",
        "fr_valid_rate",
        "sa_valid_rate",
        "best_sa_crossings",
        "avg_sa_crossings",
        "avg_duration_ms",
    ]

    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()

        for summary in summaries:
            writer.writerow({
                "graph": summary.graph,
                "runs": summary.runs,
                "original_k": summary.original_k,
                "original_crossings": summary.original_crossings,
                "original_valid_rate": format_float(summary.original_valid_rate),
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
                "random_valid_rate": format_float(summary.random_valid_rate),
                "circular_valid_rate": format_float(summary.circular_valid_rate),
                "grid_valid_rate": format_float(summary.grid_valid_rate),
                "fr_valid_rate": format_float(summary.fr_valid_rate),
                "sa_valid_rate": format_float(summary.sa_valid_rate),
                "best_sa_crossings": summary.best_sa_crossings,
                "avg_sa_crossings": format_float(summary.avg_sa_crossings),
                "avg_duration_ms": format_float(summary.avg_duration_ms),
            })


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize solver experiment CSV files."
    )

    parser.add_argument(
        "input_csv",
        type=Path,
        help="Input experiment CSV produced by scripts/run_experiments.sh."
    )

    parser.add_argument(
        "--output-csv",
        type=Path,
        default=None,
        help="Optional output CSV for summarized results."
    )

    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    if not args.input_csv.exists():
        raise FileNotFoundError(f"Input CSV does not exist: {args.input_csv}")

    rows = load_experiments(args.input_csv)
    summaries = summarize(rows)

    print_summary(summaries)

    if args.output_csv is not None:
        write_summary_csv(args.output_csv, summaries)
        print()
        print(f"Summary written to: {args.output_csv}")


if __name__ == "__main__":
    main()
