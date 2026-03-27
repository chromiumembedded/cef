"""Abstract base driver for browser automation frameworks.

Each framework implements this interface. All timing is in milliseconds.
"""

import time
from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from typing import Any, Optional


@dataclass
class TimedResult:
    """Result of a timed operation."""
    duration_ms: float
    success: bool = True
    value: Any = None
    error: Optional[str] = None


class BrowserDriver(ABC):
    """Abstract interface that each browser framework must implement."""

    @property
    @abstractmethod
    def name(self) -> str:
        """Short name for this driver (used in reports)."""
        ...

    @property
    @abstractmethod
    def version(self) -> str:
        """Version string of the framework."""
        ...

    @abstractmethod
    async def setup(self) -> TimedResult:
        """Create browser session. Returns time to first ready state."""
        ...

    @abstractmethod
    async def teardown(self) -> None:
        """Destroy browser session and cleanup."""
        ...

    @abstractmethod
    async def navigate(self, url: str) -> TimedResult:
        """Navigate to URL and wait for load. Returns navigation time."""
        ...

    @abstractmethod
    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        """Capture DOM/accessibility snapshot. Value is the snapshot text."""
        ...

    @abstractmethod
    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        """Capture screenshot. Value is the file path."""
        ...

    @abstractmethod
    async def eval_js(self, code: str) -> TimedResult:
        """Execute JavaScript and return result. Value is the eval result."""
        ...

    @abstractmethod
    async def click(self, selector: str) -> TimedResult:
        """Click an element by CSS selector. Returns click + settle time."""
        ...

    @abstractmethod
    async def save_state(self, path: str) -> TimedResult:
        """Save browser state to disk."""
        ...

    @abstractmethod
    async def load_state(self, path: str) -> TimedResult:
        """Load browser state from disk."""
        ...

    # Utility: time a sync callable
    @staticmethod
    def _time_sync(fn, *args, **kwargs) -> TimedResult:
        start = time.perf_counter()
        try:
            value = fn(*args, **kwargs)
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=value)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    # Utility: time an async callable
    @staticmethod
    async def _time_async(coro) -> TimedResult:
        start = time.perf_counter()
        try:
            value = await coro
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=value)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))
