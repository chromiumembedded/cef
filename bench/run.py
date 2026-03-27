"""agent-browser-bench orchestrator.

Runs all benchmark tasks against selected browser drivers and writes
structured JSON results to disk.

Usage:
    python run.py                           # all drivers, all tasks
    python run.py --only zun,stagehand      # specific drivers
    python run.py --tasks cold_start,eval_simple  # specific tasks
    python run.py --output results/run1.json
"""

import argparse
import asyncio
import importlib
import json
import os
import statistics
import sys
import tempfile
import time
from pathlib import Path

# Ensure bench/ is importable
sys.path.insert(0, str(Path(__file__).parent))

from tasks import TASKS, TASK_MAP, PAGES
from drivers.base import BrowserDriver, TimedResult

# Driver registry: name -> (module_path, class_name)
DRIVER_REGISTRY = {
    "zun": ("drivers.zun_driver", "ZunDriver"),
    "agent-browser": ("drivers.agent_browser_driver", "AgentBrowserDriver"),
    "browser-use": ("drivers.browser_use_driver", "BrowserUseDriver"),
    "stagehand": ("drivers.stagehand_driver", "StagehandDriver"),
    "devtools-mcp": ("drivers.devtools_mcp_driver", "DevToolsMcpDriver"),
}

# URLs used by the navigate_10x task
NAV_10X_URLS = [
    "https://example.com",
    "https://news.ycombinator.com",
    "https://en.wikipedia.org/wiki/Chromium_(web_browser)",
    "https://httpbin.org/html",
    "https://www.w3.org/",
    "https://example.org",
    "https://jsonplaceholder.typicode.com",
    "https://httpbin.org/get",
    "https://www.iana.org",
    "https://tools.ietf.org",
]


def _load_driver(name: str) -> BrowserDriver:
    """Dynamically import and instantiate a driver by name."""
    if name not in DRIVER_REGISTRY:
        raise ValueError(
            f"Unknown driver: {name}. Available: {list(DRIVER_REGISTRY)}"
        )
    module_path, class_name = DRIVER_REGISTRY[name]
    module = importlib.import_module(module_path)
    cls = getattr(module, class_name)
    return cls()


def _result_to_dict(r: TimedResult) -> dict:
    """Serialize a TimedResult for JSON output."""
    d = {
        "duration_ms": round(r.duration_ms, 3),
        "success": r.success,
    }
    if r.error:
        d["error"] = r.error
    # Omit value for brevity (snapshots can be huge)
    return d


async def run_task(driver: BrowserDriver, task) -> list[dict]:
    """Run a single task for the configured number of iterations.

    Returns a list of serialized TimedResult dicts.
    """
    results = []

    for i in range(task.iterations):
        # Pre-navigate for tasks that need a specific page already loaded
        if task.url and task.id not in ("cold_start", "navigate_complex"):
            pre = await driver.navigate(task.url)
            if not pre.success:
                results.append(_result_to_dict(pre))
                continue

        if task.id == "cold_start":
            await driver.teardown()
            result = await driver.setup()
            if result.success:
                nav = await driver.navigate(task.url)
                result = TimedResult(
                    duration_ms=result.duration_ms + nav.duration_ms,
                    success=nav.success,
                    error=nav.error,
                )

        elif task.id == "navigate_complex":
            result = await driver.navigate(task.url)

        elif task.id == "snapshot_full":
            result = await driver.snapshot(interactive_only=False)

        elif task.id == "snapshot_interactive":
            result = await driver.snapshot(interactive_only=True)

        elif task.id == "screenshot_plain":
            path = os.path.join(
                tempfile.gettempdir(),
                f"bench_{driver.name}_plain_{i}.png",
            )
            result = await driver.screenshot(path, annotate=False)

        elif task.id == "screenshot_annotated":
            path = os.path.join(
                tempfile.gettempdir(),
                f"bench_{driver.name}_annotated_{i}.png",
            )
            result = await driver.screenshot(path, annotate=True)

        elif task.id == "eval_simple":
            result = await driver.eval_js(task.js_code)

        elif task.id == "eval_complex":
            result = await driver.eval_js(task.js_code)

        elif task.id == "click_wait_snapshot":
            click_result = await driver.click(task.selector)
            if click_result.success:
                snap = await driver.snapshot(interactive_only=False)
                result = TimedResult(
                    duration_ms=click_result.duration_ms + snap.duration_ms,
                    success=snap.success,
                    error=snap.error,
                )
            else:
                result = click_result

        elif task.id == "navigate_10x":
            total_ms = 0.0
            all_ok = True
            for url in NAV_10X_URLS:
                nav = await driver.navigate(url)
                total_ms += nav.duration_ms
                if not nav.success:
                    all_ok = False
                    break
            result = TimedResult(
                duration_ms=total_ms,
                success=all_ok,
                error=None if all_ok else "Navigation failed mid-sequence",
            )

        elif task.id == "state_save_load":
            save_path = os.path.join(
                tempfile.gettempdir(),
                f"bench_{driver.name}_state_{i}.json",
            )
            save_result = await driver.save_state(save_path)
            if save_result.success:
                load_result = await driver.load_state(save_path)
                result = TimedResult(
                    duration_ms=save_result.duration_ms + load_result.duration_ms,
                    success=load_result.success,
                    error=load_result.error,
                )
            else:
                result = save_result

        else:
            result = TimedResult(
                duration_ms=0,
                success=False,
                error=f"Unknown task: {task.id}",
            )

        results.append(_result_to_dict(result))

    return results


