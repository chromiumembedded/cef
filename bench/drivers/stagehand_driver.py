"""Driver for Stagehand (Browserbase).

Manages a persistent Node.js helper subprocess that accepts JSON commands over
stdin and returns JSON results on stdout, backed by the Stagehand SDK.
"""

import asyncio
import json
import os
import tempfile
import time
from typing import Optional

from .base import BrowserDriver, TimedResult

# JavaScript helper script embedded as a string. It is written to a temp file
# at setup time and executed with Node.js.
_HELPER_JS = r"""
const { Stagehand } = require("@browserbasehq/stagehand");
const readline = require("readline");

(async () => {
  const stagehand = new Stagehand({ env: "LOCAL" });
  await stagehand.init();
  const page = stagehand.page;

  // Signal ready
  process.stdout.write(JSON.stringify({ ready: true }) + "\n");

  const rl = readline.createInterface({ input: process.stdin });

  for await (const line of rl) {
    let cmd;
    try {
      cmd = JSON.parse(line);
    } catch {
      process.stdout.write(JSON.stringify({ error: "invalid JSON" }) + "\n");
      continue;
    }

    const start = Date.now();
    let result = {};

    try {
      switch (cmd.type) {
        case "navigate": {
          await page.goto(cmd.url, { waitUntil: "load" });
          result.value = cmd.url;
          break;
        }
        case "snapshot": {
          const tree = await page.accessibility.snapshot({
            interestingOnly: !!cmd.interactiveOnly,
          });
          result.value = JSON.stringify(tree);
          break;
        }
        case "screenshot": {
          await page.screenshot({ path: cmd.path, fullPage: false });
          result.value = cmd.path;
          break;
        }
        case "eval": {
          const val = await page.evaluate(cmd.code);
          result.value = val;
          break;
        }
        case "click": {
          await page.click(cmd.selector);
          await page.waitForLoadState("load").catch(() => {});
          result.value = cmd.selector;
          break;
        }
        case "save_state": {
          const cookies = await page.context().cookies();
          const storage = await page.evaluate(() =>
            JSON.stringify(Object.entries(localStorage))
          );
          const url = page.url();
          const fs = require("fs");
          fs.writeFileSync(
            cmd.path,
            JSON.stringify({ cookies, localStorage: JSON.parse(storage), url })
          );
          result.value = cmd.path;
          break;
        }
        case "load_state": {
          const fs = require("fs");
          const state = JSON.parse(fs.readFileSync(cmd.path, "utf-8"));
          const ctx = page.context();
          await ctx.clearCookies();
          if (state.cookies && state.cookies.length) {
            await ctx.addCookies(state.cookies);
          }
          await page.goto(state.url || "about:blank", { waitUntil: "load" });
          if (state.localStorage && state.localStorage.length) {
            const setCode = state.localStorage
              .map(([k, v]) => `localStorage.setItem(${JSON.stringify(k)}, ${JSON.stringify(v)})`)
              .join(";");
            await page.evaluate(setCode);
          }
          result.value = cmd.path;
          break;
        }
        case "close": {
          await stagehand.close();
          process.stdout.write(JSON.stringify({ closed: true }) + "\n");
          process.exit(0);
        }
        default:
          result.error = `Unknown command: ${cmd.type}`;
      }
    } catch (err) {
      result.error = err.message || String(err);
    }

    result.duration_ms = Date.now() - start;
    process.stdout.write(JSON.stringify(result) + "\n");
  }
})();
"""


class StagehandDriver(BrowserDriver):
    """Stagehand (Browserbase) driver using a persistent Node.js subprocess."""

    _name = "stagehand"
    _version = "0.1.0"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    def __init__(self, node_path: str = "node"):
        self._node = node_path
        self._proc: Optional[asyncio.subprocess.Process] = None
        self._helper_path: Optional[str] = None

    async def _send_cmd(self, cmd: dict) -> dict:
        """Send a JSON command to the helper and read the JSON response."""
        if not self._proc or self._proc.returncode is not None:
            raise RuntimeError("Stagehand helper is not running")
        line = json.dumps(cmd) + "\n"
        self._proc.stdin.write(line.encode())
        await self._proc.stdin.drain()
        raw = await asyncio.wait_for(self._proc.stdout.readline(), timeout=60)
        return json.loads(raw.decode())

    async def setup(self) -> TimedResult:
        start = time.perf_counter()
        try:
            # Write helper script to temp file
            fd, self._helper_path = tempfile.mkstemp(suffix=".js")
            with os.fdopen(fd, "w") as f:
                f.write(_HELPER_JS)

            self._proc = await asyncio.create_subprocess_exec(
                self._node,
                self._helper_path,
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )

            # Wait for ready signal
            raw = await asyncio.wait_for(
                self._proc.stdout.readline(), timeout=30
            )
            resp = json.loads(raw.decode())
            if not resp.get("ready"):
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(
                    duration_ms=elapsed,
                    success=False,
                    error="Helper did not signal ready",
                )

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def teardown(self) -> None:
        if self._proc and self._proc.returncode is None:
            try:
                await asyncio.wait_for(
                    self._send_cmd({"type": "close"}), timeout=10
                )
            except Exception:
                self._proc.terminate()
            try:
                await asyncio.wait_for(self._proc.wait(), timeout=5)
            except asyncio.TimeoutError:
                self._proc.kill()
        self._proc = None
        if self._helper_path and os.path.isfile(self._helper_path):
            try:
                os.unlink(self._helper_path)
            except OSError:
                pass
            self._helper_path = None

    async def _timed_cmd(self, cmd: dict) -> TimedResult:
        """Send a command and wrap the response in a TimedResult."""
        start = time.perf_counter()
        try:
            resp = await self._send_cmd(cmd)
            elapsed = (time.perf_counter() - start) * 1000
            if resp.get("error"):
                return TimedResult(
                    duration_ms=elapsed, success=False, error=resp["error"]
                )
            return TimedResult(duration_ms=elapsed, value=resp.get("value"))
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def navigate(self, url: str) -> TimedResult:
        return await self._timed_cmd({"type": "navigate", "url": url})

    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        return await self._timed_cmd(
            {"type": "snapshot", "interactiveOnly": interactive_only}
        )

    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        result = await self._timed_cmd({"type": "screenshot", "path": path})
        if annotate and result.success:
            # Stagehand doesn't have built-in annotation; note in result
            result.value = path
        return result

    async def eval_js(self, code: str) -> TimedResult:
        return await self._timed_cmd({"type": "eval", "code": code})

    async def click(self, selector: str) -> TimedResult:
        return await self._timed_cmd({"type": "click", "selector": selector})

    async def save_state(self, path: str) -> TimedResult:
        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
        return await self._timed_cmd({"type": "save_state", "path": path})

    async def load_state(self, path: str) -> TimedResult:
        return await self._timed_cmd({"type": "load_state", "path": path})
