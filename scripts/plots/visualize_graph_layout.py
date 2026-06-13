#!/usr/bin/env python3
import argparse
import json
import re
import subprocess
from pathlib import Path
from typing import Any, Optional

import matplotlib.pyplot as plt


def load_graph(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as file:
        return json.load(file)


def node_lookup(graph: dict[str, Any]) -> dict[int, dict[str, Any]]:
    return {int(node["id"]): node for node in graph.get("nodes", [])}


def parse_solver_score(output: str) -> tuple[Optional[int], Optional[int]]:
    k = None
    crossings = None

    for line in output.splitlines():
        if line.startswith("K: "):
            k = int(line.split(": ", 1)[1])
        elif line.startswith("Crossings: "):
            crossings = int(line.split(": ", 1)[1])

    return k, crossings


def score_graph(path: Path, solver: Optional[Path]) -> tuple[Optional[int], Optional[int]]:
    if solver is None:
        return None, None

    if not solver.exists():
        return None, None

    try:
        result = subprocess.run(
            [str(solver), str(path)],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.SubprocessError:
        return None, None

    return parse_solver_score(result.stdout)


def title_for(path: Path, solver: Optional[Path], custom_title: Optional[str]) -> str:
    k, crossings = score_graph(path, solver)
    base = custom_title if custom_title else path.name

    if k is None or crossings is None:
        return base

    return f"{base}\nk={k}, crossings={crossings}"


def should_label_nodes(graph: dict[str, Any], force_labels: bool, max_label_nodes: int) -> bool:
    return force_labels or len(graph.get("nodes", [])) <= max_label_nodes


def draw_graph(
    ax: plt.Axes,
    graph: dict[str, Any],
    path: Path,
    solver: Optional[Path],
    custom_title: Optional[str],
    force_labels: bool,
    max_label_nodes: int,
) -> None:
    nodes = node_lookup(graph)
    draw_labels = should_label_nodes(graph, force_labels, max_label_nodes)

    for edge in graph.get("edges", []):
        source_id = int(edge["source"])
        target_id = int(edge["target"])

        if source_id not in nodes or target_id not in nodes:
            continue

        source = nodes[source_id]
        target = nodes[target_id]

        ax.plot(
            [float(source["x"]), float(target["x"])],
            [float(source["y"]), float(target["y"])],
            color="0.65",
            linewidth=0.7,
            zorder=1,
        )

    xs = [float(node["x"]) for node in graph.get("nodes", [])]
    ys = [float(node["y"]) for node in graph.get("nodes", [])]
    colors = ["tab:red" if node.get("fixed") is True else "tab:blue" for node in graph.get("nodes", [])]

    ax.scatter(xs, ys, s=18, c=colors, zorder=2)

    if draw_labels:
        for node in graph.get("nodes", []):
            ax.text(float(node["x"]), float(node["y"]), str(node["id"]), fontsize=7, zorder=3)

    ax.set_title(title_for(path, solver, custom_title))
    ax.set_aspect("equal", adjustable="box")
    ax.grid(alpha=0.25)


def visualize_single(args: argparse.Namespace) -> None:
    graph = load_graph(args.input)
    fig, ax = plt.subplots(figsize=(8, 8))
    draw_graph(
        ax,
        graph,
        args.input,
        args.solver,
        args.title,
        args.node_labels,
        args.max_label_nodes,
    )
    plt.tight_layout()
    fig.savefig(args.output_image, dpi=180)
    plt.close(fig)
    print(f"Wrote {args.output_image}")


def visualize_pair(args: argparse.Namespace) -> None:
    input_graph = load_graph(args.input)
    optimized_graph = load_graph(args.optimized)

    fig, axes = plt.subplots(1, 2, figsize=(15, 7.5))
    draw_graph(
        axes[0],
        input_graph,
        args.input,
        args.solver,
        args.left_title or "Original / Input",
        args.node_labels,
        args.max_label_nodes,
    )
    draw_graph(
        axes[1],
        optimized_graph,
        args.optimized,
        args.solver,
        args.right_title or "Optimiert / Output",
        args.node_labels,
        args.max_label_nodes,
    )
    plt.tight_layout()
    fig.savefig(args.output_image, dpi=180)
    plt.close(fig)
    print(f"Wrote {args.output_image}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Visualize graph JSON layouts with k/crossing annotations.")
    parser.add_argument("--input", required=True, type=Path, help="Input/original graph JSON")
    parser.add_argument("--optimized", type=Path, help="Optional optimized graph JSON for side-by-side plot")
    parser.add_argument("--output-image", required=True, type=Path, help="Output PNG path")
    parser.add_argument("--solver", type=Path, default=Path("./build/solver"), help="Solver executable used to read k/crossings")
    parser.add_argument("--title", help="Title for single graph mode")
    parser.add_argument("--left-title", help="Left title in side-by-side mode")
    parser.add_argument("--right-title", help="Right title in side-by-side mode")
    parser.add_argument("--node-labels", action="store_true", help="Force node labels")
    parser.add_argument("--max-label-nodes", type=int, default=40, help="Automatically label nodes up to this count")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    args.output_image.parent.mkdir(parents=True, exist_ok=True)

    if args.optimized is None:
        visualize_single(args)
    else:
        visualize_pair(args)


if __name__ == "__main__":
    main()
