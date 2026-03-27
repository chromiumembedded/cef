#!/usr/bin/env python3
"""
agent-browser-bench v0.2.0 -- Real Agentic Tasks

6 real-world tasks x 4 browser frameworks, run sequentially.
Each framework gets the full machine to itself (52 cores, 442GB RAM).
If any step fails, log the failure and move on -- never crash.

Frameworks:
  1. agent-browser  (Vercel CLI, v0.22.3)
  2. Playwright     (browser-use / stagehand engine, v1.58.0)
  3. CDP Raw        (Chrome DevTools Protocol via Playwright CDP session)
  4. CEF            (our fork, zun -- cefclient + websocket CDP)

Tasks:
  1. Job Search            (indeed.com)
  2. Product Comparison    (amazon.com)
  3. Travel Research       (google.com/travel/flights)
  4. News Aggregation      (news.ycombinator.com)
  5. Reference Lookup      (wikipedia.org)
  6. Form Submission       (httpbin.org/forms/post)
"""

import json
import os
import signal
import subprocess
import sys
import tempfile
import textwrap
import time
import traceback


# ---------------------------------------------------------------------------
# Constants
# ---------------------------------------------------------------------------

STEP_TIMEOUT = 30          # seconds per individual step
FRAMEWORK_TIMEOUT = 300    # seconds per framework script (playwright/cdp/cef)
INTER_FRAMEWORK_SLEEP = 3  # seconds between frameworks

CEF_BINARY = "/home/ubuntu/cef-build/chromium/src/out/Release_GN_x64/cefclient"
CEF_DEBUG_PORT = 9333
CEF_DISPLAY = ":99"

SCREENSHOT_DIR = "/tmp/bench-screenshots"


# ---------------------------------------------------------------------------
# Step logging
# ---------------------------------------------------------------------------

def step(name, cmd, timeout=STEP_TIMEOUT):
    """Run a shell command, time it, print aligned result. Never throws."""
    start = time.perf_counter()
    try:
        r = subprocess.run(
            cmd, shell=True, capture_output=True, text=True, timeout=timeout,
        )
        ms = (time.perf_counter() - start) * 1000
        ok = r.returncode == 0
        detail = ""
        if not ok:
            # Check for common bot-detection patterns in output
            combined = (r.stdout + r.stderr).lower()
            if any(w in combined for w in ["captcha", "blocked", "robot", "verify"]):
                detail = "BLOCKED BY SITE"
            else:
                detail = (r.stderr or r.stdout).strip().split("\n")[-1][:80]
        status = "OK" if ok else "FAIL"
        print(f"  {name:38s} {ms:8.0f}ms  {status:4s}  {detail}")
        sys.stdout.flush()
        return (name, ms, ok, detail)
    except subprocess.TimeoutExpired:
        ms = (time.perf_counter() - start) * 1000
        print(f"  {name:38s} {ms:8.0f}ms  FAIL  TIMEOUT ({timeout}s)")
        sys.stdout.flush()
        return (name, ms, False, "TIMEOUT")
    except Exception as e:
        ms = (time.perf_counter() - start) * 1000
        msg = str(e)[:80]
        print(f"  {name:38s} {ms:8.0f}ms  FAIL  {msg}")
        sys.stdout.flush()
        return (name, ms, False, msg)


def run_script(path, timeout=FRAMEWORK_TIMEOUT):
    """Run a Python script in a subprocess, streaming its stdout in real time."""
    try:
        proc = subprocess.Popen(
            ["python3", path],
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            text=True, bufsize=1,
        )
        # Stream stdout line by line
        for line in iter(proc.stdout.readline, ""):
            print(line, end="", flush=True)
        proc.stdout.close()
        proc.wait(timeout=timeout)
        if proc.returncode != 0:
            stderr = proc.stderr.read()
            if stderr.strip():
                # Print last 5 lines of stderr for debugging
                lines = stderr.strip().split("\n")
                for line in lines[-5:]:
                    print(f"  [stderr] {line}")
                sys.stdout.flush()
    except subprocess.TimeoutExpired:
        proc.kill()
        print(f"  FRAMEWORK TIMEOUT after {timeout}s")
    except Exception as e:
        print(f"  FRAMEWORK ERROR: {e}")


def cleanup_all_browsers():
    """Kill all browser processes and wait for them to die."""
    targets = [
        "chrome --", "chromium", "cefclient",
        "agent-browser", "Xvfb",
    ]
    for t in targets:
        os.system(f"pkill -9 -f '{t}' 2>/dev/null")
    time.sleep(INTER_FRAMEWORK_SLEEP)


# ---------------------------------------------------------------------------
# Shared Python code fragments for Playwright/CDP/CEF scripts
# ---------------------------------------------------------------------------

