"""Driver for Vercel agent-browser CLI.

Shells out to the ``agent-browser`` CLI tool and parses JSON output.
Requires agent-browser to be installed and on PATH.
"""

import asyncio
import json
import os
import tempfile
import time
from typing import Optional

from .base import BrowserDriver, TimedResult


class AgentBrowserDriver(BrowserDriver):
    """Vercel agent-browser CLI driver."""

    _name = "agent-browser"
    _version = "0.1.0"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    def __init__(self, cli_path: str = "agent-browser"):
        self._cli = cli_path
        self._session_id: Optional[str] = None

    async def _run_cmd(self, *args: str) -> tuple[str, str, float]:
        """Execute an agent-browser CLI command and return (stdout, stderr, ms)."""
        start = time.perf_counter()
        cmd_args = [self._cli, *args, "--json"]
        if self._session_id:
            cmd_args.extend(["--session", self._session_id])
        proc = await asyncio.create_subprocess_exec(
            *cmd_args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        elapsed = (time.perf_counter() - start) * 1000
        return stdout.decode(), stderr.decode(), elapsed

    def _parse_output(self, stdout: str) -> Optional[dict]:
        """Parse JSON from CLI stdout, return None on failure."""
        try:
            return json.loads(stdout)
        except (json.JSONDecodeError, ValueError):
            return None

    async def setup(self) -> TimedResult:
        start = time.perf_counter()
        try:
            stdout, stderr, ms = await self._run_cmd("launch")
            data = self._parse_output(stdout)
            if data and data.get("sessionId"):
                self._session_id = data["sessionId"]
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(duration_ms=elapsed, value=self._session_id)
            elapsed = (time.perf_counter() - start) * 1000
            error = stderr.strip() or "No session ID returned"
            return TimedResult(duration_ms=elapsed, success=False, error=error)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def teardown(self) -> None:
        if self._session_id:
            try:
                await self._run_cmd("close")
            except Exception:
                pass
            self._session_id = None

    async def navigate(self, url: str) -> TimedResult:
        try:
            stdout, stderr, ms = await self._run_cmd("open", url)
            data = self._parse_output(stdout)
            if data and data.get("error"):
                return TimedResult(
                    duration_ms=ms, success=False, error=data["error"]
                )
            return TimedResult(duration_ms=ms, value=data)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        try:
            args = ["snapshot"]
            if interactive_only:
                args.append("-i")
            stdout, stderr, ms = await self._run_cmd(*args)
            data = self._parse_output(stdout)
            if data is None:
                # Raw text snapshot
                return TimedResult(duration_ms=ms, value=stdout.strip())
            return TimedResult(duration_ms=ms, value=data)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        try:
            args = ["screenshot", "--output", path]
            if annotate:
                args.append("--annotate")
            stdout, stderr, ms = await self._run_cmd(*args)
            if os.path.isfile(path):
                return TimedResult(duration_ms=ms, value=path)
            return TimedResult(
                duration_ms=ms,
                success=False,
                error=stderr.strip() or "Screenshot file not created",
            )
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def eval_js(self, code: str) -> TimedResult:
        try:
            stdout, stderr, ms = await self._run_cmd("eval", code)
            data = self._parse_output(stdout)
            if data is not None:
                return TimedResult(duration_ms=ms, value=data)
            return TimedResult(duration_ms=ms, value=stdout.strip())
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def click(self, selector: str) -> TimedResult:
        try:
            stdout, stderr, ms = await self._run_cmd("click", selector)
            data = self._parse_output(stdout)
            if data and data.get("error"):
                return TimedResult(
                    duration_ms=ms, success=False, error=data["error"]
                )
            return TimedResult(duration_ms=ms, value=selector)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def save_state(self, path: str) -> TimedResult:
        try:
            stdout, stderr, ms = await self._run_cmd("state", "save", path)
            if os.path.isfile(path):
                return TimedResult(duration_ms=ms, value=path)
            return TimedResult(
                duration_ms=ms,
                success=False,
                error=stderr.strip() or "State file not created",
            )
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def load_state(self, path: str) -> TimedResult:
        try:
            stdout, stderr, ms = await self._run_cmd("state", "load", path)
            data = self._parse_output(stdout)
            if data and data.get("error"):
                return TimedResult(
                    duration_ms=ms, success=False, error=data["error"]
                )
            return TimedResult(duration_ms=ms, value=path)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))
