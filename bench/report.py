"""Generate a Markdown benchmark report from results JSON.

Usage:
    python report.py                         # reads results/latest.json
    python report.py results/run1.json       # explicit path
    python report.py --format md             # Markdown (default)
    python report.py --format csv            # CSV table
"""

import argparse
import json
import sys
from pathlib import Path


def load_results(path: str) -> dict:
    with open(path) as f:
        return json.load(f)


def generate_markdown(data: dict) -> str:
    lines = []
    timestamp = data.get("timestamp", "unknown")
    drivers = data.get("drivers", {})
    driver_names = [d for d in drivers if "error" not in drivers[d] or drivers[d].get("tasks")]

    lines.append(f"# agent-browser-bench Results")
    lines.append(f"")
    lines.append(f"**Date:** {timestamp}")
    lines.append(f"**Drivers:** {', '.join(driver_names)}")
    lines.append("")

    if not driver_names:
        lines.append("No successful driver runs.")
        return "\n".join(lines)

    # Collect all task IDs across drivers
    all_task_ids = []
    seen = set()
    for dname in driver_names:
        for tid in drivers[dname].get("tasks", {}):
            if tid not in seen:
                all_task_ids.append(tid)
                seen.add(tid)

    # --- Per-task tables ---
    lines.append("## Detailed Results (ms)")
    lines.append("")

    # Header
    header_cols = ["Task"]
    for dname in driver_names:
        header_cols.extend([f"{dname} p50", f"{dname} p95", f"{dname} p99"])
    lines.append("| " + " | ".join(header_cols) + " |")
    lines.append("| " + " | ".join(["---"] * len(header_cols)) + " |")

    # Rows
    task_winners = {}
    for tid in all_task_ids:
        # Get display name
        task_name = tid
        for dname in driver_names:
            t = drivers[dname].get("tasks", {}).get(tid, {})
            if t.get("name"):
                task_name = t["name"]
                break

        row = [task_name]
        best_p50 = float("inf")
        best_driver = None

        for dname in driver_names:
            t = drivers[dname].get("tasks", {}).get(tid, {})
            stats = t.get("stats", {})
            if stats:
                p50 = stats.get("p50_ms", "-")
                p95 = stats.get("p95_ms", "-")
                p99 = stats.get("p99_ms", "-")
                row.extend([f"{p50}", f"{p95}", f"{p99}"])
                if isinstance(p50, (int, float)) and p50 < best_p50:
                    best_p50 = p50
                    best_driver = dname
            else:
                sr = t.get("success_rate", 0)
                row.extend(["FAIL", "FAIL", f"{sr:.0%}"])

        if best_driver:
            task_winners[tid] = (best_driver, best_p50)

        lines.append("| " + " | ".join(str(c) for c in row) + " |")

    lines.append("")

    # --- Winner summary ---
    lines.append("## Winners by Task (lowest p50)")
    lines.append("")
    lines.append("| Task | Winner | p50 (ms) |")
    lines.append("| --- | --- | --- |")
    for tid in all_task_ids:
        task_name = tid
        for dname in driver_names:
            t = drivers[dname].get("tasks", {}).get(tid, {})
            if t.get("name"):
                task_name = t["name"]
                break
        if tid in task_winners:
            winner, p50 = task_winners[tid]
            lines.append(f"| {task_name} | {winner} | {p50:.1f} |")
        else:
            lines.append(f"| {task_name} | - | - |")

    lines.append("")

    # --- Overall ranking ---
    lines.append("## Overall Ranking")
    lines.append("")
    lines.append("Computed by counting task wins (lowest p50) and breaking ties by average p50.")
    lines.append("")

    win_counts = {}
    avg_p50s = {}
    for dname in driver_names:
        win_counts[dname] = sum(
            1 for _, (w, _) in task_winners.items() if w == dname
        )
        p50_values = []
        for tid in all_task_ids:
            t = drivers[dname].get("tasks", {}).get(tid, {})
            stats = t.get("stats", {})
            p50 = stats.get("p50_ms")
            if isinstance(p50, (int, float)):
                p50_values.append(p50)
        avg_p50s[dname] = (
            sum(p50_values) / len(p50_values) if p50_values else float("inf")
        )

    ranked = sorted(
        driver_names,
        key=lambda d: (-win_counts[d], avg_p50s[d]),
    )

    lines.append("| Rank | Driver | Wins | Avg p50 (ms) |")
    lines.append("| --- | --- | --- | --- |")
    for i, dname in enumerate(ranked, 1):
        avg = avg_p50s[dname]
        avg_str = f"{avg:.1f}" if avg < float("inf") else "-"
        lines.append(f"| {i} | {dname} | {win_counts[dname]} | {avg_str} |")

    lines.append("")

    # --- Setup times ---
    lines.append("## Setup Times")
    lines.append("")
    lines.append("| Driver | Version | Setup (ms) |")
    lines.append("| --- | --- | --- |")
    for dname in driver_names:
        info = drivers[dname]
        version = info.get("version", "?")
        setup_ms = info.get("setup_ms", "-")
        lines.append(f"| {dname} | {version} | {setup_ms} |")

    lines.append("")
    return "\n".join(lines)


def generate_csv(data: dict) -> str:
    drivers = data.get("drivers", {})
    driver_names = [d for d in drivers if drivers[d].get("tasks")]

    all_task_ids = []
    seen = set()
    for dname in driver_names:
        for tid in drivers[dname].get("tasks", {}):
            if tid not in seen:
                all_task_ids.append(tid)
                seen.add(tid)

    header = ["task", "driver", "p50_ms", "p95_ms", "p99_ms", "mean_ms", "min_ms", "max_ms", "success_rate"]
    rows = [",".join(header)]

    for tid in all_task_ids:
        for dname in driver_names:
            t = drivers[dname].get("tasks", {}).get(tid, {})
            stats = t.get("stats", {})
            sr = t.get("success_rate", 0)
            row = [
                tid,
                dname,
                str(stats.get("p50_ms", "")),
                str(stats.get("p95_ms", "")),
                str(stats.get("p99_ms", "")),
                str(stats.get("mean_ms", "")),
                str(stats.get("min_ms", "")),
                str(stats.get("max_ms", "")),
                str(sr),
            ]
            rows.append(",".join(row))

    return "\n".join(rows)


def main():
    parser = argparse.ArgumentParser(description="Generate benchmark report")
    parser.add_argument(
        "input",
        nargs="?",
        default="results/latest.json",
        help="Path to results JSON (default: results/latest.json)",
    )
    parser.add_argument(
        "--format",
        choices=["md", "csv"],
        default="md",
        help="Output format (default: md)",
    )
    parser.add_argument(
        "--output",
        "-o",
        help="Write to file instead of stdout",
    )
    args = parser.parse_args()

    data = load_results(args.input)

    if args.format == "csv":
        report = generate_csv(data)
    else:
        report = generate_markdown(data)

    if args.output:
        Path(args.output).parent.mkdir(parents=True, exist_ok=True)
        with open(args.output, "w") as f:
            f.write(report)
        print(f"Report written to {args.output}")
    else:
        print(report)


if __name__ == "__main__":
    main()