STEP_HELPER = textwrap.dedent('''\
    import asyncio, base64, json, os, sys, time, traceback

    class StepPrinter:
        """Print aligned step results. Catches all exceptions."""

        def __init__(self):
            self.results = []

        async def run(self, name, coro):
            """Await a coroutine, time it, print result. Never throws."""
            start = time.perf_counter()
            try:
                result = await coro
                ms = (time.perf_counter() - start) * 1000
                detail = ""
                if isinstance(result, str):
                    detail = f"{len(result)}b"
                elif isinstance(result, (list, dict)):
                    detail = f"{len(result)} items"
                print(f"  {name:38s} {ms:8.0f}ms  OK    {detail}")
                sys.stdout.flush()
                self.results.append((name, ms, True))
                return result
            except Exception as e:
                ms = (time.perf_counter() - start) * 1000
                msg = str(e)[:80]
                if any(w in msg.lower() for w in ["captcha", "blocked", "robot"]):
                    msg = "BLOCKED BY SITE"
                print(f"  {name:38s} {ms:8.0f}ms  FAIL  {msg}")
                sys.stdout.flush()
                self.results.append((name, ms, False))
                return None

    S = StepPrinter()
''')


# ---------------------------------------------------------------------------
# Framework 1: agent-browser (Vercel CLI)
# ---------------------------------------------------------------------------

def run_agent_browser():
    """Benchmark agent-browser CLI via subprocess calls."""

    def ab(args):
        return f"agent-browser {args}"

    # -- Task 1: Job Search --
    print("\n  --- Task 1: Job Search (indeed.com) ---")
    step("navigate",         ab("open https://www.indeed.com"))
    step("fill_what",        ab("fill 'input#text-input-what' 'software engineer'"))
    step("fill_where",       ab("fill 'input#text-input-where' 'San Francisco'"))
    step("click_search",     ab("click 'button[type=submit]'"))
    step("snapshot_results", ab("snapshot"))
    step("eval_job_cards",   ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("[data-testid=slider_item], .job_seen_beacon, .resultContent")).slice(0,10).map(el=>({title:el.querySelector("h2,a")?.textContent?.trim(),company:el.querySelector("[data-testid=company-name],.companyName")?.textContent?.trim()})))'"""))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-indeed.png"))
    os.system("agent-browser close 2>/dev/null")

    # -- Task 2: Product Comparison --
    print("\n  --- Task 2: Product Comparison (amazon.com) ---")
    step("navigate",         ab("open https://www.amazon.com"))
    step("fill_search",      ab("fill 'input#twotabsearchtextbox' 'usb-c cable'"))
    step("click_search",     ab("click 'input#nav-search-submit-button'"))
    step("snapshot_results", ab("snapshot"))
    step("eval_first",       ab("""eval 'JSON.stringify((() => { const c = document.querySelector("[data-component-type=s-search-result]"); return { title: c?.querySelector("h2")?.textContent?.trim(), price: c?.querySelector(".a-price .a-offscreen")?.textContent, rating: c?.querySelector(".a-icon-alt")?.textContent, reviews: c?.querySelector("[data-csa-c-func-deps=aui-da-a-popover]")?.textContent?.trim() }; })())'"""))
    step("click_first",      ab("click '[data-component-type=s-search-result] h2 a'"))
    step("eval_product",     ab("""eval 'JSON.stringify({title:document.querySelector("#productTitle")?.textContent?.trim(),price:document.querySelector(".a-price .a-offscreen")?.textContent,rating:document.querySelector("#acrPopover .a-icon-alt")?.textContent,reviews:document.querySelector("#acrCustomerReviewText")?.textContent?.trim()})'"""))
    step("go_back",          ab("eval 'history.back()'"))
    step("click_second",     ab("click '[data-component-type=s-search-result]:nth-child(2) h2 a'"))
    step("eval_product_2",   ab("""eval 'JSON.stringify({title:document.querySelector("#productTitle")?.textContent?.trim(),price:document.querySelector(".a-price .a-offscreen")?.textContent})'"""))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-amazon.png"))
    os.system("agent-browser close 2>/dev/null")

    # -- Task 3: Travel Research --
    print("\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
    step("navigate",         ab("open https://www.google.com/travel/flights"))
    step("eval_find_inputs", ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("input")).map(i=>({name:i.name,placeholder:i.placeholder,ariaLabel:i.getAttribute("aria-label")})))'"""))
    step("fill_origin",      ab("fill 'input[aria-label*=Where]' 'SFO'"))
    step("fill_dest",        ab("fill 'input[aria-label*=Where]:last-of-type' 'JFK'"))
    step("snapshot",         ab("snapshot"))
    step("eval_flights",     ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("[class*=result],[role=listitem]")).slice(0,5).map(el=>el.textContent?.trim()?.slice(0,200)))'"""))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-flights.png"))
    os.system("agent-browser close 2>/dev/null")

    # -- Task 4: News Aggregation --
    print("\n  --- Task 4: News Aggregation (news.ycombinator.com) ---")
    step("navigate",         ab("open https://news.ycombinator.com"))
    step("snapshot_front",   ab("snapshot"))
    step("eval_stories",     ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll(".athing")).map(el=>({title:el.querySelector(".titleline a")?.textContent,url:el.querySelector(".titleline a")?.href,score:el.nextElementSibling?.querySelector(".score")?.textContent})))'"""))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-hn.png"))
    step("click_more",       ab("click 'a.morelink'"))
    step("eval_page2",       ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll(".athing")).map(el=>({title:el.querySelector(".titleline a")?.textContent,url:el.querySelector(".titleline a")?.href,score:el.nextElementSibling?.querySelector(".score")?.textContent})))'"""))
    step("click_comments",   ab("""eval 'document.querySelector(".subtext a[href*=item]")?.click()'"""))
    step("eval_comments",    ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll(".commtext")).slice(0,5).map(c=>c.textContent?.trim()?.slice(0,200)))'"""))
    step("snapshot_comments",ab("snapshot"))
    os.system("agent-browser close 2>/dev/null")

    # -- Task 5: Reference Lookup --
    print("\n  --- Task 5: Reference Lookup (wikipedia.org) ---")
    step("navigate",         ab("open 'https://en.wikipedia.org/wiki/Special:Search?search=Chromium+web+browser'"))
    step("click_first",      ab("click '.mw-search-result-heading a'"))
    step("eval_toc",         ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("#toc a,.toc a,[class*=toc] a")).map(a=>a.textContent.trim()).filter(Boolean))'"""))
    step("eval_infobox",     ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("table.infobox tr")).slice(0,15).map(tr=>tr.textContent.trim()))'"""))
    step("eval_scroll",      ab("""eval 'window.scrollTo(0,document.querySelector("#History")?.offsetTop||0)'"""))
    step("eval_history_tbl", ab("""eval 'JSON.stringify(Array.from(document.querySelectorAll("#History ~ table, #History ~ .wikitable")).slice(0,2).map(t=>t.textContent?.trim()?.slice(0,300)))'"""))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-wiki.png"))
    step("snapshot_article", ab("snapshot"))
    os.system("agent-browser close 2>/dev/null")

    # -- Task 6: Form Submission --
    print("\n  --- Task 6: Form Submission (httpbin.org/forms/post) ---")
    step("navigate",         ab("open https://httpbin.org/forms/post"))
    step("snapshot_form",    ab("snapshot"))
    step("fill_name",        ab("fill 'input[name=custname]' 'John Doe'"))
    step("fill_phone",       ab("fill 'input[name=custtel]' '555-0123'"))
    step("fill_email",       ab("fill 'input[name=custemail]' 'john@example.com'"))
    step("fill_comments",    ab("""fill 'textarea[name=comments]' 'This is a benchmark test for agent-browser-bench v0.2.0'"""))
    step("click_radio",      ab("click 'input[name=size][value=medium]'"))
    step("click_topping_1",  ab("click 'input[name=topping][value=cheese]'"))
    step("click_topping_2",  ab("click 'input[name=topping][value=mushroom]'"))
    step("submit",           ab("click 'button[type=submit]'"))
    step("eval_response",    ab("""eval 'document.body.textContent.trim().slice(0,500)'"""))
    step("snapshot_response",ab("snapshot"))
    step("screenshot",       ab(f"screenshot {SCREENSHOT_DIR}/ab-form.png"))
    os.system("agent-browser close 2>/dev/null")


