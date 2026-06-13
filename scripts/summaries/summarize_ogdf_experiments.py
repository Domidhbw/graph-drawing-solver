
#!/usr/bin/env python3

import argparse
import csv
from dataclasses import dataclass
from pathlib import Path
from statistics import mean, median


@dataclass
class OgdfExperimentRow:
    graph: str
    seed: int
    sa_iterations: int

    ogdf_k: int | None
    ogdf_crossings: int | None
    ogdf_valid: bool

    ogdf_sa_k: int | None
    ogdf_sa_crossings: int | None
    ogdf_sa_valid: bool

    duration_ms: int


@dataclass
class OgdfSummary:
    graph: str
    sa_iterations: int
    runs: int

    ogdf_valid_rate: float
    best_ogdf_k: int | None
    median_ogdf_k: float | None
    avg_ogdf_k: float | None
    worst_ogdf_k: int | None
    best_ogdf_crossings: int | None
    median_ogdf_crossings: float | None
    avg_ogdf_crossings: float | None

    ogdf_sa_valid_rate: float
    best_ogdf_sa_k: int | None
    median_ogdf_sa_k: float | None
    avg_ogdf_sa_k: float | None
    worst_ogdf_sa_k: int | None
    best_ogdf_sa_crossings: int | None
    median_ogdf_sa_crossings: float | None
    avg_ogdf_sa_crossings: float | None

    improvement_best_k: int | None
    improvement_median_k: float | None
    improvement_avg_k: float | None
    improvement_worst_k: int | None

    ogdf_sa_wins_by_k: int
    ogdf_wins_by_k: int
    ties_by_k: int

    avg_duration_ms: float


def parse_optional_int(row: dict[str, str], column: str) -> int | None:
    value = row.get(column)

    if value is None or value == "":
        return None

    return int(value)


def parse_required_int(row: dict[str, str], column: str) -> int:
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


def parse_row(row: dict[str, str]) -> OgdfExperimentRow:
    return OgdfExperimentRow(
        graph=row["graph"],
        seed=parse_required_int(row, "seed"),
        sa_iterations=parse_required_int(row, "sa_iterations"),

        ogdf_k=parse_optional_int(row, "ogdf_k"),
        ogdf_crossings=parse_optional_int(row, "ogdf_crossings"),
        ogdf_valid=parse_bool(row, "ogdf_valid"),

        ogdf_sa_k=parse_optional_int(row, "ogdf_sa_k"),
        ogdf_sa_crossings=parse_optional_int(row, "ogdf_sa_crossings"),
        ogdf_sa_valid=parse_bool(row, "ogdf_sa_valid"),

        duration_ms=parse_required_int(row, "duration_ms"),
    )


def load_rows(path: Path) -> list[OgdfExperimentRow]:
    rows: list[OgdfExperimentRow] = []

    with path.open("r", encoding="utf-8", newline="") as file:
        reader = csv.DictReader(file)

        required_columns = {
            "graph",
            "seed",
            "sa_iterations",
            "ogdf_k",
            "ogdf_crossings",
            "ogdf_valid",
            "ogdf_sa_k",
            "ogdf_sa_crossings",
            "ogdf_sa_valid",
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
    rows: list[OgdfExperimentRow]
) -> dict[tuple[str, int], list[OgdfExperimentRow]]:
    grouped: dict[tuple[str, int], list[OgdfExperimentRow]] = {}

    for row in rows:
        key = (row.graph, row.sa_iterations)
        grouped.setdefault(key, []).append(row)

    return grouped


def bool_rate(values: list[bool]) -> float:
    if not values:
        return 0.0

    return mean(1.0 if value else 0.0 for value in values)


def present_values(values: list[int | None]) -> list[int]:
    return [value for value in values if value is not None]


def min_or_none(values: list[int]) -> int | None:
    if not values:
        return None

    return min(values)


def max_or_none(values: list[int]) -> int | None:
    if not values:
        return None

    return max(values)


def median_or_none(values: list[int]) -> float | None:
    if not values:
        return None

    return float(median(values))


def mean_or_none(values: list[int]) -> float | None:
    if not values:
        return None

    return float(mean(values))


def optional_difference(first: int | float | None, second: int | float | None) -> float | None:
    if first is None or second is None:
        return None

    return float(first) - float(second)


def count_ogdf_sa_wins_by_k(rows: list[OgdfExperimentRow]) -> int:
    return sum(
        1
        for row in rows
        if row.ogdf_k is not None
        and row.ogdf_sa_k is not None
        and row.ogdf_sa_k < row.ogdf_k
    )


