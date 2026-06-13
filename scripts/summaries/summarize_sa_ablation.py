
#!/usr/bin/env python3

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from statistics import mean, median


@dataclass
class ExperimentRow:
    graph: str
    seed: int
    sa_iterations: int

    sa_k: int
    sa_crossings: int
    sa_valid: bool

    sa_uniform_k: int
    sa_uniform_crossings: int
    sa_uniform_valid: bool

    duration_ms: int


@dataclass
class AblationSummary:
    graph: str
    sa_iterations: int
    runs: int

    best_sa_k: int
    median_sa_k: float
    avg_sa_k: float
    worst_sa_k: int
    best_sa_crossings: int
    median_sa_crossings: float
    avg_sa_crossings: float
    sa_valid_rate: float

    best_sa_uniform_k: int
    median_sa_uniform_k: float
    avg_sa_uniform_k: float
    worst_sa_uniform_k: int
    best_sa_uniform_crossings: int
    median_sa_uniform_crossings: float
    avg_sa_uniform_crossings: float
    sa_uniform_valid_rate: float

    hotspot_minus_uniform_best_k: int
    hotspot_minus_uniform_median_k: float
    hotspot_minus_uniform_avg_k: float
    hotspot_minus_uniform_worst_k: int

    hotspot_wins_by_k: int
    uniform_wins_by_k: int
    ties_by_k: int

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
        sa_iterations=parse_int(row, "sa_iterations"),

        sa_k=parse_int(row, "sa_k"),
        sa_crossings=parse_int(row, "sa_crossings"),
        sa_valid=parse_bool(row, "sa_valid"),

        sa_uniform_k=parse_int(row, "sa_uniform_k"),
        sa_uniform_crossings=parse_int(row, "sa_uniform_crossings"),
        sa_uniform_valid=parse_bool(row, "sa_uniform_valid"),

        duration_ms=parse_int(row, "duration_ms"),
    )


def load_rows(path: Path) -> list[ExperimentRow]:
    rows: list[ExperimentRow] = []

    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)

        required_columns = {
            "graph",
            "seed",
            "sa_iterations",
            "sa_k",
            "sa_crossings",
            "sa_valid",
            "sa_uniform_k",
            "sa_uniform_crossings",
            "sa_uniform_valid",
            "duration_ms",
        }

        missing_columns = required_columns.difference(reader.fieldnames or [])

        if missing_columns:
            missing = ", ".join(sorted(missing_columns))
            raise ValueError(f"Input CSV is missing required columns: {missing}")

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


def count_hotspot_wins_by_k(rows: list[ExperimentRow]) -> int:
    return sum(1 for row in rows if row.sa_k < row.sa_uniform_k)


def count_uniform_wins_by_k(rows: list[ExperimentRow]) -> int:
    return sum(1 for row in rows if row.sa_uniform_k < row.sa_k)


def count_ties_by_k(rows: list[ExperimentRow]) -> int:
    return sum(1 for row in rows if row.sa_k == row.sa_uniform_k)


def summarize_group(
    graph: str,
    sa_iterations: int,
    rows: list[ExperimentRow]
) -> AblationSummary:
    sa_k_values = [row.sa_k for row in rows]
    sa_uniform_k_values = [row.sa_uniform_k for row in rows]

    sa_crossing_values = [row.sa_crossings for row in rows]
    sa_uniform_crossing_values = [row.sa_uniform_crossings for row in rows]

    best_sa_k = min(sa_k_values)
    median_sa_k = median(sa_k_values)
    avg_sa_k = mean(sa_k_values)
    worst_sa_k = max(sa_k_values)

    best_sa_uniform_k = min(sa_uniform_k_values)
    median_sa_uniform_k = median(sa_uniform_k_values)
    avg_sa_uniform_k = mean(sa_uniform_k_values)
    worst_sa_uniform_k = max(sa_uniform_k_values)

    return AblationSummary(
        graph=graph,
        sa_iterations=sa_iterations,
        runs=len(rows),

        best_sa_k=best_sa_k,
        median_sa_k=median_sa_k,
        avg_sa_k=avg_sa_k,
        worst_sa_k=worst_sa_k,
        best_sa_crossings=min(sa_crossing_values),
        median_sa_crossings=median(sa_crossing_values),
        avg_sa_crossings=mean(sa_crossing_values),
        sa_valid_rate=bool_rate([row.sa_valid for row in rows]),

        best_sa_uniform_k=best_sa_uniform_k,
        median_sa_uniform_k=median_sa_uniform_k,
        avg_sa_uniform_k=avg_sa_uniform_k,
        worst_sa_uniform_k=worst_sa_uniform_k,
        best_sa_uniform_crossings=min(sa_uniform_crossing_values),
        median_sa_uniform_crossings=median(sa_uniform_crossing_values),
        avg_sa_uniform_crossings=mean(sa_uniform_crossing_values),
        sa_uniform_valid_rate=bool_rate([row.sa_uniform_valid for row in rows]),

        hotspot_minus_uniform_best_k=best_sa_k - best_sa_uniform_k,
        hotspot_minus_uniform_median_k=median_sa_k - median_sa_uniform_k,
        hotspot_minus_uniform_avg_k=avg_sa_k - avg_sa_uniform_k,
        hotspot_minus_uniform_worst_k=worst_sa_k - worst_sa_uniform_k,

        hotspot_wins_by_k=count_hotspot_wins_by_k(rows),
        uniform_wins_by_k=count_uniform_wins_by_k(rows),
        ties_by_k=count_ties_by_k(rows),

        avg_duration_ms=mean(row.duration_ms for row in rows),
    )