# ---------------------------------------------------------------------------
# Framework 2: Playwright (browser-use / stagehand engine)
# ---------------------------------------------------------------------------

def run_playwright():
    """Benchmark Playwright async API by writing+running a temp script."""
    script = STEP_HELPER + textwrap.dedent('''\

    async def main():
        from playwright.async_api import async_playwright

        async with async_playwright() as pw:
            browser = await pw.chromium.launch(headless=True)
            ctx = await browser.new_context(
                user_agent="Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                           "(KHTML, like Gecko) Chrome/131.0.0.0 Safari/537.36"
            )
            page = await ctx.new_page()

            # -- Task 1: Job Search --
            print("\\n  --- Task 1: Job Search (indeed.com) ---")
            await S.run("navigate",         page.goto("https://www.indeed.com", wait_until="domcontentloaded", timeout=15000))
            await S.run("fill_what",        page.fill("input#text-input-what", "software engineer"))
            await S.run("fill_where",       page.fill("input#text-input-where", "San Francisco"))
            await S.run("click_search",     page.click("button[type=submit]"))
            await S.run("wait_results",     page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_job_cards",   page.evaluate("""Array.from(document.querySelectorAll('[data-testid=slider_item],.job_seen_beacon,.resultContent')).slice(0,10).map(el=>({title:el.querySelector('h2,a')?.textContent?.trim(),company:el.querySelector('[data-testid=company-name],.companyName')?.textContent?.trim()}))"""))
            await S.run("snapshot",         page.content())
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-indeed.png"))

            # -- Task 2: Product Comparison --
            print("\\n  --- Task 2: Product Comparison (amazon.com) ---")
            await S.run("navigate",         page.goto("https://www.amazon.com", wait_until="domcontentloaded", timeout=15000))
            await S.run("fill_search",      page.fill("input#twotabsearchtextbox", "usb-c cable"))
            await S.run("click_search",     page.click("input#nav-search-submit-button"))
            await S.run("wait_results",     page.wait_for_load_state("domcontentloaded"))
            await S.run("snapshot",         page.content())
            await S.run("eval_first",       page.evaluate("""(() => { const c = document.querySelector('[data-component-type=s-search-result]'); return { title: c?.querySelector('h2')?.textContent?.trim(), price: c?.querySelector('.a-price .a-offscreen')?.textContent, rating: c?.querySelector('.a-icon-alt')?.textContent, reviews: c?.querySelector('[data-csa-c-func-deps=aui-da-a-popover]')?.textContent?.trim() }; })()"""))
            await S.run("click_first",      page.click("[data-component-type=s-search-result] h2 a"))
            await S.run("wait_product",     page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_product",     page.evaluate("""({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent,rating:document.querySelector('#acrPopover .a-icon-alt')?.textContent,reviews:document.querySelector('#acrCustomerReviewText')?.textContent?.trim()})"""))
            await S.run("go_back",          page.go_back(wait_until="domcontentloaded"))
            await S.run("click_second",     page.click("[data-component-type=s-search-result]:nth-child(2) h2 a"))
            await S.run("eval_product_2",   page.evaluate("""({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent})"""))
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-amazon.png"))

            # -- Task 3: Travel Research --
            print("\\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
            await S.run("navigate",         page.goto("https://www.google.com/travel/flights", wait_until="domcontentloaded", timeout=15000))
            await S.run("eval_find_inputs", page.evaluate("""Array.from(document.querySelectorAll('input')).map(i=>({name:i.name,placeholder:i.placeholder,ariaLabel:i.getAttribute('aria-label')}))"""))
            await S.run("fill_origin",      page.fill("input[aria-label*=Where]", "SFO"))
            await S.run("fill_dest",        page.fill("input[aria-label*=Where]:last-of-type", "JFK"))
            await S.run("snapshot",         page.content())
            await S.run("eval_flights",     page.evaluate("""Array.from(document.querySelectorAll('[class*=result],[role=listitem]')).slice(0,5).map(el=>el.textContent?.trim()?.slice(0,200))"""))
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-flights.png"))

            # -- Task 4: News Aggregation --
            print("\\n  --- Task 4: News Aggregation (news.ycombinator.com) ---")
            await S.run("navigate",         page.goto("https://news.ycombinator.com", wait_until="domcontentloaded"))
            await S.run("snapshot",         page.content())
            await S.run("eval_stories",     page.evaluate("""Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent}))"""))
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-hn.png"))
            await S.run("click_more",       page.click("a.morelink"))
            await S.run("wait_page2",       page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_page2",       page.evaluate("""Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent}))"""))
            await S.run("click_comments",   page.evaluate("document.querySelector('.subtext a[href*=item]')?.click()"))
            await S.run("wait_comments",    page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_comments",    page.evaluate("""Array.from(document.querySelectorAll('.commtext')).slice(0,5).map(c=>c.textContent?.trim()?.slice(0,200))"""))
            await S.run("snapshot_comments",page.content())

            # -- Task 5: Reference Lookup --
            print("\\n  --- Task 5: Reference Lookup (wikipedia.org) ---")
            await S.run("navigate",         page.goto("https://en.wikipedia.org/wiki/Special:Search?search=Chromium+web+browser", wait_until="domcontentloaded"))
            await S.run("click_first",      page.click(".mw-search-result-heading a"))
            await S.run("wait_article",     page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_toc",         page.evaluate("""Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).map(a=>a.textContent.trim()).filter(Boolean)"""))
            await S.run("eval_infobox",     page.evaluate("""Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim())"""))
            await S.run("eval_scroll",      page.evaluate("window.scrollTo(0,document.querySelector('#History')?.offsetTop||0)"))
            await S.run("eval_history_tbl", page.evaluate("""Array.from(document.querySelectorAll('#History ~ table, #History ~ .wikitable')).slice(0,2).map(t=>t.textContent?.trim()?.slice(0,300))"""))
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-wiki.png"))
            await S.run("snapshot_article", page.content())

            # -- Task 6: Form Submission --
            print("\\n  --- Task 6: Form Submission (httpbin.org/forms/post) ---")
            await S.run("navigate",         page.goto("https://httpbin.org/forms/post", wait_until="domcontentloaded"))
            await S.run("snapshot_form",    page.content())
            await S.run("fill_name",        page.fill("input[name=custname]", "John Doe"))
            await S.run("fill_phone",       page.fill("input[name=custtel]", "555-0123"))
            await S.run("fill_email",       page.fill("input[name=custemail]", "john@example.com"))
            await S.run("fill_comments",    page.fill("textarea[name=comments]", "This is a benchmark test for agent-browser-bench v0.2.0"))
            await S.run("click_radio",      page.click("input[name=size][value=medium]"))
            await S.run("click_topping_1",  page.click("input[name=topping][value=cheese]"))
            await S.run("click_topping_2",  page.click("input[name=topping][value=mushroom]"))
            await S.run("submit",           page.click("button[type=submit]"))
            await S.run("wait_response",    page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_response",    page.text_content("body"))
            await S.run("verify_values",    page.evaluate("""(() => { const t = document.body.textContent; return { has_name: t.includes('John Doe'), has_phone: t.includes('555-0123'), has_email: t.includes('john@example.com') }; })()"""))
            await S.run("snapshot_response",page.content())
            await S.run("screenshot",       page.screenshot(path="SCREENSHOT_DIR/pw-form.png"))

            await browser.close()

    asyncio.run(main())
    ''').replace("SCREENSHOT_DIR", SCREENSHOT_DIR)

    path = os.path.join(tempfile.gettempdir(), "bench_playwright.py")
    with open(path, "w") as f:
        f.write(script)
    run_script(path)


