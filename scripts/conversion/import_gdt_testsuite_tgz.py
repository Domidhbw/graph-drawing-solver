
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
import re
import tarfile
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_WIDTH = 1_000_000
DEFAULT_HEIGHT = 1_000_000

NODE_START_RE = re.compile(r"^\s*<NODE>\s*$")
NODE_END_RE = re.compile(r"^\s*</NODE>\s*$")
EDGE_RE = re.compile(r"^\s*<EDGE>\s+([A-Za-z0-9_.:-]+)\s+(?:->|<-)\s*$")
INTEGER_RE = re.compile(r"^\s*(-?\d+)\s*$")


@dataclass(frozen=True)
class ParsedGraph:
    source_name: str
    node_count: int
    edges: list[tuple[int, int]]


def is_readme_or_directory(member_name: str) -> bool:
    lower_name = member_name.lower()

    if lower_name.endswith("/"):
        return True

    file_name = Path(member_name).name.lower()

    return file_name in {
        "readme",
        "readme.txt",
    }


def looks_like_graph_file(member_name: str) -> bool:
    if is_readme_or_directory(member_name):
        return False

    file_name = Path(member_name).name

    # GDT files in the downloaded archives use names such as:
    #   ug2.100
    #   ug1.10
    #   g.00010.01
    # They usually do not have graphml/xml extensions.
    return "." in file_name or file_name.startswith("g")


def iter_archive_members(input_dir: Path) -> Iterable[tuple[str, bytes]]:
    archive_paths = sorted(
        path
        for path in input_dir.iterdir()
        if path.is_file() and path.suffix.lower() in {".tgz", ".gz", ".tar"}
    )

    for archive_path in archive_paths:
        with tarfile.open(archive_path, mode="r:*") as archive:
            for member in archive.getmembers():
                if not member.isfile():
                    continue

                if not looks_like_graph_file(member.name):
                    continue

                handle = archive.extractfile(member)
                if handle is None:
                    continue

                yield f"{archive_path.name}:{member.name}", handle.read()


def parse_gdt_graph(content: bytes, source_name: str) -> ParsedGraph:
    text = content.decode("utf-8", errors="replace")
    lines = text.splitlines()

    node_ids_in_order: list[int] = []
    edge_to_nodes: dict[str, list[int]] = {}

    inside_node = False
    current_node: int | None = None
    waiting_for_node_id = False

    for line in lines:
        if NODE_START_RE.match(line):
            inside_node = True
            current_node = None
            waiting_for_node_id = True
            continue

        if NODE_END_RE.match(line):
            inside_node = False
            current_node = None
            waiting_for_node_id = False
            continue

        if not inside_node:
            continue

        if waiting_for_node_id:
            match = INTEGER_RE.match(line)
            if match is None:
                continue

            current_node = int(match.group(1))
            node_ids_in_order.append(current_node)
            waiting_for_node_id = False
            continue

        if current_node is None:
            continue

        edge_match = EDGE_RE.match(line)
        if edge_match is None:
            continue

        edge_id = edge_match.group(1)
        edge_to_nodes.setdefault(edge_id, []).append(current_node)

    if not node_ids_in_order:
        raise ValueError(f"{source_name}: no nodes parsed.")

    unique_node_ids = sorted(set(node_ids_in_order))
    id_to_index = {
        node_id: index
        for index, node_id in enumerate(unique_node_ids)
    }

    edges: list[tuple[int, int]] = []
    seen_edges: set[tuple[int, int]] = set()

    for edge_id, incident_nodes in edge_to_nodes.items():
        unique_incident_nodes = list(dict.fromkeys(incident_nodes))

        if len(unique_incident_nodes) != 2:
            # Skip malformed, dangling, hyperedge-like, or loop-like entries.
            continue

        u_original = unique_incident_nodes[0]
        v_original = unique_incident_nodes[1]

        if u_original == v_original:
            continue

        u = id_to_index[u_original]
        v = id_to_index[v_original]

        key = (min(u, v), max(u, v))
        if key in seen_edges:
            continue

        seen_edges.add(key)
        edges.append(key)

    if not edges:
        raise ValueError(f"{source_name}: no usable edges parsed.")

    edges.sort()

    return ParsedGraph(
        source_name=source_name,
        node_count=len(unique_node_ids),
        edges=edges,
    )


