
#!/usr/bin/env python3

import argparse
import csv
import json
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
    duration_ms: int


@dataclass
class GraphInfo:
    graph: str
    nodes: int
    edges: int
    width: int
    height: int


@dataclass
class RuntimeScalingSummary:
    graph: str
    nodes: int
    edges: int
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

    min_duration_ms: int
    median_duration_ms: float
    avg_duration_ms: float
    max_duration_ms: int

    avg_duration_per_iteration_ms: float
    avg_duration_per_node_ms: float
    avg_duration_per_edge_ms: float
    avg_duration_per_edge_pair_ms: float


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


def parse_experiment_row(row: dict[str, str]) -> ExperimentRow:
    return ExperimentRow(
        graph=row["graph"],
        seed=parse_int(row, "seed"),
        sa_iterations=parse_int(row, "sa_iterations"),
        sa_k=parse_int(row, "sa_k"),
        sa_crossings=parse_int(row, "sa_crossings"),
        sa_valid=parse_bool(row, "sa_valid"),
        duration_ms=parse_int(row, "duration_ms"),
    )


def load_experiment_rows(path: Path) -> list[ExperimentRow]:
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
            "duration_ms",
        }

        missing_columns = required_columns.difference(reader.fieldnames or [])

        if missing_columns:
            missing = ", ".join(sorted(missing_columns))
            raise ValueError(f"Input CSV is missing required columns: {missing}")

        for row in reader:
            rows.append(parse_experiment_row(row))

    if not rows:
        raise ValueError(f"No experiment rows found in {path}")

    return rows


def load_graph_info(graph_path: Path) -> GraphInfo:
    with graph_path.open("r", encoding="utf-8") as file:
        data = json.load(file)

    nodes = data.get("nodes", [])
    edges = data.get("edges", [])

    return GraphInfo(
        graph=str(graph_path),
        nodes=len(nodes),
        edges=len(edges),
        width=int(data.get("width", 1_000_000)),
        height=int(data.get("height", 1_000_000)),
    )


def load_graph_infos(rows: list[ExperimentRow]) -> dict[str, GraphInfo]:
    infos: dict[str, GraphInfo] = {}

    for row in rows:
        if row.graph in infos:
            continue

        graph_path = Path(row.graph)

        if not graph_path.exists():
            raise FileNotFoundError(f"Graph file from experiment CSV does not exist: {graph_path}")

        infos[row.graph] = load_graph_info(graph_path)

    return infos


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


def safe_divide(value: float, divisor: float) -> float:
    if divisor == 0.0:
        return 0.0

    return value / divisor


def edge_pair_count(edges: int) -> int:
    if edges < 2:
        return 0

    return edges * (edges - 1) // 2


def summarize_group(
    graph: str,
    sa_iterations: int,
    rows: list[ExperimentRow],
    graph_info: GraphInfo
) -> RuntimeScalingSummary:
    sa_k_values = [row.sa_k for row in rows]
    sa_crossing_values = [row.sa_crossings for row in rows]
    duration_values = [row.duration_ms for row in rows]

    avg_duration = mean(duration_values)
    pairs = edge_pair_count(graph_info.edges)

    return RuntimeScalingSummary(
        graph=graph,
        nodes=graph_info.nodes,
        edges=graph_info.edges,
        sa_iterations=sa_iterations,
        runs=len(rows),

        best_sa_k=min(sa_k_values),
        median_sa_k=median(sa_k_values),
        avg_sa_k=mean(sa_k_values),
        worst_sa_k=max(sa_k_values),

        best_sa_crossings=min(sa_crossing_values),
        median_sa_crossings=median(sa_crossing_values),
        avg_sa_crossings=mean(sa_crossing_values),

        sa_valid_rate=bool_rate([row.sa_valid for row in rows]),

        min_duration_ms=min(duration_values),
        median_duration_ms=median(duration_values),
        avg_duration_ms=avg_duration,
        max_duration_ms=max(duration_values),

        avg_duration_per_iteration_ms=safe_divide(avg_duration, float(sa_iterations)),
        avg_duration_per_node_ms=safe_divide(avg_duration, float(graph_info.nodes)),
        avg_duration_per_edge_ms=safe_divide(avg_duration, float(graph_info.edges)),
        avg_duration_per_edge_pair_ms=safe_divide(avg_duration, float(pairs)),
    )


