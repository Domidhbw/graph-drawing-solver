
#!/usr/bin/env python3
import argparse
import json
from pathlib import Path
from typing import Any


DEFAULT_WIDTH = 1_000_000
DEFAULT_HEIGHT = 1_000_000


def parse_int(value: Any, field_name: str) -> int:
    if isinstance(value, bool):
        raise ValueError(f"{field_name} must be an integer, got boolean.")

    if isinstance(value, int):
        return value

    if isinstance(value, str):
        if value.strip() == "":
            raise ValueError(f"{field_name} must not be empty.")
        return int(value)

    raise ValueError(f"{field_name} must be an integer or integer string, got {type(value).__name__}.")


def parse_float(value: Any, field_name: str) -> float:
    if isinstance(value, bool):
        raise ValueError(f"{field_name} must be numeric, got boolean.")

    if isinstance(value, (int, float)):
        return float(value)

    if isinstance(value, str):
        if value.strip() == "":
            raise ValueError(f"{field_name} must not be empty.")
        return float(value)

    raise ValueError(f"{field_name} must be numeric or numeric string, got {type(value).__name__}.")


def read_graph(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        data = json.load(file)

    if "nodes" not in data:
        raise ValueError(f"Missing nodes array in {path}")

    if "edges" not in data:
        raise ValueError(f"Missing edges array in {path}")

    if not isinstance(data["nodes"], list):
        raise ValueError(f"nodes must be an array in {path}")

    if not isinstance(data["edges"], list):
        raise ValueError(f"edges must be an array in {path}")

    return data


def normalize_node(raw_node: dict[str, Any]) -> dict[str, Any]:
    node = {
        "id": parse_int(raw_node["id"], "node.id"),
        "x": parse_float(raw_node["x"], "node.x"),
        "y": parse_float(raw_node["y"], "node.y"),
    }

    if raw_node.get("fixed") is True:
        node["fixed"] = True

    return node


def normalize_edge(raw_edge: dict[str, Any]) -> dict[str, int]:
    return {
        "source": parse_int(raw_edge["source"], "edge.source"),
        "target": parse_int(raw_edge["target"], "edge.target"),
    }


def normalize_graph(data: dict[str, Any]) -> dict[str, Any]:
    width = parse_int(data.get("width", DEFAULT_WIDTH), "width")
    height = parse_int(data.get("height", DEFAULT_HEIGHT), "height")

    if width < 0:
        raise ValueError("width must not be negative.")

    if height < 0:
        raise ValueError("height must not be negative.")

    nodes = [normalize_node(raw_node) for raw_node in data["nodes"]]
    edges = [normalize_edge(raw_edge) for raw_edge in data["edges"]]

    return {
        "width": width,
        "height": height,
        "nodes": nodes,
        "edges": edges,
    }


def normalize_file(input_path: Path, output_path: Path) -> None:
    data = read_graph(input_path)
    normalized = normalize_graph(data)

    output_path.parent.mkdir(parents=True, exist_ok=True)

    with output_path.open("w", encoding="utf-8") as file:
        json.dump(normalized, file, indent=2)
        file.write("\n")


def collect_input_files(input_path: Path) -> list[Path]:
    if input_path.is_file():
        return [input_path]

    if input_path.is_dir():
        return sorted(input_path.glob("*.json"))

    raise FileNotFoundError(f"Input path does not exist: {input_path}")


def output_path_for(input_file: Path, input_root: Path, output_path: Path) -> Path:
    if input_root.is_file():
        return output_path

    return output_path / input_file.name


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Normalize old contest JSON files to the internal solver JSON format."
    )

    parser.add_argument(
        "input",
        type=Path,
        help="Input JSON file or directory containing JSON files."
    )

    parser.add_argument(
        "output",
        type=Path,
        help="Output JSON file or output directory."
    )

    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    input_files = collect_input_files(args.input)
    if not input_files:
        raise ValueError(f"No JSON files found in input path: {args.input}")

    for input_file in input_files:
        output_file = output_path_for(input_file, args.input, args.output)
        normalize_file(input_file, output_file)
        print(f"OK: {input_file} -> {output_file}")


if __name__ == "__main__":
    main()