def count_ogdf_wins_by_k(rows: list[OgdfExperimentRow]) -> int:
    return sum(
        1
        for row in rows
        if row.ogdf_k is not None
        and row.ogdf_sa_k is not None
        and row.ogdf_k < row.ogdf_sa_k
    )


def count_ties_by_k(rows: list[OgdfExperimentRow]) -> int:
    return sum(
        1
        for row in rows
        if row.ogdf_k is not None
        and row.ogdf_sa_k is not None
        and row.ogdf_k == row.ogdf_sa_k
    )


def summarize_group(
    graph: str,
    sa_iterations: int,
    rows: list[OgdfExperimentRow]
) -> OgdfSummary:
    ogdf_k_values = present_values([row.ogdf_k for row in rows])
    ogdf_crossing_values = present_values([row.ogdf_crossings for row in rows])

    ogdf_sa_k_values = present_values([row.ogdf_sa_k for row in rows])
    ogdf_sa_crossing_values = present_values([row.ogdf_sa_crossings for row in rows])

    best_ogdf_k = min_or_none(ogdf_k_values)
    median_ogdf_k = median_or_none(ogdf_k_values)
    avg_ogdf_k = mean_or_none(ogdf_k_values)
    worst_ogdf_k = max_or_none(ogdf_k_values)

    best_ogdf_sa_k = min_or_none(ogdf_sa_k_values)
    median_ogdf_sa_k = median_or_none(ogdf_sa_k_values)
    avg_ogdf_sa_k = mean_or_none(ogdf_sa_k_values)
    worst_ogdf_sa_k = max_or_none(ogdf_sa_k_values)

    return OgdfSummary(
        graph=graph,
        sa_iterations=sa_iterations,
        runs=len(rows),

        ogdf_valid_rate=bool_rate([row.ogdf_valid for row in rows]),
        best_ogdf_k=best_ogdf_k,
        median_ogdf_k=median_ogdf_k,
        avg_ogdf_k=avg_ogdf_k,
        worst_ogdf_k=worst_ogdf_k,
        best_ogdf_crossings=min_or_none(ogdf_crossing_values),
        median_ogdf_crossings=median_or_none(ogdf_crossing_values),
        avg_ogdf_crossings=mean_or_none(ogdf_crossing_values),

        ogdf_sa_valid_rate=bool_rate([row.ogdf_sa_valid for row in rows]),
        best_ogdf_sa_k=best_ogdf_sa_k,
        median_ogdf_sa_k=median_ogdf_sa_k,
        avg_ogdf_sa_k=avg_ogdf_sa_k,
        worst_ogdf_sa_k=worst_ogdf_sa_k,
        best_ogdf_sa_crossings=min_or_none(ogdf_sa_crossing_values),
        median_ogdf_sa_crossings=median_or_none(ogdf_sa_crossing_values),
        avg_ogdf_sa_crossings=mean_or_none(ogdf_sa_crossing_values),

        improvement_best_k=None if best_ogdf_k is None or best_ogdf_sa_k is None else best_ogdf_k - best_ogdf_sa_k,
        improvement_median_k=optional_difference(median_ogdf_k, median_ogdf_sa_k),
        improvement_avg_k=optional_difference(avg_ogdf_k, avg_ogdf_sa_k),
        improvement_worst_k=None if worst_ogdf_k is None or worst_ogdf_sa_k is None else worst_ogdf_k - worst_ogdf_sa_k,

        ogdf_sa_wins_by_k=count_ogdf_sa_wins_by_k(rows),
        ogdf_wins_by_k=count_ogdf_wins_by_k(rows),
        ties_by_k=count_ties_by_k(rows),

        avg_duration_ms=mean(row.duration_ms for row in rows),
    )


def summarize(rows: list[OgdfExperimentRow]) -> list[OgdfSummary]:
    grouped = group_by_graph_and_budget(rows)

    summaries = [
        summarize_group(graph, sa_iterations, group_rows)
        for (graph, sa_iterations), group_rows in grouped.items()
    ]

    return sorted(
        summaries,
        key=lambda summary: (summary.graph, summary.sa_iterations)
    )


def format_optional_int(value: int | None) -> str:
    if value is None:
        return ""

    return str(value)


def format_optional_float(value: float | None) -> str:
    if value is None:
        return ""

    return f"{value:.2f}"


def format_float(value: float) -> str:
    return f"{value:.2f}"


