
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import tarfile
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_WIDTH = 1_000_000
DEFAULT_HEIGHT = 1_000_000


@dataclass(frozen=True)
class ParsedGraph:
    source_name: str
    node_count: int
    edges: list[tuple[int, int]]


def strip_namespace(tag: str) -> str:
    if "}" in tag:
        return tag.rsplit("}", 1)[1]
    return tag


def child_elements_by_name(root: ET.Element, local_name: str) -> list[ET.Element]:
    return [element for element in root.iter() if strip_namespace(element.tag) == local_name]


def parse_graphml(content: bytes, source_name: str) -> ParsedGraph:
    root = ET.fromstring(content)

    graph_elements = child_elements_by_name(root, "graph")
    if not graph_elements:
        raise ValueError(f"{source_name}: no graph element found.")

    graph = graph_elements[0]

    node_elements = [
        element
        for element in graph
        if strip_namespace(element.tag) == "node"
    ]

    edge_elements = [
        element
        for element in graph
        if strip_namespace(element.tag) == "edge"
    ]

    id_to_index: dict[str, int] = {}

    for node in node_elements:
        node_id = node.get("id")
        if node_id is None:
            raise ValueError(f"{source_name}: node without id.")

        if node_id not in id_to_index:
            id_to_index[node_id] = len(id_to_index)

    edges: list[tuple[int, int]] = []
    seen_edges: set[tuple[int, int]] = set()

    for edge in edge_elements:
        source = edge.get("source")
        target = edge.get("target")

        if source is None or target is None:
            raise ValueError(f"{source_name}: edge without source/target.")

        if source not in id_to_index or target not in id_to_index:
            raise ValueError(f"{source_name}: edge references unknown node.")

        u = id_to_index[source]
        v = id_to_index[target]

        if u == v:
            continue

        key = (min(u, v), max(u, v))
        if key in seen_edges:
            continue

        seen_edges.add(key)
        edges.append(key)

    return ParsedGraph(
        source_name=source_name,
        node_count=len(id_to_index),
        edges=edges,
    )


def iter_graphml_from_archive(path: Path) -> Iterable[tuple[str, bytes]]:
    with tarfile.open(path, mode="r:*") as archive:
        for member in archive.getmembers():
            if not member.isfile():
                continue

            lower_name = member.name.lower()
            if not lower_name.endswith(".graphml") and not lower_name.endswith(".xml"):
                continue

            handle = archive.extractfile(member)
            if handle is None:
                continue

            yield f"{path.name}:{member.name}", handle.read()


def iter_graphml_inputs(input_dir: Path) -> Iterable[tuple[str, bytes]]:
    archive_paths = sorted(
        path
        for path in input_dir.iterdir()
        if path.is_file() and path.suffix.lower() in {".tgz", ".gz", ".tar"}
    )

    graphml_paths = sorted(input_dir.rglob("*.graphml"))

    for archive_path in archive_paths:
        yield from iter_graphml_from_archive(archive_path)

    for graphml_path in graphml_paths:
        yield str(graphml_path), graphml_path.read_bytes()


def parse_all_graphs(input_dir: Path) -> list[ParsedGraph]:
    graphs: list[ParsedGraph] = []
    for source_name, content in iter_graphml_inputs(input_dir):
        try:
            graph = parse_graphml(content, source_name)
        except (ET.ParseError, ValueError) as exception:
            print(f"SKIP: {source_name}: {exception}")
            continue

        if graph.node_count == 0 or not graph.edges:
            continue

        graphs.append(graph)

    graphs.sort(key=lambda graph: (graph.node_count, len(graph.edges), graph.source_name))
    return graphs


def select_by_targets(
    graphs: list[ParsedGraph],
    targets: list[int],
) -> list[ParsedGraph]:
    selected: list[ParsedGraph] = []
    used_names: set[str] = set()

    for target in targets:
        best_graph: ParsedGraph | None = None
        best_key: tuple[int, int, str] | None = None

        for graph in graphs:
            if graph.source_name in used_names:
                continue

            key = (
                abs(graph.node_count - target),
                len(graph.edges),
                graph.source_name,
            )

            if best_key is None or key < best_key:
                best_key = key
                best_graph = graph

        if best_graph is not None:
            selected.append(best_graph)
            used_names.add(best_graph.source_name)

    return selected