# ---------------------------------------------------------------------------
# Framework 3: CDP Raw (Chrome DevTools Protocol)
# ---------------------------------------------------------------------------

def run_cdp():
    """Benchmark raw CDP commands via Playwright's CDP session."""
    script = STEP_HELPER + textwrap.dedent('''\

    async def cdp_eval(cdp, expr):
        """Evaluate JS via CDP and return the value."""
        r = await cdp.send("Runtime.evaluate", {
            "expression": expr,
            "returnByValue": True,
            "awaitPromise": True,
        })
        return r.get("result", {}).get("value")

    async def main():
        from playwright.async_api import async_playwright

        async with async_playwright() as pw:
            browser = await pw.chromium.launch(headless=True)
            page = await browser.new_page()
            cdp = await page.context.new_cdp_session(page)

            # -- Task 1: Job Search --
            print("\\n  --- Task 1: Job Search (indeed.com) ---")
            await S.run("navigate",         page.goto("https://www.indeed.com", wait_until="domcontentloaded", timeout=15000))
            await S.run("fill_what",        cdp_eval(cdp, "document.querySelector('input#text-input-what').value='software engineer'"))
            await S.run("fill_where",       cdp_eval(cdp, "document.querySelector('input#text-input-where').value='San Francisco'"))
            await S.run("click_search",     cdp_eval(cdp, "document.querySelector('button[type=submit]')?.click()"))
            await S.run("wait_results",     page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_job_cards",   cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('[data-testid=slider_item],.job_seen_beacon,.resultContent')).slice(0,10).map(el=>({title:el.querySelector('h2,a')?.textContent?.trim(),company:el.querySelector('[data-testid=company-name],.companyName')?.textContent?.trim()})))"""))
            await S.run("ax_tree",          cdp.send("Accessibility.getFullAXTree"))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png"}))

            # -- Task 2: Product Comparison --
            print("\\n  --- Task 2: Product Comparison (amazon.com) ---")
            await S.run("navigate",         page.goto("https://www.amazon.com", wait_until="domcontentloaded", timeout=15000))
            await S.run("fill_search",      cdp_eval(cdp, "document.querySelector('input#twotabsearchtextbox').value='usb-c cable'"))
            await S.run("click_search",     cdp_eval(cdp, "document.querySelector('input#nav-search-submit-button')?.click()"))
            await S.run("wait_results",     page.wait_for_load_state("domcontentloaded"))
            await S.run("ax_tree",          cdp.send("Accessibility.getFullAXTree"))
            await S.run("eval_first",       cdp_eval(cdp, """JSON.stringify((() => { const c = document.querySelector('[data-component-type=s-search-result]'); return { title: c?.querySelector('h2')?.textContent?.trim(), price: c?.querySelector('.a-price .a-offscreen')?.textContent, rating: c?.querySelector('.a-icon-alt')?.textContent }; })())"""))
            await S.run("click_first",      cdp_eval(cdp, "document.querySelector('[data-component-type=s-search-result] h2 a')?.click()"))
            await S.run("wait_product",     page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_product",     cdp_eval(cdp, """JSON.stringify({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent,rating:document.querySelector('#acrPopover .a-icon-alt')?.textContent})"""))
            await S.run("go_back",          cdp_eval(cdp, "history.back()"))
            await S.run("wait_back",        page.wait_for_load_state("domcontentloaded"))
            await S.run("click_second",     cdp_eval(cdp, "document.querySelector('[data-component-type=s-search-result]:nth-child(2) h2 a')?.click()"))
            await S.run("eval_product_2",   cdp_eval(cdp, """JSON.stringify({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent})"""))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png"}))

            # -- Task 3: Travel Research --
            print("\\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
            await S.run("navigate",         page.goto("https://www.google.com/travel/flights", wait_until="domcontentloaded", timeout=15000))
            await S.run("eval_find_inputs", cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('input')).map(i=>({name:i.name,placeholder:i.placeholder,ariaLabel:i.getAttribute('aria-label')})))"""))
            await S.run("fill_origin",      cdp_eval(cdp, "document.querySelector('input[aria-label*=Where]').value='SFO'"))
            await S.run("fill_dest",        cdp_eval(cdp, "document.querySelector('input[aria-label*=Where]:last-of-type').value='JFK'"))
            await S.run("ax_tree",          cdp.send("Accessibility.getFullAXTree"))
            await S.run("eval_flights",     cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('[class*=result],[role=listitem]')).slice(0,5).map(el=>el.textContent?.trim()?.slice(0,200)))"""))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png"}))

            # -- Task 4: News Aggregation --
            print("\\n  --- Task 4: News Aggregation (news.ycombinator.com) ---")
            await S.run("navigate",         page.goto("https://news.ycombinator.com", wait_until="domcontentloaded"))
            await S.run("ax_tree",          cdp.send("Accessibility.getFullAXTree"))
            await S.run("eval_stories",     cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent})))"""))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png"}))
            await S.run("click_more",       cdp_eval(cdp, "document.querySelector('a.morelink')?.click()"))
            await S.run("wait_page2",       page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_page2",       cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent})))"""))
            await S.run("click_comments",   cdp_eval(cdp, "document.querySelector('.subtext a[href*=item]')?.click()"))
            await S.run("wait_comments",    page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_comments",    cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('.commtext')).slice(0,5).map(c=>c.textContent?.trim()?.slice(0,200)))"""))
            await S.run("ax_tree_comments", cdp.send("Accessibility.getFullAXTree"))

            # -- Task 5: Reference Lookup --
            print("\\n  --- Task 5: Reference Lookup (wikipedia.org) ---")
            await S.run("navigate",         page.goto("https://en.wikipedia.org/wiki/Special:Search?search=Chromium+web+browser", wait_until="domcontentloaded"))
            await S.run("click_first",      cdp_eval(cdp, "document.querySelector('.mw-search-result-heading a')?.click()"))
            await S.run("wait_article",     page.wait_for_load_state("domcontentloaded"))
            await S.run("ax_tree",          cdp.send("Accessibility.getFullAXTree"))
            await S.run("eval_toc",         cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).map(a=>a.textContent.trim()).filter(Boolean))"""))
            await S.run("eval_infobox",     cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim()))"""))
            await S.run("eval_scroll",      cdp_eval(cdp, "window.scrollTo(0,document.querySelector('#History')?.offsetTop||0)"))
            await S.run("eval_history_tbl", cdp_eval(cdp, """JSON.stringify(Array.from(document.querySelectorAll('#History ~ table, #History ~ .wikitable')).slice(0,2).map(t=>t.textContent?.trim()?.slice(0,300)))"""))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png", "captureBeyondViewport": True}))

            # -- Task 6: Form Submission --
            print("\\n  --- Task 6: Form Submission (httpbin.org/forms/post) ---")
            await S.run("navigate",         page.goto("https://httpbin.org/forms/post", wait_until="domcontentloaded"))
            await S.run("ax_tree_form",     cdp.send("Accessibility.getFullAXTree"))
            await S.run("fill_name",        cdp_eval(cdp, "document.querySelector('input[name=custname]').value='John Doe'"))
            await S.run("fill_phone",       cdp_eval(cdp, "document.querySelector('input[name=custtel]').value='555-0123'"))
            await S.run("fill_email",       cdp_eval(cdp, "document.querySelector('input[name=custemail]').value='john@example.com'"))
            await S.run("fill_comments",    cdp_eval(cdp, "document.querySelector('textarea[name=comments]').value='This is a benchmark test for agent-browser-bench v0.2.0'"))
            await S.run("set_radio",        cdp_eval(cdp, "document.querySelector('input[name=size][value=medium]').checked=true"))
            await S.run("set_topping_1",    cdp_eval(cdp, "document.querySelector('input[name=topping][value=cheese]').checked=true"))
            await S.run("set_topping_2",    cdp_eval(cdp, "document.querySelector('input[name=topping][value=mushroom]').checked=true"))
            await S.run("submit",           cdp_eval(cdp, "document.querySelector('button[type=submit]')?.click()"))
            await S.run("wait_response",    page.wait_for_load_state("domcontentloaded"))
            await S.run("eval_response",    cdp_eval(cdp, "document.body.textContent?.trim()?.slice(0,500)"))
            await S.run("verify_values",    cdp_eval(cdp, """JSON.stringify({has_name:document.body.textContent.includes('John Doe'),has_phone:document.body.textContent.includes('555-0123'),has_email:document.body.textContent.includes('john@example.com')})"""))
            await S.run("screenshot",       cdp.send("Page.captureScreenshot", {"format": "png"}))

            await browser.close()

    asyncio.run(main())
    ''').replace("SCREENSHOT_DIR", SCREENSHOT_DIR)

    path = os.path.join(tempfile.gettempdir(), "bench_cdp_raw.py")
    with open(path, "w") as f:
        f.write(script)
    run_script(path)


