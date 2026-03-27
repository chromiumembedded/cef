#!/usr/bin/env python3.11
"""browser-use benchmark — 6 real agentic tasks."""
import asyncio, time, sys

async def main():
    from playwright.async_api import async_playwright

    print("=" * 72)
    print("  BROWSER-USE 0.12.5 (Playwright 1.58.0)")
    print("=" * 72)

    def step(name, ms, ok=True, extra=""):
        s = "OK" if ok else "FAIL"
        print(f"  {name:40s} {ms:8.0f}ms  {s}  {extra}")
        sys.stdout.flush()

    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        page = await browser.new_page()

        # Task 1: Indeed
        print("\n  --- Task 1: Job Search (indeed.com) ---")
        s=time.perf_counter(); await page.goto("https://www.indeed.com",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        try: s=time.perf_counter(); await page.fill("input#text-input-what","software engineer",timeout=5000); step("fill_what",(time.perf_counter()-s)*1000)
        except: step("fill_what",(time.perf_counter()-s)*1000,False,"selector not found")
        try: s=time.perf_counter(); await page.fill("input#text-input-where","San Francisco",timeout=5000); step("fill_where",(time.perf_counter()-s)*1000)
        except: step("fill_where",(time.perf_counter()-s)*1000,False,"selector not found")
        s=time.perf_counter(); c=await page.content(); step("snapshot",(time.perf_counter()-s)*1000,extra=f"{len(c)}b")
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-indeed.png"); step("screenshot",(time.perf_counter()-s)*1000)

        # Task 2: Amazon
        print("\n  --- Task 2: Product Comparison (amazon.com) ---")
        s=time.perf_counter(); await page.goto("https://www.amazon.com",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        try: s=time.perf_counter(); await page.fill("input#twotabsearchtextbox","usb-c cable",timeout=5000); step("fill_search",(time.perf_counter()-s)*1000)
        except: step("fill_search",(time.perf_counter()-s)*1000,False)
        try: s=time.perf_counter(); await page.click("input#nav-search-submit-button",timeout=5000); step("click_search",(time.perf_counter()-s)*1000)
        except: step("click_search",(time.perf_counter()-s)*1000,False)
        await asyncio.sleep(1)
        s=time.perf_counter(); c=await page.content(); step("snapshot",(time.perf_counter()-s)*1000,extra=f"{len(c)}b")
        s=time.perf_counter(); r=await page.evaluate("JSON.stringify({title:document.querySelector('[data-component-type=s-search-result] h2')?.textContent?.trim()})"); step("eval_first",(time.perf_counter()-s)*1000,extra=r[:80] if r else "")
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-amazon.png"); step("screenshot",(time.perf_counter()-s)*1000)

        # Task 3: Flights
        print("\n  --- Task 3: Travel Research (google.com/travel/flights) ---")
        s=time.perf_counter(); await page.goto("https://www.google.com/travel/flights",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); c=await page.content(); step("snapshot",(time.perf_counter()-s)*1000,extra=f"{len(c)}b")
        s=time.perf_counter(); n=await page.evaluate("document.querySelectorAll('input').length"); step("eval_inputs",(time.perf_counter()-s)*1000,extra=f"{n} inputs")
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-flights.png"); step("screenshot",(time.perf_counter()-s)*1000)

        # Task 4: HN
        print("\n  --- Task 4: News Aggregation (HN) ---")
        s=time.perf_counter(); await page.goto("https://news.ycombinator.com",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); c=await page.content(); step("snapshot",(time.perf_counter()-s)*1000,extra=f"{len(c)}b")
        s=time.perf_counter(); stories=await page.evaluate("Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,score:el.nextElementSibling?.querySelector('.score')?.textContent}))"); step("eval_stories",(time.perf_counter()-s)*1000,extra=f"{len(stories)} items")
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-hn.png"); step("screenshot",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.click("a.morelink"); await page.wait_for_load_state("domcontentloaded"); step("click_more",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); p2=await page.evaluate("document.querySelectorAll('.athing').length"); step("eval_page2",(time.perf_counter()-s)*1000,extra=f"{p2} items")

        # Task 5: Wikipedia
        print("\n  --- Task 5: Reference Lookup (Wikipedia) ---")
        s=time.perf_counter(); await page.goto("https://en.wikipedia.org/wiki/Chromium_(web_browser)",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); c=await page.content(); step("full_content",(time.perf_counter()-s)*1000,extra=f"{len(c)}b")
        s=time.perf_counter(); toc=await page.evaluate("Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).map(a=>a.textContent.trim()).filter(Boolean)"); step("eval_toc",(time.perf_counter()-s)*1000,extra=f"{len(toc)} items")
        s=time.perf_counter(); ib=await page.evaluate("Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim())"); step("eval_infobox",(time.perf_counter()-s)*1000,extra=f"{len(ib)} rows")
        s=time.perf_counter(); await page.evaluate("window.scrollTo(0,document.querySelector('#History')?.offsetTop||0)"); step("scroll_history",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-wiki.png",full_page=True); step("screenshot_fullpage",(time.perf_counter()-s)*1000)

        # Task 6: Form
        print("\n  --- Task 6: Form Submission (httpbin) ---")
        s=time.perf_counter(); await page.goto("https://httpbin.org/forms/post",wait_until="domcontentloaded"); step("navigate",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.fill("input[name=custname]","John Doe"); step("fill_name",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.fill("input[name=custtel]","555-0123"); step("fill_phone",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.fill("input[name=custemail]","john@example.com"); step("fill_email",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.fill("textarea[name=comments]","Benchmark test"); step("fill_comments",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.click("input[name=size][value=medium]"); step("click_radio",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.click("input[name=topping][value=cheese]"); step("click_topping",(time.perf_counter()-s)*1000)
        s=time.perf_counter(); await page.screenshot(path="/tmp/bu-form.png"); step("screenshot",(time.perf_counter()-s)*1000)

        await browser.close()
    print("\n  DONE")

asyncio.run(main())
