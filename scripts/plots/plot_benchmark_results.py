#!/usr/bin/env python3
import argparse
from pathlib import Path

import matplotlib.pyplot as plt
import pandas as pd


def instance_label(graph_path: str) -> str:
    path = Path(str(graph_path))
    return path.stem


def ensure_output_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def save_current(path: Path) -> None:
    plt.tight_layout()
    plt.savefig(path, dpi=180)
    plt.close()
    print(f"Wrote {path}")


def plot_best_k_comparison(summary: pd.DataFrame, output_dir: Path, name: str) -> None:
    labels = summary["label"].tolist()
    x = range(len(summary))
    width = 0.25

    plt.figure(figsize=(max(10, len(labels) * 1.2), 5.5))
    plt.bar([i - width for i in x], summary["original_k"], width=width, label="Original")
    plt.bar(list(x), summary["best_fr_k"], width=width, label="FR")
    plt.bar([i + width for i in x], summary["best_sa_k"], width=width, label="SA best")
    plt.xticks(list(x), labels, rotation=35, ha="right")
    plt.ylabel("k = max. Kreuzungen pro Kante")
    plt.title("Vergleich der k-Werte pro Instanz")
    plt.legend()
    plt.grid(axis="y", alpha=0.3)
    save_current(output_dir / f"{name}_best_k_comparison.png")


def plot_sa_improvement(summary: pd.DataFrame, output_dir: Path, name: str) -> None:
    labels = summary["label"].tolist()
    improvement = summary["original_k"] - summary["best_sa_k"]
    improvement_pct = improvement / summary["original_k"] * 100.0

    fig, ax1 = plt.subplots(figsize=(max(10, len(labels) * 1.2), 5.5))
    bars = ax1.bar(labels, improvement)
    ax1.set_ylabel("absolute k-Reduktion")
    ax1.set_title("Verbesserung durch SA gegenüber dem Originallayout")
    ax1.tick_params(axis="x", rotation=35)
    ax1.grid(axis="y", alpha=0.3)

    ax2 = ax1.twinx()
    ax2.plot(labels, improvement_pct, marker="o", linestyle="--", label="Reduktion in %")
    ax2.set_ylabel("k-Reduktion in %")

    for bar, pct in zip(bars, improvement_pct):
        ax1.text(
            bar.get_x() + bar.get_width() / 2.0,
            bar.get_height(),
            f"{pct:.1f}%",
            ha="center",
            va="bottom",
            fontsize=8,
        )

    save_current(output_dir / f"{name}_sa_improvement.png")


def plot_runtime_per_graph(runtime: pd.DataFrame, output_dir: Path, name: str) -> None:
    labels = runtime["label"].tolist()
    seconds = runtime["avg_duration_ms"] / 1000.0

    plt.figure(figsize=(max(10, len(labels) * 1.2), 5.5))
    plt.bar(labels, seconds)
    plt.xticks(rotation=35, ha="right")
    plt.ylabel("durchschnittliche Laufzeit [s]")
    plt.title("Durchschnittliche Laufzeit pro Instanz")
    plt.grid(axis="y", alpha=0.3)
    save_current(output_dir / f"{name}_runtime_per_graph.png")


def plot_runtime_vs_edges(runtime: pd.DataFrame, output_dir: Path, name: str) -> None:
    seconds = runtime["avg_duration_ms"] / 1000.0

    plt.figure(figsize=(8, 5.5))
    plt.scatter(runtime["edges"], seconds)
    for _, row in runtime.iterrows():
        plt.annotate(row["label"], (row["edges"], row["avg_duration_ms"] / 1000.0), fontsize=8)
    plt.xlabel("Kanten")
    plt.ylabel("durchschnittliche Laufzeit [s]")
    plt.title("Laufzeit in Abhängigkeit von der Kantenzahl")
    plt.grid(alpha=0.3)
    save_current(output_dir / f"{name}_runtime_vs_edges.png")


