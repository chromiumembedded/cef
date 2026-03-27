"""Driver for browser-use.

Shells out to the ``browser-use`` CLI tool and parses output.
Requires browser-use to be installed and on PATH.
"""

import asyncio
import json
import os
import time
from typing import Optional

from .base import BrowserDriver, TimedResult


class BrowserUseDriver(BrowserDriver):
    """browser-use CLI driver."""

    _name = "browser-use"
    _version = "0.1.0"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    def __init__(self, cli_path: str = "browser-use"):
        self._cli = cli_path
        self._session_id: Optional[str] = None

    async def _run_cmd(self, *args: str) -> tuple[str, str, float, int]:
        """Execute a browser-use CLI command.

        Returns (stdout, stderr, elapsed_ms, returncode).
        """
        start = time.perf_counter()
        cmd_args = [self._cli, *args]
        if self._session_id:
            cmd_args.extend(["--session", self._session_id])
        proc = await asyncio.create_subprocess_exec(
            *cmd_args,
            stdout=asyncio.subprocess.PIPE,
            stderr=asyncio.subprocess.PIPE,
        )
        stdout, stderr = await proc.communicate()
        elapsed = (time.perf_counter() - start) * 1000
        return stdout.decode(), stderr.decode(), elapsed, proc.returncode

    def _parse_json(self, text: str) -> Optional[dict]:
        try:
            return json.loads(text)
        except (json.JSONDecodeError, ValueError):
            return None

    async def setup(self) -> TimedResult:
        start = time.perf_counter()
        try:
            stdout, stderr, ms, rc = await self._run_cmd("launch", "--json")
            data = self._parse_json(stdout)
            if rc == 0 and data and data.get("sessionId"):
                self._session_id = data["sessionId"]
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(duration_ms=elapsed, value=self._session_id)
            elif rc == 0:
                # Some versions don't return JSON; assume session started
                self._session_id = "default"
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(duration_ms=elapsed, value=self._session_id)
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(
                duration_ms=elapsed,
                success=False,
                error=stderr.strip() or f"Exit code {rc}",
            )
        except FileNotFoundError:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(
                duration_ms=elapsed,
                success=False,
                error=f"CLI not found: {self._cli}",
            )
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
            stdout, stderr, ms, rc = await self._run_cmd("open", url)
            if rc != 0:
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or f"Exit code {rc}",
                )
            return TimedResult(duration_ms=ms, value=url)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        try:
            args = ["state"]
            if interactive_only:
                args.append("--interactive")
            stdout, stderr, ms, rc = await self._run_cmd(*args)
            if rc != 0:
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or f"Exit code {rc}",
                )
            return TimedResult(duration_ms=ms, value=stdout.strip())
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        try:
            args = ["screenshot", "--output", path]
            if annotate:
                args.append("--annotate")
            stdout, stderr, ms, rc = await self._run_cmd(*args)
            if rc != 0 or not os.path.isfile(path):
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or "Screenshot not created",
                )
            return TimedResult(duration_ms=ms, value=path)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def eval_js(self, code: str) -> TimedResult:
        # browser-use does not directly support JS eval; attempt via CLI
        try:
            stdout, stderr, ms, rc = await self._run_cmd("eval", code)
            if rc != 0:
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error="JS eval not supported by browser-use CLI",
                )
            data = self._parse_json(stdout)
            return TimedResult(
                duration_ms=ms, value=data if data is not None else stdout.strip()
            )
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def click(self, selector: str) -> TimedResult:
        try:
            # browser-use uses index-based clicking; try CSS selector first
            stdout, stderr, ms, rc = await self._run_cmd("click", selector)
            if rc != 0:
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or f"Click failed on {selector}",
                )
            return TimedResult(duration_ms=ms, value=selector)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def save_state(self, path: str) -> TimedResult:
        try:
            stdout, stderr, ms, rc = await self._run_cmd(
                "state", "--save", path
            )
            if rc != 0 or not os.path.isfile(path):
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or "State save failed",
                )
            return TimedResult(duration_ms=ms, value=path)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))

    async def load_state(self, path: str) -> TimedResult:
        try:
            stdout, stderr, ms, rc = await self._run_cmd(
                "state", "--load", path
            )
            if rc != 0:
                return TimedResult(
                    duration_ms=ms,
                    success=False,
                    error=stderr.strip() or "State load failed",
                )
            return TimedResult(duration_ms=ms, value=path)
        except Exception as e:
            return TimedResult(duration_ms=0, success=False, error=str(e))
