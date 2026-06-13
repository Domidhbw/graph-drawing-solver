
#!/usr/bin/env python3

import argparse
import json
from pathlib import Path


def read_non_empty_non_comment_lines(path: Path) -> list[str]:
    lines: list[str] = []

    with path.open("r", encoding="utf-8") as file:
        for raw_line in file:
            line = raw_line.strip()

            if not line:
                continue

            if line.startswith("#"):
                continue

            lines.append(line)

    return lines


def parse_header(line: str, path: Path) -> tuple[int, int]:
    parts = line.split()

    if len(parts) != 2:
        raise ValueError(f"{path}: invalid header. Expected '<num_nodes> <num_edges>'.")

    try:
        node_count = int(parts[0])
        edge_count = int(parts[1])
    except ValueError as exception:
        raise ValueError(f"{path}: header values must be integers.") from exception

    if node_count < 0:
        raise ValueError(f"{path}: node count must not be negative.")

    if edge_count < 0:
        raise ValueError(f"{path}: edge count must not be negative.")

    return node_count, edge_count


def parse_edge(line: str, path: Path, line_number: int, node_count: int) -> dict:
    parts = line.split()

    if len(parts) != 2:
        raise ValueError(f"{path}:{line_number}: invalid edge. Expected '<source> <target>'.")

    try:
        source = int(parts[0])
        target = int(parts[1])
    except ValueError as exception:
        raise ValueError(f"{path}:{line_number}: edge endpoints must be integers.") from exception

    if source < 0 or source >= node_count:
        raise ValueError(
            f"{path}:{line_number}: source node id {source} out of range 0..{node_count - 1}."
        )

    if target < 0 or target >= node_count:
        raise ValueError(
            f"{path}:{line_number}: target node id {target} out of range 0..{node_count - 1}."
        )

    return {
        "source": source,
        "target": target
    }


def calculate_grid_size(node_count: int) -> int:
    return max(1, node_count * node_count)


def convert_edgelist_to_graph(path: Path) -> dict:
    lines = read_non_empty_non_comment_lines(path)

    if not lines:
        raise ValueError(f"{path}: file is empty.")

    node_count, expected_edge_count = parse_header(lines[0], path)

    edge_lines = lines[1:]

    if len(edge_lines) != expected_edge_count:
        raise ValueError(
            f"{path}: expected {expected_edge_count} edges, but found {len(edge_lines)}."
        )

    nodes = [
        {
            "id": node_id,
            "x": 0,
            "y": 0
        }
        for node_id in range(node_count)
    ]

    edges = [
        parse_edge(line, path, index + 2, node_count)
        for index, line in enumerate(edge_lines)
    ]

    grid_size = calculate_grid_size(node_count)

    return {
        "nodes": nodes,
        "edges": edges,
        "width": grid_size,
        "height": grid_size
    }


def convert_file(input_path: Path, output_path: Path) -> None:
    graph = convert_edgelist_to_graph(input_path)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as file:
        json.dump(graph, file, indent=2)
        file.write("\n")

    print(f"Converted: {input_path} -> {output_path}")


def convert_directory(input_dir: Path, output_dir: Path) -> None:
    input_files = sorted(input_dir.rglob("*.txt"))

    if not input_files:
        raise ValueError(f"No .txt files found in {input_dir}")

    converted_count = 0

    for input_path in input_files:
        relative_path = input_path.relative_to(input_dir)
        output_path = output_dir / relative_path.with_suffix(".json")

        convert_file(input_path, output_path)
        converted_count += 1

    print()
    print(f"Finished. Converted {converted_count} files.")


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert graph edge-list .txt files to solver JSON format."
    )

    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing .txt edge-list files."
    )

    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Directory where converted .json files are written."
    )

    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    if not args.input_dir.exists():
        raise ValueError(f"Input directory does not exist: {args.input_dir}")

    if not args.input_dir.is_dir():
        raise ValueError(f"Input path is not a directory: {args.input_dir}")

    convert_directory(args.input_dir, args.output_dir)


if __name__ == "__main__":
    main()