def plot_sa_stability(runtime: pd.DataFrame, output_dir: Path, name: str) -> None:
    labels = runtime["label"].tolist()
    x = range(len(runtime))
    width = 0.25

    plt.figure(figsize=(max(10, len(labels) * 1.2), 5.5))
    plt.bar([i - width for i in x], runtime["best_sa_k"], width=width, label="best")
    plt.bar(list(x), runtime["median_sa_k"], width=width, label="median")
    plt.bar([i + width for i in x], runtime["worst_sa_k"], width=width, label="worst")
    plt.xticks(list(x), labels, rotation=35, ha="right")
    plt.ylabel("k")
    plt.title("Seed-Stabilität der SA-Ergebnisse")
    plt.legend()
    plt.grid(axis="y", alpha=0.3)
    save_current(output_dir / f"{name}_sa_stability.png")


def plot_crossings(summary: pd.DataFrame, output_dir: Path, name: str) -> None:
    labels = summary["label"].tolist()

    plt.figure(figsize=(max(10, len(labels) * 1.2), 5.5))
    plt.bar(labels, summary["best_sa_crossings"])
    plt.xticks(rotation=35, ha="right")
    plt.ylabel("Kreuzungen")
    plt.title("Gesamtzahl der Kreuzungen im besten SA-Ergebnis")
    plt.grid(axis="y", alpha=0.3)
    save_current(output_dir / f"{name}_best_sa_crossings.png")


def write_key_metrics(summary: pd.DataFrame, runtime: pd.DataFrame, output_dir: Path, name: str) -> None:
    reduction = summary["original_k"] - summary["best_sa_k"]
    reduction_pct = reduction / summary["original_k"] * 100.0

    metrics = {
        "instances": len(summary),
        "avg_original_k": summary["original_k"].mean(),
        "avg_best_sa_k": summary["best_sa_k"].mean(),
        "avg_absolute_k_reduction": reduction.mean(),
        "avg_relative_k_reduction_percent": reduction_pct.mean(),
        "min_sa_valid_rate": summary["sa_valid_rate"].min(),
        "avg_duration_ms": runtime["avg_duration_ms"].mean(),
        "max_duration_ms": runtime["avg_duration_ms"].max(),
    }

    path = output_dir / f"{name}_key_metrics.txt"
    with path.open("w", encoding="utf-8") as file:
        for key, value in metrics.items():
            if isinstance(value, float):
                file.write(f"{key}: {value:.4f}\n")
            else:
                file.write(f"{key}: {value}\n")

    print(f"Wrote {path}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create benchmark plots for the seminar thesis.")
    parser.add_argument("--summary", required=True, type=Path, help="Summary CSV, e.g. contest_old_5k_summary.csv")
    parser.add_argument("--runtime", required=True, type=Path, help="Runtime CSV, e.g. contest_old_5k_runtime.csv")
    parser.add_argument("--budget", required=False, type=Path, help="Budget CSV, currently optional")
    parser.add_argument("--output-dir", required=True, type=Path, help="Directory for generated figures")
    parser.add_argument("--name", required=True, help="Prefix for output files")
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    ensure_output_dir(args.output_dir)

    summary = pd.read_csv(args.summary)
    runtime = pd.read_csv(args.runtime)

    summary["label"] = summary["graph"].apply(instance_label)
    runtime["label"] = runtime["graph"].apply(instance_label)

    plot_best_k_comparison(summary, args.output_dir, args.name)
    plot_sa_improvement(summary, args.output_dir, args.name)
    plot_runtime_per_graph(runtime, args.output_dir, args.name)
    plot_runtime_vs_edges(runtime, args.output_dir, args.name)
    plot_sa_stability(runtime, args.output_dir, args.name)
    plot_crossings(summary, args.output_dir, args.name)
    write_key_metrics(summary, runtime, args.output_dir, args.name)


if __name__ == "__main__":
    main()