def print_summary(summaries: list[OgdfSummary]) -> None:
    header = [
        "graph",
        "sa_iterations",
        "runs",
        "ogdf_valid_rate",
        "best_ogdf_k",
        "median_ogdf_k",
        "avg_ogdf_k",
        "worst_ogdf_k",
        "ogdf_sa_valid_rate",
        "best_ogdf_sa_k",
        "median_ogdf_sa_k",
        "avg_ogdf_sa_k",
        "worst_ogdf_sa_k",
        "improvement_best_k",
        "improvement_median_k",
        "improvement_avg_k",
        "improvement_worst_k",
        "ogdf_sa_wins_by_k",
        "ogdf_wins_by_k",
        "ties_by_k",
        "avg_duration_ms",
    ]

    print(",".join(header))

    for summary in summaries:
        values = [
            summary.graph,
            str(summary.sa_iterations),
            str(summary.runs),
            format_float(summary.ogdf_valid_rate),
            format_optional_int(summary.best_ogdf_k),
            format_optional_float(summary.median_ogdf_k),
            format_optional_float(summary.avg_ogdf_k),
            format_optional_int(summary.worst_ogdf_k),
            format_float(summary.ogdf_sa_valid_rate),
            format_optional_int(summary.best_ogdf_sa_k),
            format_optional_float(summary.median_ogdf_sa_k),
            format_optional_float(summary.avg_ogdf_sa_k),
            format_optional_int(summary.worst_ogdf_sa_k),
            format_optional_int(summary.improvement_best_k if summary.improvement_best_k is None else int(summary.improvement_best_k)),
            format_optional_float(summary.improvement_median_k),
            format_optional_float(summary.improvement_avg_k),
            format_optional_int(summary.improvement_worst_k),
            str(summary.ogdf_sa_wins_by_k),
            str(summary.ogdf_wins_by_k),
            str(summary.ties_by_k),
            format_float(summary.avg_duration_ms),
        ]

        print(",".join(values))


def write_csv(path: Path, summaries: list[OgdfSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "graph",
        "sa_iterations",
        "runs",

        "ogdf_valid_rate",
        "best_ogdf_k",
        "median_ogdf_k",
        "avg_ogdf_k",
        "worst_ogdf_k",
        "best_ogdf_crossings",
        "median_ogdf_crossings",
        "avg_ogdf_crossings",

        "ogdf_sa_valid_rate",
        "best_ogdf_sa_k",
        "median_ogdf_sa_k",
        "avg_ogdf_sa_k",
        "worst_ogdf_sa_k",
        "best_ogdf_sa_crossings",
        "median_ogdf_sa_crossings",
        "avg_ogdf_sa_crossings",

        "improvement_best_k",
        "improvement_median_k",
        "improvement_avg_k",
        "improvement_worst_k",

        "ogdf_sa_wins_by_k",
        "ogdf_wins_by_k",
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

                "ogdf_valid_rate": format_float(summary.ogdf_valid_rate),
                "best_ogdf_k": format_optional_int(summary.best_ogdf_k),
                "median_ogdf_k": format_optional_float(summary.median_ogdf_k),
                "avg_ogdf_k": format_optional_float(summary.avg_ogdf_k),
                "worst_ogdf_k": format_optional_int(summary.worst_ogdf_k),
                "best_ogdf_crossings": format_optional_int(summary.best_ogdf_crossings),
                "median_ogdf_crossings": format_optional_float(summary.median_ogdf_crossings),
                "avg_ogdf_crossings": format_optional_float(summary.avg_ogdf_crossings),

                "ogdf_sa_valid_rate": format_float(summary.ogdf_sa_valid_rate),
                "best_ogdf_sa_k": format_optional_int(summary.best_ogdf_sa_k),
                "median_ogdf_sa_k": format_optional_float(summary.median_ogdf_sa_k),
                "avg_ogdf_sa_k": format_optional_float(summary.avg_ogdf_sa_k),
                "worst_ogdf_sa_k": format_optional_int(summary.worst_ogdf_sa_k),
                "best_ogdf_sa_crossings": format_optional_int(summary.best_ogdf_sa_crossings),
                "median_ogdf_sa_crossings": format_optional_float(summary.median_ogdf_sa_crossings),
                "avg_ogdf_sa_crossings": format_optional_float(summary.avg_ogdf_sa_crossings),

                "improvement_best_k": format_optional_int(summary.improvement_best_k),
                "improvement_median_k": format_optional_float(summary.improvement_median_k),
                "improvement_avg_k": format_optional_float(summary.improvement_avg_k),
                "improvement_worst_k": format_optional_int(summary.improvement_worst_k),

                "ogdf_sa_wins_by_k": summary.ogdf_sa_wins_by_k,
                "ogdf_wins_by_k": summary.ogdf_wins_by_k,
                "ties_by_k": summary.ties_by_k,

                "avg_duration_ms": format_float(summary.avg_duration_ms),
            })


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize OGDF and OGDF+SA experiment results."
    )

    parser.add_argument(
        "input_csv",
        type=Path,
        help="Input CSV produced by scripts/run_ogdf_experiments.sh."
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
