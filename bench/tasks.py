"""Benchmark task definitions for agent-browser-bench.

Each task is a deterministic browser operation measured for latency.
No LLM in the loop — we benchmark infrastructure speed only.
"""

from dataclasses import dataclass, field
from typing import Optional


# Target pages (public, no login, deterministic content)
PAGES = {
    "minimal": "https://example.com",
    "medium": "https://news.ycombinator.com",
    "complex": "https://en.wikipedia.org/wiki/Chromium_(web_browser)",
}

# JS expressions for eval benchmarks
JS_SIMPLE = "document.title"
JS_COMPLEX = """
(() => {
  const els = document.querySelectorAll('a, button, input, select, textarea');
  return Array.from(els).slice(0, 100).map(e => ({
    tag: e.tagName,
    text: (e.textContent || '').trim().slice(0, 50),
    href: e.href || null,
    type: e.type || null,
    id: e.id || null,
    class: e.className || null,
    rect: e.getBoundingClientRect().toJSON()
  }));
})()
"""


@dataclass
class Task:
    """A single benchmark task."""
    id: str
    name: str
    description: str
    category: str  # startup, navigation, snapshot, screenshot, eval, compound, state
    iterations: int = 10

    # Task-specific params
    url: Optional[str] = None
    js_code: Optional[str] = None
    selector: Optional[str] = None
    interactive_only: bool = False
    annotate: bool = False
    save_path: Optional[str] = None


# All benchmark tasks
TASKS = [
    Task(
        id="cold_start",
        name="Cold Start + Navigate",
        description="Create browser session and navigate to blank page",
        category="startup",
        url="about:blank",
    ),
    Task(
        id="navigate_complex",
        name="Navigate (complex page)",
        description="Navigate to Wikipedia article and wait for load",
        category="navigation",
        url=PAGES["complex"],
    ),
    Task(
        id="snapshot_full",
        name="Full DOM Snapshot",
        description="Capture full accessibility tree snapshot",
        category="snapshot",
        url=PAGES["complex"],
    ),
    Task(
        id="snapshot_interactive",
        name="Interactive-Only Snapshot",
        description="Capture snapshot filtered to interactive elements only",
        category="snapshot",
        url=PAGES["complex"],
        interactive_only=True,
    ),
    Task(
        id="screenshot_plain",
        name="Screenshot (PNG)",
        description="Capture page screenshot as PNG",
        category="screenshot",
        url=PAGES["complex"],
    ),
    Task(
        id="screenshot_annotated",
        name="Annotated Screenshot",
        description="Capture screenshot with numbered element labels",
        category="screenshot",
        url=PAGES["complex"],
        annotate=True,
    ),
    Task(
        id="eval_simple",
        name="JS Eval (simple)",
        description="Evaluate document.title and return result",
        category="eval",
        url=PAGES["minimal"],
        js_code=JS_SIMPLE,
    ),
    Task(
        id="eval_complex",
        name="JS Eval (extract 100 elements)",
        description="Extract tag, text, href, rect from first 100 interactive elements",
        category="eval",
        url=PAGES["complex"],
        js_code=JS_COMPLEX,
    ),
    Task(
        id="click_wait_snapshot",
        name="Click + Wait + Snapshot",
        description="Click first link, wait for load, take snapshot",
        category="compound",
        url=PAGES["medium"],
        selector="a.titleline a",
    ),
    Task(
        id="navigate_10x",
        name="10x Sequential Navigations",
        description="Navigate to 10 different pages sequentially",
        category="throughput",
        iterations=3,  # 3 iterations of 10 navs each
    ),
    Task(
        id="state_save_load",
        name="State Save + Load",
        description="Save browser state to disk, then restore it",
        category="state",
        url=PAGES["medium"],
    ),
]

TASK_MAP = {t.id: t for t in TASKS}