# ---------------------------------------------------------------------------
# Framework 4: CEF (our fork, zun)
# ---------------------------------------------------------------------------

def run_cef():
    """Benchmark CEF via cefclient + websocket CDP connection."""
    script = STEP_HELPER + textwrap.dedent('''\
    import subprocess, urllib.request

    async def main():
        import websockets

        # -- Start Xvfb --
        xvfb = subprocess.Popen(
            ["Xvfb", "CEF_DISPLAY", "-screen", "0", "1920x1080x24"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
        )
        os.environ["DISPLAY"] = "CEF_DISPLAY"
        await asyncio.sleep(1)

        # -- Start cefclient --
        cef = subprocess.Popen(
            [
                "CEF_BINARY",
                "--no-sandbox",
                "--remote-debugging-port=CEF_PORT",
                "--off-screen-rendering-enabled",
            ],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
            env={**os.environ, "DISPLAY": "CEF_DISPLAY"},
        )
        await asyncio.sleep(3)

        # -- Discover CDP websocket endpoint --
        ws_url = None
        for attempt in range(10):
            try:
                resp = urllib.request.urlopen("http://127.0.0.1:CEF_PORT/json/version")
                info = json.loads(resp.read())
                ws_url = info.get("webSocketDebuggerUrl")
                if ws_url:
                    break
            except Exception:
                await asyncio.sleep(1)

        if not ws_url:
            print("  FAIL: Could not connect to CEF CDP endpoint")
            cef.terminate()
            xvfb.terminate()
            return

        print(f"  Connected to CEF at {ws_url}")

        msg_id = 0

        async def cdp_send(ws, method, params=None):
            nonlocal msg_id
            msg_id += 1
            cmd = {"id": msg_id, "method": method}
            if params:
                cmd["params"] = params
            await ws.send(json.dumps(cmd))
            while True:
                raw = await asyncio.wait_for(ws.recv(), timeout=30)
                resp = json.loads(raw)
                if resp.get("id") == msg_id:
                    if "error" in resp:
                        raise RuntimeError(resp["error"].get("message", str(resp["error"])))
                    return resp.get("result", {})

        async def cdp_eval(ws, expr):
            r = await cdp_send(ws, "Runtime.evaluate", {
                "expression": expr,
                "returnByValue": True,
                "awaitPromise": True,
            })
            return r.get("result", {}).get("value")

        async def cdp_navigate(ws, url):
            await cdp_send(ws, "Page.navigate", {"url": url})
            # Wait for load
            await asyncio.sleep(2)

        try:
            async with websockets.connect(ws_url, max_size=50*1024*1024) as ws:
                # Enable domains
                await cdp_send(ws, "Page.enable")
                await cdp_send(ws, "Runtime.enable")

                # -- Task 1: Job Search --
                print("\\n  --- Task 1: Job Search (indeed.com) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://www.indeed.com"))
                await S.run("fill_what",        cdp_eval(ws, "document.querySelector('input#text-input-what').value='software engineer'"))
                await S.run("fill_where",       cdp_eval(ws, "document.querySelector('input#text-input-where').value='San Francisco'"))
                await S.run("click_search",     cdp_eval(ws, "document.querySelector('button[type=submit]')?.click()"))
                await S.run("wait_load",        asyncio.sleep(3))
                await S.run("eval_job_cards",   cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('[data-testid=slider_item],.job_seen_beacon,.resultContent')).slice(0,10).map(el=>({title:el.querySelector('h2,a')?.textContent?.trim()})))"""))
                await S.run("ax_tree",          cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png"}))

                # -- Task 2: Product Comparison --
                print("\\n  --- Task 2: Product Comparison (amazon.com) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://www.amazon.com"))
                await S.run("fill_search",      cdp_eval(ws, "document.querySelector('input#twotabsearchtextbox').value='usb-c cable'"))
                await S.run("click_search",     cdp_eval(ws, "document.querySelector('input#nav-search-submit-button')?.click()"))
                await S.run("wait_results",     asyncio.sleep(3))
                await S.run("ax_tree",          cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("eval_first",       cdp_eval(ws, """JSON.stringify((() => { const c = document.querySelector('[data-component-type=s-search-result]'); return { title: c?.querySelector('h2')?.textContent?.trim(), price: c?.querySelector('.a-price .a-offscreen')?.textContent }; })())"""))
                await S.run("click_first",      cdp_eval(ws, "document.querySelector('[data-component-type=s-search-result] h2 a')?.click()"))
                await S.run("wait_product",     asyncio.sleep(3))
                await S.run("eval_product",     cdp_eval(ws, """JSON.stringify({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent})"""))
                await S.run("go_back",          cdp_eval(ws, "history.back()"))
                await S.run("wait_back",        asyncio.sleep(2))
                await S.run("click_second",     cdp_eval(ws, "document.querySelector('[data-component-type=s-search-result]:nth-child(2) h2 a')?.click()"))
                await S.run("eval_product_2",   cdp_eval(ws, """JSON.stringify({title:document.querySelector('#productTitle')?.textContent?.trim(),price:document.querySelector('.a-price .a-offscreen')?.textContent})"""))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png"}))

                # -- Task 3: Travel Research --
                print("\\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://www.google.com/travel/flights"))
                await S.run("eval_find_inputs", cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('input')).map(i=>({name:i.name,ariaLabel:i.getAttribute('aria-label')})))"""))
                await S.run("fill_origin",      cdp_eval(ws, "document.querySelector('input[aria-label*=Where]').value='SFO'"))
                await S.run("fill_dest",        cdp_eval(ws, "document.querySelector('input[aria-label*=Where]:last-of-type').value='JFK'"))
                await S.run("ax_tree",          cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("eval_flights",     cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('[class*=result],[role=listitem]')).slice(0,5).map(el=>el.textContent?.trim()?.slice(0,200)))"""))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png"}))

                # -- Task 4: News Aggregation --
                print("\\n  --- Task 4: News Aggregation (news.ycombinator.com) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://news.ycombinator.com"))
                await S.run("ax_tree",          cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("eval_stories",     cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent})))"""))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png"}))
                await S.run("click_more",       cdp_eval(ws, "document.querySelector('a.morelink')?.click()"))
                await S.run("wait_page2",       asyncio.sleep(2))
                await S.run("eval_page2",       cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,url:el.querySelector('.titleline a')?.href,score:el.nextElementSibling?.querySelector('.score')?.textContent})))"""))
                await S.run("click_comments",   cdp_eval(ws, "document.querySelector('.subtext a[href*=item]')?.click()"))
                await S.run("wait_comments",    asyncio.sleep(2))
                await S.run("eval_comments",    cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('.commtext')).slice(0,5).map(c=>c.textContent?.trim()?.slice(0,200)))"""))
                await S.run("ax_tree_comments", cdp_send(ws, "Accessibility.getFullAXTree"))

                # -- Task 5: Reference Lookup --
                print("\\n  --- Task 5: Reference Lookup (wikipedia.org) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://en.wikipedia.org/wiki/Special:Search?search=Chromium+web+browser"))
                await S.run("click_first",      cdp_eval(ws, "document.querySelector('.mw-search-result-heading a')?.click()"))
                await S.run("wait_article",     asyncio.sleep(3))
                await S.run("ax_tree",          cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("eval_toc",         cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).map(a=>a.textContent.trim()).filter(Boolean))"""))
                await S.run("eval_infobox",     cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim()))"""))
                await S.run("eval_scroll",      cdp_eval(ws, "window.scrollTo(0,document.querySelector('#History')?.offsetTop||0)"))
                await S.run("eval_history_tbl", cdp_eval(ws, """JSON.stringify(Array.from(document.querySelectorAll('#History ~ table, #History ~ .wikitable')).slice(0,2).map(t=>t.textContent?.trim()?.slice(0,300)))"""))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png", "captureBeyondViewport": True}))

                # -- Task 6: Form Submission --
                print("\\n  --- Task 6: Form Submission (httpbin.org/forms/post) ---")
                await S.run("navigate",         cdp_navigate(ws, "https://httpbin.org/forms/post"))
                await S.run("ax_tree_form",     cdp_send(ws, "Accessibility.getFullAXTree"))
                await S.run("fill_name",        cdp_eval(ws, "document.querySelector('input[name=custname]').value='John Doe'"))
                await S.run("fill_phone",       cdp_eval(ws, "document.querySelector('input[name=custtel]').value='555-0123'"))
                await S.run("fill_email",       cdp_eval(ws, "document.querySelector('input[name=custemail]').value='john@example.com'"))
                await S.run("fill_comments",    cdp_eval(ws, "document.querySelector('textarea[name=comments]').value='This is a benchmark test for agent-browser-bench v0.2.0'"))
                await S.run("set_radio",        cdp_eval(ws, "document.querySelector('input[name=size][value=medium]').checked=true"))
                await S.run("set_topping_1",    cdp_eval(ws, "document.querySelector('input[name=topping][value=cheese]').checked=true"))
                await S.run("set_topping_2",    cdp_eval(ws, "document.querySelector('input[name=topping][value=mushroom]').checked=true"))
                await S.run("submit",           cdp_eval(ws, "document.querySelector('button[type=submit]')?.click()"))
                await S.run("wait_response",    asyncio.sleep(2))
                await S.run("eval_response",    cdp_eval(ws, "document.body.textContent?.trim()?.slice(0,500)"))
                await S.run("verify_values",    cdp_eval(ws, """JSON.stringify({has_name:document.body.textContent.includes('John Doe'),has_phone:document.body.textContent.includes('555-0123'),has_email:document.body.textContent.includes('john@example.com')})"""))
                await S.run("screenshot",       cdp_send(ws, "Page.captureScreenshot", {"format": "png"}))

        except Exception as e:
            print(f"  CEF session error: {e}")
            traceback.print_exc()
        finally:
            cef.terminate()
            xvfb.terminate()
            try:
                cef.wait(timeout=5)
            except Exception:
                cef.kill()
            try:
                xvfb.wait(timeout=5)
            except Exception:
                xvfb.kill()

    asyncio.run(main())
    ''').replace(
        "CEF_BINARY", CEF_BINARY
    ).replace(
        "CEF_DISPLAY", CEF_DISPLAY
    ).replace(
        "CEF_PORT", str(CEF_DEBUG_PORT)
    ).replace(
        "SCREENSHOT_DIR", SCREENSHOT_DIR
    )

    path = os.path.join(tempfile.gettempdir(), "bench_cef.py")
    with open(path, "w") as f:
        f.write(script)
    run_script(path)


