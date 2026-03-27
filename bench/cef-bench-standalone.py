#!/usr/bin/env python3.11
"""CEF benchmark — standalone. Runs 6 agentic tasks via CDP."""
import asyncio, json, os, subprocess, time, sys

CEF_BIN = "/home/ubuntu/cef-build/chromium/src/out/Release_GN_x64/cefsimple"
CEF_LIB = "/home/ubuntu/cef-build/chromium/src/out/Release_GN_x64"

async def main():
    print("=" * 70)
    print("  CEF (zun fork, cefsimple) — Chromium 147.0.7727.0")
    print("=" * 70)
    sys.stdout.flush()

    # Start Xvfb
    xvfb = subprocess.Popen(["Xvfb", ":99", "-screen", "0", "1920x1080x24"],
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    await asyncio.sleep(1)

    # Start CEF
    env = {**os.environ, "DISPLAY": ":99", "LD_LIBRARY_PATH": CEF_LIB}
    cef = subprocess.Popen(
        [CEF_BIN, "--no-sandbox", "--remote-debugging-port=9333", "--url=about:blank"],
        stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    await asyncio.sleep(4)

    # Get page target via subprocess curl (urllib hangs on CEF)
    r = subprocess.run(["curl", "-s", "http://127.0.0.1:9333/json"],
                       capture_output=True, text=True, timeout=5)
    if not r.stdout.strip():
        print("  FAIL: CEF not responding")
        cef.kill(); xvfb.kill()
        return

    targets = json.loads(r.stdout)
    ws_url = targets[0]["webSocketDebuggerUrl"]
    print(f"  Connected: {ws_url}")
    sys.stdout.flush()

    import websockets

    msg_id = 0
    async with websockets.connect(ws_url, max_size=50*1024*1024) as ws:
        async def send(method, params=None):
            nonlocal msg_id
            msg_id += 1
            msg = {"id": msg_id, "method": method}
            if params:
                msg["params"] = params
            await ws.send(json.dumps(msg))
            while True:
                resp = json.loads(await asyncio.wait_for(ws.recv(), timeout=15))
                if resp.get("id") == msg_id:
                    if "error" in resp:
                        return None
                    return resp.get("result", {})

        async def navigate(url):
            await send("Page.navigate", {"url": url})
            await asyncio.sleep(2)

        async def evaluate(expr):
            r = await send("Runtime.evaluate", {"expression": expr, "returnByValue": True})
            if r and "result" in r:
                return r["result"].get("value")
            return None

        def step(name, ms, ok=True, extra=""):
            status = "OK" if ok else "FAIL"
            print(f"  {name:40s} {ms:8.0f}ms  {status}  {extra}")
            sys.stdout.flush()

        # ============ Task 1: Job Search ============
        print("\n  --- Task 1: Job Search (indeed.com) ---")
        s=time.perf_counter(); await navigate("https://www.indeed.com"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input#text-input-what')&&(document.querySelector('input#text-input-what').value='software engineer')"); step("fill_what", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input#text-input-where')&&(document.querySelector('input#text-input-where').value='San Francisco')"); step("fill_where", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('button[type=submit]')?.click()"); step("click_search", (time.perf_counter()-s)*1000)
        await asyncio.sleep(2)
        s=time.perf_counter(); r = await evaluate("document.querySelectorAll('[data-testid=slider_item],.resultContent').length"); step("eval_job_cards", (time.perf_counter()-s)*1000, extra=str(r))
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)

        # ============ Task 2: Product Comparison ============
        print("\n  --- Task 2: Product Comparison (amazon.com) ---")
        s=time.perf_counter(); await navigate("https://www.amazon.com"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input#twotabsearchtextbox')&&(document.querySelector('input#twotabsearchtextbox').value='usb-c cable')"); step("fill_search", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input#nav-search-submit-button')?.click()"); step("click_search", (time.perf_counter()-s)*1000)
        await asyncio.sleep(2)
        s=time.perf_counter(); r = await evaluate("JSON.stringify({title:document.querySelector('[data-component-type=s-search-result] h2')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent})"); step("eval_first", (time.perf_counter()-s)*1000, extra=str(r)[:80] if r else "")
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)

        # ============ Task 3: Travel Research ============
        print("\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
        s=time.perf_counter(); await navigate("https://www.google.com/travel/flights"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); r = await evaluate("document.querySelectorAll('input').length"); step("eval_inputs", (time.perf_counter()-s)*1000, extra=str(r))
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)

        # ============ Task 4: News Aggregation ============
        print("\n  --- Task 4: News Aggregation (news.ycombinator.com) ---")
        s=time.perf_counter(); await navigate("https://news.ycombinator.com"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); tree = await send("Accessibility.getFullAXTree"); nodes = len(tree.get("nodes",[])) if tree else 0; step("ax_tree", (time.perf_counter()-s)*1000, extra=f"{nodes} nodes")
        s=time.perf_counter(); r = await evaluate("JSON.stringify(Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent})))"); step("eval_stories", (time.perf_counter()-s)*1000, extra=f"{len(json.loads(r)) if r else 0} stories")
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('a.morelink')?.click()"); await asyncio.sleep(1); step("click_more", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); r = await evaluate("document.querySelectorAll('.athing').length"); step("eval_page2", (time.perf_counter()-s)*1000, extra=str(r))

        # ============ Task 5: Reference Lookup ============
        print("\n  --- Task 5: Reference Lookup (wikipedia.org) ---")
        s=time.perf_counter(); await navigate("https://en.wikipedia.org/wiki/Chromium_(web_browser)"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); tree = await send("Accessibility.getFullAXTree"); nodes = len(tree.get("nodes",[])) if tree else 0; step("ax_tree", (time.perf_counter()-s)*1000, extra=f"{nodes} nodes")
        s=time.perf_counter(); r = await evaluate("Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).length"); step("eval_toc", (time.perf_counter()-s)*1000, extra=f"{r} items")
        s=time.perf_counter(); r = await evaluate("JSON.stringify(Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim()))"); step("eval_infobox", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("window.scrollTo(0,document.querySelector('#History')?.offsetTop||0)"); step("scroll_history", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)

        # ============ Task 6: Form Submission ============
        print("\n  --- Task 6: Form Submission (httpbin.org/forms/post) ---")
        s=time.perf_counter(); await navigate("https://httpbin.org/forms/post"); step("navigate", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input[name=custname]').value='John Doe'"); step("fill_name", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input[name=custtel]').value='555-0123'"); step("fill_phone", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input[name=custemail]').value='john@example.com'"); step("fill_email", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('textarea[name=comments]').value='Benchmark test'"); step("fill_comments", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input[name=size][value=medium]').checked=true"); step("set_radio", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('input[name=topping][value=cheese]').checked=true"); step("set_topping", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); await evaluate("document.querySelector('button[type=submit]').click()"); await asyncio.sleep(1); step("submit", (time.perf_counter()-s)*1000)
        s=time.perf_counter(); r = await evaluate("document.body.textContent.length"); step("read_result", (time.perf_counter()-s)*1000, extra=f"{r}b")
        s=time.perf_counter(); await send("Page.captureScreenshot", {"format":"png"}); step("screenshot", (time.perf_counter()-s)*1000)

    cef.kill()
    xvfb.kill()
    print("\n  DONE")

asyncio.run(main())
