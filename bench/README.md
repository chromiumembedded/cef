# agent-browser-bench

Benchmark suite comparing browser automation frameworks for agent use cases.
Measures raw infrastructure latency -- no LLM in the loop.

## Drivers

| Driver | Framework | Method |
| --- | --- | --- |
| `zun` | CEF (our browser) | CDP over WebSocket |
| `agent-browser` | Vercel agent-browser | CLI subprocess |
| `browser-use` | browser-use | CLI subprocess |
| `stagehand` | Stagehand (Browserbase) | Node.js subprocess (persistent) |
| `devtools-mcp` | Chrome DevTools MCP | MCP stdio JSON-RPC |

## Tasks

See `tasks.py` for the full list. Categories: startup, navigation, snapshot,
screenshot, eval, compound, throughput, state.

## Setup

```bash
# Python dependencies
pip install -r requirements.txt

# Node dependencies (for stagehand and devtools-mcp drivers)
npm install
```

## Usage

```bash
# Run all drivers, all tasks
python run.py

# Run specific drivers
python run.py --only zun,stagehand

# Run specific tasks
python run.py --tasks cold_start,eval_simple,snapshot_full

# Custom output path
python run.py --output results/my_run.json

# Skip warmup
python run.py --warmup 0
```

## Generating Reports

```bash
# Markdown report to stdout
python report.py

# From a specific results file
python report.py results/my_run.json

# CSV output
python report.py --format csv

# Write to file
python report.py --output results/report.md
```

## Adding a New Driver

1. Create `bench/drivers/my_driver.py`
2. Subclass `BrowserDriver` from `drivers/base.py`
3. Implement all abstract methods (setup, teardown, navigate, snapshot,
   screenshot, eval_js, click, save_state, load_state)
4. Register the driver in `run.py` `DRIVER_REGISTRY`
5. Add the import to `drivers/__init__.py`

## Results Format

Output is JSON with this structure:

```json
{
  "timestamp": "2026-03-26T12:00:00+0000",
  "drivers": {
    "zun": {
      "version": "0.1.0",
      "setup_ms": 1234.5,
      "tasks": {
        "cold_start": {
          "name": "Cold Start + Navigate",
          "category": "startup",
          "iterations": [...],
          "stats": { "p50_ms": 100, "p95_ms": 150, "p99_ms": 200 },
          "success_rate": 1.0
        }
      }
    }
  }
}
```