def summarize(rows: list[ExperimentRow]) -> list[AblationSummary]:
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


def print_summary(summaries: list[AblationSummary]) -> None:
    header = [
        "graph",
        "sa_iterations",
        "runs",
        "best_sa_k",
        "median_sa_k",
        "avg_sa_k",
        "worst_sa_k",
        "best_sa_uniform_k",
        "median_sa_uniform_k",
        "avg_sa_uniform_k",
        "worst_sa_uniform_k",
        "hotspot_minus_uniform_best_k",
        "hotspot_minus_uniform_median_k",
        "hotspot_minus_uniform_avg_k",
        "hotspot_minus_uniform_worst_k",
        "hotspot_wins_by_k",
        "uniform_wins_by_k",
        "ties_by_k",
        "sa_valid_rate",
        "sa_uniform_valid_rate",
        "avg_duration_ms",
    ]

    print(",".join(header))

    for summary in summaries:
        values = [
            summary.graph,
            str(summary.sa_iterations),
            str(summary.runs),
            str(summary.best_sa_k),
            format_float(summary.median_sa_k),
            format_float(summary.avg_sa_k),
            str(summary.worst_sa_k),
            str(summary.best_sa_uniform_k),
            format_float(summary.median_sa_uniform_k),
            format_float(summary.avg_sa_uniform_k),
            str(summary.worst_sa_uniform_k),
            str(summary.hotspot_minus_uniform_best_k),
            format_float(summary.hotspot_minus_uniform_median_k),
            format_float(summary.hotspot_minus_uniform_avg_k),
            str(summary.hotspot_minus_uniform_worst_k),
            str(summary.hotspot_wins_by_k),
            str(summary.uniform_wins_by_k),
            str(summary.ties_by_k),
            format_float(summary.sa_valid_rate),
            format_float(summary.sa_uniform_valid_rate),
            format_float(summary.avg_duration_ms),
        ]

        print(",".join(values))


def write_csv(path: Path, summaries: list[AblationSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "graph",
        "sa_iterations",
        "runs",

        "best_sa_k",
        "median_sa_k",
        "avg_sa_k",
        "worst_sa_k",
        "best_sa_crossings",
        "median_sa_crossings",
        "avg_sa_crossings",
        "sa_valid_rate",

        "best_sa_uniform_k",
        "median_sa_uniform_k",
        "avg_sa_uniform_k",
        "worst_sa_uniform_k",
        "best_sa_uniform_crossings",
        "median_sa_uniform_crossings",
        "avg_sa_uniform_crossings",
        "sa_uniform_valid_rate",

        "hotspot_minus_uniform_best_k",
        "hotspot_minus_uniform_median_k",
        "hotspot_minus_uniform_avg_k",
        "hotspot_minus_uniform_worst_k",

        "hotspot_wins_by_k",
        "uniform_wins_by_k",
        "ties_by_k",

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

                "best_sa_k": summary.best_sa_k,
                "median_sa_k": format_float(summary.median_sa_k),
                "avg_sa_k": format_float(summary.avg_sa_k),
                "worst_sa_k": summary.worst_sa_k,
                "best_sa_crossings": summary.best_sa_crossings,
                "median_sa_crossings": format_float(summary.median_sa_crossings),
                "avg_sa_crossings": format_float(summary.avg_sa_crossings),
                "sa_valid_rate": format_float(summary.sa_valid_rate),

                "best_sa_uniform_k": summary.best_sa_uniform_k,
                "median_sa_uniform_k": format_float(summary.median_sa_uniform_k),
                "avg_sa_uniform_k": format_float(summary.avg_sa_uniform_k),
                "worst_sa_uniform_k": summary.worst_sa_uniform_k,
                "best_sa_uniform_crossings": summary.best_sa_uniform_crossings,
                "median_sa_uniform_crossings": format_float(summary.median_sa_uniform_crossings),
                "avg_sa_uniform_crossings": format_float(summary.avg_sa_uniform_crossings),
                "sa_uniform_valid_rate": format_float(summary.sa_uniform_valid_rate),

                "hotspot_minus_uniform_best_k": summary.hotspot_minus_uniform_best_k,
                "hotspot_minus_uniform_median_k": format_float(summary.hotspot_minus_uniform_median_k),
                "hotspot_minus_uniform_avg_k": format_float(summary.hotspot_minus_uniform_avg_k),
                "hotspot_minus_uniform_worst_k": summary.hotspot_minus_uniform_worst_k,

                "hotspot_wins_by_k": summary.hotspot_wins_by_k,
                "uniform_wins_by_k": summary.uniform_wins_by_k,
                "ties_by_k": summary.ties_by_k,

                "avg_duration_ms": format_float(summary.avg_duration_ms),
            })


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize SA hotspot vs uniform sampling ablation experiments."
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
        help="Optional output CSV path."
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