def summarize(rows: list[ExperimentRow]) -> list[RuntimeScalingSummary]:
    graph_infos = load_graph_infos(rows)
    grouped = group_by_graph_and_budget(rows)

    summaries = [
        summarize_group(
            graph=graph,
            sa_iterations=sa_iterations,
            rows=group_rows,
            graph_info=graph_infos[graph],
        )
        for (graph, sa_iterations), group_rows in grouped.items()
    ]

    return sorted(
        summaries,
        key=lambda summary: (summary.nodes, summary.edges, summary.graph, summary.sa_iterations)
    )


def format_float(value: float) -> str:
    return f"{value:.4f}"


def print_summary(summaries: list[RuntimeScalingSummary]) -> None:
    header = [
        "graph",
        "nodes",
        "edges",
        "sa_iterations",
        "runs",
        "best_sa_k",
        "median_sa_k",
        "avg_sa_k",
        "worst_sa_k",
        "sa_valid_rate",
        "median_duration_ms",
        "avg_duration_ms",
        "avg_duration_per_iteration_ms",
        "avg_duration_per_node_ms",
        "avg_duration_per_edge_ms",
        "avg_duration_per_edge_pair_ms",
    ]

    print(",".join(header))

    for summary in summaries:
        values = [
            summary.graph,
            str(summary.nodes),
            str(summary.edges),
            str(summary.sa_iterations),
            str(summary.runs),
            str(summary.best_sa_k),
            format_float(summary.median_sa_k),
            format_float(summary.avg_sa_k),
            str(summary.worst_sa_k),
            format_float(summary.sa_valid_rate),
            format_float(summary.median_duration_ms),
            format_float(summary.avg_duration_ms),
            format_float(summary.avg_duration_per_iteration_ms),
            format_float(summary.avg_duration_per_node_ms),
            format_float(summary.avg_duration_per_edge_ms),
            format_float(summary.avg_duration_per_edge_pair_ms),
        ]

        print(",".join(values))


def write_csv(path: Path, summaries: list[RuntimeScalingSummary]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    fieldnames = [
        "graph",
        "nodes",
        "edges",
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

        "min_duration_ms",
        "median_duration_ms",
        "avg_duration_ms",
        "max_duration_ms",

        "avg_duration_per_iteration_ms",
        "avg_duration_per_node_ms",
        "avg_duration_per_edge_ms",
        "avg_duration_per_edge_pair_ms",
    ]

    with path.open("w", encoding="utf-8", newline="") as file:
        writer = csv.DictWriter(file, fieldnames=fieldnames)
        writer.writeheader()

        for summary in summaries:
            writer.writerow({
                "graph": summary.graph,
                "nodes": summary.nodes,
                "edges": summary.edges,
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

                "min_duration_ms": summary.min_duration_ms,
                "median_duration_ms": format_float(summary.median_duration_ms),
                "avg_duration_ms": format_float(summary.avg_duration_ms),
                "max_duration_ms": summary.max_duration_ms,

                "avg_duration_per_iteration_ms": format_float(summary.avg_duration_per_iteration_ms),
                "avg_duration_per_node_ms": format_float(summary.avg_duration_per_node_ms),
                "avg_duration_per_edge_ms": format_float(summary.avg_duration_per_edge_ms),
                "avg_duration_per_edge_pair_ms": format_float(summary.avg_duration_per_edge_pair_ms),
            })


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Summarize runtime and scaling behavior of solver experiments."
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

    rows = load_experiment_rows(args.input_csv)
    summaries = summarize(rows)

    print_summary(summaries)

    if args.output_csv is not None:
        write_csv(args.output_csv, summaries)
        print()
        print(f"Summary written to: {args.output_csv}")


if __name__ == "__main__":
    main()