def compute_stats(durations: list[float]) -> dict:
    """Compute p50/p95/p99/mean/stdev from a list of durations in ms."""
    if not durations:
        return {}
    durations_sorted = sorted(durations)
    n = len(durations_sorted)

    def percentile(p: float) -> float:
        idx = (p / 100) * (n - 1)
        lo = int(idx)
        hi = min(lo + 1, n - 1)
        frac = idx - lo
        return durations_sorted[lo] + frac * (durations_sorted[hi] - durations_sorted[lo])

    stats = {
        "count": n,
        "mean_ms": round(statistics.mean(durations), 3),
        "p50_ms": round(percentile(50), 3),
        "p95_ms": round(percentile(95), 3),
        "p99_ms": round(percentile(99), 3),
        "min_ms": round(durations_sorted[0], 3),
        "max_ms": round(durations_sorted[-1], 3),
    }
    if n > 1:
        stats["stdev_ms"] = round(statistics.stdev(durations), 3)
    return stats


async def main():
    parser = argparse.ArgumentParser(
        description="agent-browser-bench: benchmark browser automation frameworks"
    )
    parser.add_argument(
        "--only",
        help="Comma-separated driver names to run (default: all)",
    )
    parser.add_argument(
        "--tasks",
        help="Comma-separated task IDs to run (default: all)",
    )
    parser.add_argument(
        "--output",
        default="results/latest.json",
        help="Output JSON path (default: results/latest.json)",
    )
    parser.add_argument(
        "--warmup",
        type=int,
        default=1,
        help="Number of warmup iterations before measurement (default: 1)",
    )
    args = parser.parse_args()

    # Resolve drivers
    driver_names = (
        [d.strip() for d in args.only.split(",")]
        if args.only
        else list(DRIVER_REGISTRY)
    )

    # Resolve tasks
    if args.tasks:
        task_ids = [t.strip() for t in args.tasks.split(",")]
        tasks = [TASK_MAP[tid] for tid in task_ids if tid in TASK_MAP]
        unknown = [t for t in task_ids if t not in TASK_MAP]
        if unknown:
            print(f"Warning: unknown task IDs ignored: {unknown}")
    else:
        tasks = TASKS

    # Prepare output directory
    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    all_results = {
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "drivers": {},
    }

    for dname in driver_names:
        print(f"\n{'='*60}")
        print(f"Driver: {dname}")
        print(f"{'='*60}")

        try:
            driver = _load_driver(dname)
        except Exception as e:
            print(f"  SKIP: failed to load driver: {e}")
            all_results["drivers"][dname] = {"error": str(e), "tasks": {}}
            continue

        # Setup
        setup_result = await driver.setup()
        if not setup_result.success:
            print(f"  SKIP: setup failed: {setup_result.error}")
            all_results["drivers"][dname] = {
                "error": f"Setup failed: {setup_result.error}",
                "tasks": {},
            }
            continue

        driver_results = {
            "version": driver.version,
            "setup_ms": round(setup_result.duration_ms, 3),
            "tasks": {},
        }

        for task in tasks:
            print(f"  Running: {task.name} ({task.iterations} iterations)...", end=" ", flush=True)

            # Warmup
            if args.warmup > 0 and task.id != "cold_start":
                for _ in range(args.warmup):
                    try:
                        if task.url:
                            await driver.navigate(task.url)
                    except Exception:
                        pass

            try:
                task_results = await run_task(driver, task)
            except Exception as e:
                task_results = [
                    {"duration_ms": 0, "success": False, "error": str(e)}
                ]

            # Compute stats from successful runs
            ok_durations = [
                r["duration_ms"] for r in task_results if r["success"]
            ]
            stats = compute_stats(ok_durations)
            success_rate = (
                len(ok_durations) / len(task_results) if task_results else 0
            )

            driver_results["tasks"][task.id] = {
                "name": task.name,
                "category": task.category,
                "iterations": task_results,
                "stats": stats,
                "success_rate": round(success_rate, 3),
            }

            if stats:
                print(
                    f"p50={stats['p50_ms']:.1f}ms  "
                    f"p95={stats['p95_ms']:.1f}ms  "
                    f"({len(ok_durations)}/{len(task_results)} ok)"
                )
            else:
                print(f"ALL FAILED ({len(task_results)} attempts)")

        # Teardown
        await driver.teardown()
        all_results["drivers"][dname] = driver_results

    # Write results
    with open(output_path, "w") as f:
        json.dump(all_results, f, indent=2)
    print(f"\nResults written to {output_path}")


if __name__ == "__main__":
    asyncio.run(main())