def circular_coordinates(node_count: int, width: int, height: int) -> list[tuple[float, float]]:
    if node_count == 0:
        return []

    center_x = width / 2.0
    center_y = height / 2.0
    radius = min(width, height) * 0.4

    coordinates: list[tuple[float, float]] = []

    for index in range(node_count):
        angle = 2.0 * math.pi * index / node_count
        coordinates.append((
            center_x + radius * math.cos(angle),
            center_y + radius * math.sin(angle),
        ))

    return coordinates


def graph_to_json(graph: ParsedGraph, width: int, height: int) -> dict:
    coordinates = circular_coordinates(graph.node_count, width, height)

    return {
        "width": width,
        "height": height,
        "nodes": [
            {
                "id": node_id,
                "x": coordinates[node_id][0],
                "y": coordinates[node_id][1],
            }
            for node_id in range(graph.node_count)
        ],
        "edges": [
            {
                "source": source,
                "target": target,
            }
            for source, target in graph.edges
        ],
    }


def safe_stem(source_name: str) -> str:
    value = source_name.replace(":", "_").replace("/", "_").replace("\\", "_")
    value = Path(value).stem
    allowed = []

    for char in value:
        if char.isalnum() or char in {"_", "-", "."}:
            allowed.append(char)
        else:
            allowed.append("_")

    return "".join(allowed).strip("_") or "rome_graph"


def write_graph_json(
    output_dir: Path,
    graph: ParsedGraph,
    width: int,
    height: int,
    index: int,
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)

    stem = safe_stem(graph.source_name)
    output_path = output_dir / f"{index:03d}_{stem}_n{graph.node_count}_m{len(graph.edges)}.json"

    with output_path.open("w", encoding="utf-8") as file:
        json.dump(graph_to_json(graph, width, height), file, indent=2)
        file.write("\n")

    return output_path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import local Rome GraphML .tgz archives into the solver JSON format."
    )

    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing .tgz/.tar.gz archives or extracted .graphml files.",
    )

    parser.add_argument(
        "--output-dir",
        type=Path,
        required=True,
        help="Directory for generated solver JSON files.",
    )

    parser.add_argument(
        "--targets",
        type=int,
        nargs="+",
        default=[10, 20, 30, 40, 50, 60, 70, 80, 90, 100],
        help="Target node counts. One closest graph is selected per target.",
    )

    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert every graph. This can create thousands of files.",
    )

    parser.add_argument(
        "--width",
        type=int,
        default=DEFAULT_WIDTH,
        help="Output drawing width.",
    )

    parser.add_argument(
        "--height",
        type=int,
        default=DEFAULT_HEIGHT,
        help="Output drawing height.",
    )

    return parser.parse_args()


def main() -> None:
    args = parse_arguments()

    if not args.input_dir.exists():
        raise FileNotFoundError(f"Input directory does not exist: {args.input_dir}")

    graphs = parse_all_graphs(args.input_dir)
    if not graphs:
        raise RuntimeError(f"No usable GraphML graphs found in {args.input_dir}")

    print(
        f"Parsed {len(graphs)} graphs "
        f"(nodes {min(g.node_count for g in graphs)}..{max(g.node_count for g in graphs)}, "
        f"edges {min(len(g.edges) for g in graphs)}..{max(len(g.edges) for g in graphs)})"
    )

    selected = graphs if args.all else select_by_targets(graphs, args.targets)

    print(f"Writing {len(selected)} JSON graphs to {args.output_dir}")

    for index, graph in enumerate(selected):
        output_path = write_graph_json(
            args.output_dir,
            graph,
            args.width,
            args.height,
            index,
        )

        print(
            f"OK: {output_path} "
            f"nodes={graph.node_count} edges={len(graph.edges)} "
            f"source={graph.source_name}"
        )


if __name__ == "__main__":
    main()