def parse_all_graphs(input_dir: Path) -> list[ParsedGraph]:
    graphs: list[ParsedGraph] = []

    for source_name, content in iter_archive_members(input_dir):
        try:
            graph = parse_gdt_graph(content, source_name)
        except ValueError as exception:
            print(f"SKIP: {source_name}: {exception}")
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


def select_fraction(
    graphs: list[ParsedGraph],
    fraction: float,
) -> list[ParsedGraph]:
    if not 0.0 < fraction <= 1.0:
        raise ValueError("fraction must be in (0, 1].")

    if not graphs:
        return []

    count = max(1, int(round(len(graphs) * fraction)))

    if count >= len(graphs):
        return graphs

    if count == 1:
        return [graphs[0]]

    selected: list[ParsedGraph] = []
    used_indices: set[int] = set()

    for i in range(count):
        index = round(i * (len(graphs) - 1) / (count - 1))

        if index in used_indices:
            continue

        used_indices.add(index)
        selected.append(graphs[index])

    return selected


def circular_coordinates(node_count: int, width: int, height: int) -> list[tuple[float, float]]:
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
    allowed: list[str] = []

    for char in value:
        if char.isalnum() or char in {"_", "-", "."}:
            allowed.append(char)
        else:
            allowed.append("_")

    return "".join(allowed).strip("_") or "gdt_graph"


def write_graph_json(
    output_dir: Path,
    graph: ParsedGraph,
    width: int,
    height: int,
    index: int,
) -> Path:
    output_dir.mkdir(parents=True, exist_ok=True)

    stem = safe_stem(graph.source_name)
    output_path = output_dir / f"{index:05d}_{stem}_n{graph.node_count}_m{len(graph.edges)}.json"

    with output_path.open("w", encoding="utf-8") as file:
        json.dump(graph_to_json(graph, width, height), file, indent=2)
        file.write("\n")

    return output_path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Import local GDT/Rome test-suite .tgz archives into the solver JSON format."
    )

    parser.add_argument(
        "--input-dir",
        type=Path,
        required=True,
        help="Directory containing GDT .tgz archives.",
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
        default=None,
        help="Target node counts. One closest graph is selected per target.",
    )

    parser.add_argument(
        "--fraction",
        type=float,
        default=None,
        help="Select a deterministic fraction of all parsed graphs, e.g. 0.75.",
    )

    parser.add_argument(
        "--all",
        action="store_true",
        help="Convert every parsed graph. This can create many files.",
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


def select_graphs(args: argparse.Namespace, graphs: list[ParsedGraph]) -> list[ParsedGraph]:
    selection_modes = sum([
        args.targets is not None,
        args.fraction is not None,
        args.all,
    ])

    if selection_modes != 1:
        raise ValueError("Choose exactly one selection mode: --targets, --fraction, or --all.")

    if args.targets is not None:
        return select_by_targets(graphs, args.targets)

    if args.fraction is not None:
        return select_fraction(graphs, args.fraction)

    return graphs


def main() -> None:
    args = parse_arguments()

    if not args.input_dir.exists():
        raise FileNotFoundError(f"Input directory does not exist: {args.input_dir}")

    graphs = parse_all_graphs(args.input_dir)

    if not graphs:
        raise RuntimeError(f"No usable GDT graphs found in {args.input_dir}")

    print(
        f"Parsed {len(graphs)} graphs "
        f"(nodes {min(g.node_count for g in graphs)}..{max(g.node_count for g in graphs)}, "
        f"edges {min(len(g.edges) for g in graphs)}..{max(len(g.edges) for g in graphs)})"
    )

    selected = select_graphs(args, graphs)

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