# ---------------------------------------------------------------------------
# Framework registry
# ---------------------------------------------------------------------------

FRAMEWORKS = [
    ("agent-browser v0.22.3",                          run_agent_browser),
    ("Playwright v1.58.0 (browser-use/stagehand)",     run_playwright),
    ("CDP Raw (Chrome DevTools Protocol)",              run_cdp),
    ("CEF (zun fork, cefclient)",                       run_cef),
]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    os.makedirs(SCREENSHOT_DIR, exist_ok=True)

    print("=" * 72)
    print("  agent-browser-bench v0.2.0 -- Real Agentic Tasks")
    print(f"  Date:       {time.strftime('%Y-%m-%d %H:%M:%S %Z')}")
    print(f"  Machine:    {os.cpu_count()} cores, Linux")
    print(f"  Tasks:      6 (indeed, amazon, flights, hn, wikipedia, httpbin)")
    print(f"  Frameworks: {len(FRAMEWORKS)} (sequential, full machine each)")
    print("=" * 72)

    for name, fn in FRAMEWORKS:
        # Clean slate before each framework
        print(f"\n>>> Cleaning up before {name}...")
        cleanup_all_browsers()

        print(f"\n{'=' * 72}")
        print(f"  {name}")
        print(f"{'=' * 72}")

        fw_start = time.perf_counter()
        try:
            fn()
        except Exception as e:
            print(f"\n  FRAMEWORK CRASHED: {e}")
            traceback.print_exc()
        fw_ms = (time.perf_counter() - fw_start) * 1000
        print(f"\n  Total wall time: {fw_ms / 1000:.1f}s")

        # Clean up after
        cleanup_all_browsers()

    print(f"\n{'=' * 72}")
    print("  BENCHMARK COMPLETE")
    print(f"  {time.strftime('%Y-%m-%d %H:%M:%S %Z')}")
    print(f"{'=' * 72}")


if __name__ == "__main__":
    main()
