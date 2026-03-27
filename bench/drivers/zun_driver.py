"""Driver for our CEF (zun) browser using CDP WebSocket.

Communicates with cefclient via the Chrome DevTools Protocol over WebSocket.
The CEF binary must be built and available on PATH (or passed explicitly).
"""

import asyncio
import base64
import json
import os
import subprocess
import tempfile
import time
import urllib.request
from typing import Optional

import websockets

from .base import BrowserDriver, TimedResult


class ZunDriver(BrowserDriver):
    """CEF-based browser driver using CDP over WebSocket."""

    _name = "zun"
    _version = "0.1.0"

    @property
    def name(self) -> str:
        return self._name

    @property
    def version(self) -> str:
        return self._version

    def __init__(self, cef_binary_path: Optional[str] = None, port: int = 9222):
        self._binary = cef_binary_path or "cefclient"
        self._port = port
        self._process: Optional[subprocess.Popen] = None
        self._ws = None
        self._ws_url: Optional[str] = None
        self._msg_id = 0

    async def setup(self) -> TimedResult:
        start = time.perf_counter()
        try:
            self._process = subprocess.Popen(
                [
                    self._binary,
                    f"--remote-debugging-port={self._port}",
                    "--no-sandbox",
                    "--disable-gpu",
                    "--headless",
                ],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            )
            # Poll for DevTools endpoint
            for _ in range(50):
                try:
                    resp = urllib.request.urlopen(
                        f"http://127.0.0.1:{self._port}/json"
                    )
                    targets = json.loads(resp.read())
                    if targets:
                        self._ws_url = targets[0]["webSocketDebuggerUrl"]
                        self._ws = await websockets.connect(self._ws_url)
                        break
                except Exception:
                    await asyncio.sleep(0.1)

            elapsed = (time.perf_counter() - start) * 1000
            if not self._ws:
                return TimedResult(
                    duration_ms=elapsed,
                    success=False,
                    error="Failed to connect to CDP WebSocket",
                )
            # Enable required domains
            await self._send("Page.enable")
            await self._send("DOM.enable")
            await self._send("Accessibility.enable")
            return TimedResult(duration_ms=elapsed)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def teardown(self) -> None:
        if self._ws:
            try:
                await self._ws.close()
            except Exception:
                pass
            self._ws = None
        if self._process:
            self._process.terminate()
            try:
                self._process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._process.kill()
            self._process = None
        self._ws_url = None
        self._msg_id = 0

    async def _send(self, method: str, params: Optional[dict] = None) -> dict:
        """Send a CDP command and wait for its response."""
        self._msg_id += 1
        msg_id = self._msg_id
        msg = {"id": msg_id, "method": method}
        if params:
            msg["params"] = params
        await self._ws.send(json.dumps(msg))
        while True:
            raw = await self._ws.recv()
            resp = json.loads(raw)
            if resp.get("id") == msg_id:
                if "error" in resp:
                    raise RuntimeError(
                        f"CDP error: {resp['error'].get('message', resp['error'])}"
                    )
                return resp.get("result", {})

    async def _wait_for_event(self, event_name: str, timeout: float = 30.0) -> dict:
        """Wait for a specific CDP event."""
        deadline = time.perf_counter() + timeout
        while time.perf_counter() < deadline:
            raw = await asyncio.wait_for(
                self._ws.recv(), timeout=deadline - time.perf_counter()
            )
            msg = json.loads(raw)
            if msg.get("method") == event_name:
                return msg.get("params", {})
        raise TimeoutError(f"Timed out waiting for {event_name}")

    async def navigate(self, url: str) -> TimedResult:
        start = time.perf_counter()
        try:
            await self._send("Page.navigate", {"url": url})
            await self._wait_for_event("Page.loadEventFired")
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=url)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def snapshot(self, interactive_only: bool = False) -> TimedResult:
        start = time.perf_counter()
        try:
            result = await self._send("Accessibility.getFullAXTree")
            nodes = result.get("nodes", [])

            if interactive_only:
                interactive_roles = {
                    "button", "link", "textbox", "checkbox", "radio",
                    "combobox", "menuitem", "tab", "slider", "spinbutton",
                    "searchbox", "switch", "menuitemcheckbox", "menuitemradio",
                }
                nodes = [
                    n for n in nodes
                    if n.get("role", {}).get("value", "").lower() in interactive_roles
                ]

            # Format nodes into a readable text snapshot
            lines = []
            for node in nodes:
                role = node.get("role", {}).get("value", "unknown")
                name = node.get("name", {}).get("value", "")
                desc = node.get("description", {}).get("value", "")
                text = f"[{role}]"
                if name:
                    text += f" {name}"
                if desc:
                    text += f" ({desc})"
                lines.append(text)

            snapshot_text = "\n".join(lines)
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=snapshot_text)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def screenshot(self, path: str, annotate: bool = False) -> TimedResult:
        start = time.perf_counter()
        try:
            params = {"format": "png"}
            result = await self._send("Page.captureScreenshot", params)
            img_data = base64.b64decode(result["data"])

            if annotate:
                # Inject annotation overlay before capturing
                annotation_js = """
                (() => {
                    const els = document.querySelectorAll(
                        'a, button, input, select, textarea, [role="button"], [role="link"]'
                    );
                    let idx = 0;
                    els.forEach(el => {
                        const rect = el.getBoundingClientRect();
                        if (rect.width === 0 || rect.height === 0) return;
                        const label = document.createElement('div');
                        label.textContent = String(idx);
                        label.style.cssText = [
                            'position:fixed',
                            `top:${rect.top}px`,
                            `left:${rect.left}px`,
                            'background:red',
                            'color:white',
                            'font-size:10px',
                            'padding:1px 3px',
                            'z-index:999999',
                            'pointer-events:none',
                        ].join(';');
                        label.className = '__zun_annotation';
                        document.body.appendChild(label);
                        idx++;
                    });
                })()
                """
                await self._send(
                    "Runtime.evaluate", {"expression": annotation_js}
                )
                result = await self._send("Page.captureScreenshot", params)
                img_data = base64.b64decode(result["data"])
                # Clean up annotations
                await self._send(
                    "Runtime.evaluate",
                    {
                        "expression": (
                            "document.querySelectorAll('.__zun_annotation')"
                            ".forEach(e => e.remove())"
                        )
                    },
                )

            os.makedirs(os.path.dirname(path) or ".", exist_ok=True)
            with open(path, "wb") as f:
                f.write(img_data)

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=path)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def eval_js(self, code: str) -> TimedResult:
        start = time.perf_counter()
        try:
            result = await self._send(
                "Runtime.evaluate",
                {
                    "expression": code,
                    "returnByValue": True,
                    "awaitPromise": True,
                },
            )
            if "exceptionDetails" in result:
                exc = result["exceptionDetails"]
                text = exc.get("text", str(exc))
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(duration_ms=elapsed, success=False, error=text)

            value = result.get("result", {}).get("value")
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=value)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def click(self, selector: str) -> TimedResult:
        start = time.perf_counter()
        try:
            # Get document root
            doc = await self._send("DOM.getDocument")
            root_id = doc["root"]["nodeId"]

            # Find element
            result = await self._send(
                "DOM.querySelector",
                {"nodeId": root_id, "selector": selector},
            )
            node_id = result.get("nodeId")
            if not node_id:
                elapsed = (time.perf_counter() - start) * 1000
                return TimedResult(
                    duration_ms=elapsed,
                    success=False,
                    error=f"Element not found: {selector}",
                )

            # Get bounding box
            box_result = await self._send(
                "DOM.getBoxModel", {"nodeId": node_id}
            )
            quad = box_result["model"]["content"]
            # quad is [x1,y1, x2,y2, x3,y3, x4,y4] — compute center
            cx = (quad[0] + quad[2] + quad[4] + quad[6]) / 4
            cy = (quad[1] + quad[3] + quad[5] + quad[7]) / 4

            # Dispatch mouse events
            for etype in ("mousePressed", "mouseReleased"):
                await self._send(
                    "Input.dispatchMouseEvent",
                    {
                        "type": etype,
                        "x": cx,
                        "y": cy,
                        "button": "left",
                        "clickCount": 1,
                    },
                )

            # Wait briefly for any navigation or DOM settle
            await asyncio.sleep(0.1)

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=selector)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))

    async def save_state(self, path: str) -> TimedResult:
        start = time.perf_counter()
        try:
            # Collect cookies
            cookies = await self._send("Network.getAllCookies")
            # Collect localStorage via eval
            storage = await self._send(
                "Runtime.evaluate",
                {
                    "expression": "JSON.stringify(Object.entries(localStorage))",
                    "returnByValue": True,
                },
            )
            # Collect current URL
            nav = await self._send(
                "Runtime.evaluate",
                {"expression": "location.href", "returnByValue": True},
            )

            state = {
                "cookies": cookies.get("cookies", []),
                "localStorage": json.loads(
                    storage.get("result", {}).get("value", "[]")
                ),
                "url": nav.get("result", {}).get("value", ""),
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
        start = time.perf_counter()
        try:
            with open(path) as f:
                state = json.load(f)

            # Clear and set cookies
            await self._send("Network.clearBrowserCookies")
            for cookie in state.get("cookies", []):
                await self._send("Network.setCookie", cookie)

            # Navigate to saved URL
            url = state.get("url", "about:blank")
            await self._send("Page.navigate", {"url": url})
            await self._wait_for_event("Page.loadEventFired")

            # Restore localStorage
            entries = state.get("localStorage", [])
            if entries:
                set_js = "; ".join(
                    f"localStorage.setItem({json.dumps(k)}, {json.dumps(v)})"
                    for k, v in entries
                )
                await self._send(
                    "Runtime.evaluate", {"expression": set_js}
                )

            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, value=path)
        except Exception as e:
            elapsed = (time.perf_counter() - start) * 1000
            return TimedResult(duration_ms=elapsed, success=False, error=str(e))
