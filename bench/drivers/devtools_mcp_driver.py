"""Driver for Chrome DevTools MCP.

Uses the chrome-devtools-mcp npm package, communicating via the MCP
(Model Context Protocol) stdio transport.  Commands are sent as JSON-RPC
tool calls; the MCP server translates them into CDP operations.
"""

import asyncio
import json
import os
import time
from typing import Any, Optional

from .base import BrowserDriver, TimedResult


class DevToolsMcpDriver(BrowserDriver):
    """Chrome DevTools MCP driver using stdio JSON-RPC."""

    _name = "devtools-mcp"
    _version = "0.1.0"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    def __init__(self, npx_path: str = "npx"):
        self._npx = npx_path
        self._proc: Optional[asyncio.subprocess.Process] = None
        self._msg_id = 0

    async def _send_rpc(self, method: str, params: Optional[dict] = None) -> Any:
        """Send a JSON-RPC request and wait for the matching response."""
        if not self._proc or self._proc.returncode is not None:
            raise RuntimeError("MCP server is not running")

        self._msg_id += 1
        msg_id = self._msg_id
        request = {
            "jsonrpc": "2.0",
            "id": msg_id,
            "method": method,
        }
        if params is not None:
            request["params"] = params

        line = json.dumps(request) + "\n"
        self._proc.stdin.write(line.encode())
        await self._proc.stdin.drain()

        while True:
            raw = await asyncio.wait_for(
                self._proc.stdout.readline(), timeout=60
            )
            if not raw:
                raise RuntimeError("MCP server closed stdout")
            resp = json.loads(raw.decode())
            if resp.get("id") == msg_id:
                if "error" in resp:
                    err = resp["error"]
                    raise RuntimeError(
                        err.get("message", str(err))
                        if isinstance(err, dict)
                        else str(err)
                    )
                return resp.get("result")

    async def _call_tool(self, tool_name: str, arguments: dict) -> Any:
        """Invoke an MCP tool by name."""
        return await self._send_rpc(
            "tools/call",
            {"name": tool_name, "arguments": arguments},
        )

    async def setup(self) -> TimedResult:
        start = time.perf_counter()
        try:
            self._proc = await asyncio.create_subprocess_exec(
                self._npx,
                "-y",
                "chrome-devtools-mcp",
                stdin=asyncio.subprocess.PIPE,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE,
            )

            # Initialize with MCP protocol handshake
            init_result = await self._send_rpc("initialize", {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "agent-browser-bench", "version": "0.1.0"},
            })

            # Notify initialized
            notify = {
                "jsonrpc": "2.0",
                "method": "notifications/initialized",
            }
            self._proc.stdin.write((json.dumps(notify) + "\n").encode())
            await self._proc.stdin.drain()

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=init_result)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def teardown(self) -> None:
        if self._proc and self._proc.returncode is None:
            try:
                self._proc.stdin.close()
            except Exception:
                pass
            try:
                self._proc.terminate()
                await asyncio.wait_for(self._proc.wait(), timeout=5)
            except (asyncio.TimeoutError, Exception):
                self._proc.kill()
        self._proc = None
        self._msg_id = 0

    async def navigate(self, url: str) -> TimedResult:
        start = time.perf_counter()
        try:
            result = await self._call_tool("navigate_page", {"url": url})
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=result)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        start = time.perf_counter()
        try:
            params = {}
            if interactive_only:
                params["interactiveOnly"] = True
            result = await self._call_tool("take_snapshot", params)
            elapsed = (time.perf_counter() - start) * 1000
            # Extract text content from MCP result
            value = result
            if isinstance(result, dict):
                contents = result.get("content", [])
                if contents and isinstance(contents, list):
                    value = contents[0].get("text", str(result))
            return TimedResult(duration_ms=elapsed, value=value)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        start = time.perf_counter()
        try:
            params = {"savePath": path}
            if annotate:
                params["annotate"] = True
            result = await self._call_tool("take_screenshot", params)
            elapsed = (time.perf_counter() - start) * 1000

            # MCP may return base64 data; write to file if not already done
            if isinstance(result, dict):
                contents = result.get("content", [])
                for item in contents if isinstance(contents, list) else []:
                    if item.get("type") == "image" and not os.path.isfile(path):
                        import base64
                        os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
                        with open(path, "wb") as f:
                            f.write(base64.b64decode(item["data"]))

            if os.path.isfile(path):
                return TimedResult(duration_ms=elapsed, value=path)
            return TimedResult(
                duration_ms=elapsed,
                success=False,
                error="Screenshot file not created",
            )
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def eval_js(self, code: str) -> TimedResult:
        start = time.perf_counter()
        try:
            result = await self._call_tool(
                "evaluate_script", {"script": code}
            )
            elapsed = (time.perf_counter() - start) * 1000
            value = result
            if isinstance(result, dict):
                contents = result.get("content", [])
                if contents and isinstance(contents, list):
                    text = contents[0].get("text", "")
                    try:
                        value = json.loads(text)
                    except (json.JSONDecodeError, ValueError):
                        value = text
            return TimedResult(duration_ms=elapsed, value=value)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def click(self, selector: str) -> TimedResult:
        start = time.perf_counter()
        try:
            result = await self._call_tool("click", {"selector": selector})
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=selector)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def save_state(self, path: str) -> TimedResult:
        """Save state via MCP evaluate_script (no native MCP tool for this)."""
        start = time.perf_counter()
        try:
            # Get cookies and localStorage via JS eval
            cookies_result = await self._call_tool(
                "evaluate_script",
                {"script": "JSON.stringify(document.cookie)"},
            )
            storage_result = await self._call_tool(
                "evaluate_script",
                {
                    "script": (
                        "JSON.stringify(Object.entries(localStorage))"
                    )
                },
            )
            url_result = await self._call_tool(
                "evaluate_script", {"script": "location.href"}
            )

            def _extract_text(r: Any) -> str:
                if isinstance(r, dict):
                    contents = r.get("content", [])
                    if contents and isinstance(contents, list):
                        return contents[0].get("text", "")
                return str(r) if r else ""

            state = {
                "cookies": _extract_text(cookies_result),
                "localStorage": _extract_text(storage_result),
                "url": _extract_text(url_result),
            }

            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            with open(path, "w") as f:
                json.dump(state, f)

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=path)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def load_state(self, path: str) -> TimedResult:
        """Load state via MCP navigate + evaluate_script."""
        start = time.perf_counter()
        try:
            with open(path) as f:
                state = json.load(f)

            url = state.get("url", "about:blank")
            await self._call_tool("navigate_page", {"url": url})

            local_storage = state.get("localStorage", "[]")
            if isinstance(local_storage, str):
                try:
                    entries = json.loads(local_storage)
                except (json.JSONDecodeError, ValueError):
                    entries = []
            else:
                entries = local_storage

            if entries:
                set_js = "; ".join(
                    f"localStorage.setItem({json.dumps(k)}, {json.dumps(v)})"
                    for k, v in entries
                )
                await self._call_tool(
                    "evaluate_script", {"script": set_js}
                )

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=path)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))
