const { Stagehand } = require("@browserbasehq/stagehand");
(async () => {
  let s;
  console.log("========================================================================");
  console.log("  STAGEHAND 3.2.0");
  console.log("========================================================================");

  s = Date.now();
  const stagehand = new Stagehand({ env: "LOCAL", headless: true, verbose: 0 });
  await stagehand.init();
  console.log("  cold_start                                  " + (Date.now()-s) + "ms  OK");
  const page = stagehand.page;

  // Task 1: Indeed
  console.log("\n  --- Task 1: Job Search (indeed.com) ---");
  s=Date.now(); await page.goto("https://www.indeed.com"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  try { s=Date.now(); await page.fill("input#text-input-what","software engineer"); console.log("  fill_what                                   " + (Date.now()-s) + "ms  OK"); } catch(e) { console.log("  fill_what                                   " + (Date.now()-s) + "ms  FAIL  " + e.message.slice(0,60)); }
  s=Date.now(); const ic=await page.content(); console.log("  snapshot                                    " + (Date.now()-s) + "ms  OK  " + ic.length + "b");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-indeed.png"}); console.log("  screenshot                                  " + (Date.now()-s) + "ms  OK");

  // Task 2: Amazon
  console.log("\n  --- Task 2: Product Comparison (amazon.com) ---");
  s=Date.now(); await page.goto("https://www.amazon.com"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  try { s=Date.now(); await page.fill("input#twotabsearchtextbox","usb-c cable"); console.log("  fill_search                                 " + (Date.now()-s) + "ms  OK"); } catch(e) { console.log("  fill_search                                 " + (Date.now()-s) + "ms  FAIL  " + e.message.slice(0,60)); }
  try { s=Date.now(); await page.click("input#nav-search-submit-button"); console.log("  click_search                                " + (Date.now()-s) + "ms  OK"); } catch(e) { console.log("  click_search                                " + (Date.now()-s) + "ms  FAIL"); }
  s=Date.now(); const ac=await page.content(); console.log("  snapshot                                    " + (Date.now()-s) + "ms  OK  " + ac.length + "b");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-amazon.png"}); console.log("  screenshot                                  " + (Date.now()-s) + "ms  OK");

  // Task 3: Flights
  console.log("\n  --- Task 3: Travel Research (google.com/travel/flights) ---");
  s=Date.now(); await page.goto("https://www.google.com/travel/flights"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  s=Date.now(); const fc=await page.content(); console.log("  snapshot                                    " + (Date.now()-s) + "ms  OK  " + fc.length + "b");
  s=Date.now(); const fi=await page.evaluate("document.querySelectorAll('input').length"); console.log("  eval_inputs                                 " + (Date.now()-s) + "ms  OK  " + fi + " inputs");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-flights.png"}); console.log("  screenshot                                  " + (Date.now()-s) + "ms  OK");

  // Task 4: HN
  console.log("\n  --- Task 4: News Aggregation (HN) ---");
  s=Date.now(); await page.goto("https://news.ycombinator.com"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  s=Date.now(); const hc=await page.content(); console.log("  snapshot                                    " + (Date.now()-s) + "ms  OK  " + hc.length + "b");
  s=Date.now(); const stories=await page.evaluate("Array.from(document.querySelectorAll('.athing')).map(el=>({title:el.querySelector('.titleline a')?.textContent,score:el.nextElementSibling?.querySelector('.score')?.textContent}))"); console.log("  eval_stories                                " + (Date.now()-s) + "ms  OK  " + stories.length + " items");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-hn.png"}); console.log("  screenshot                                  " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.click("a.morelink"); await page.waitForLoadState("domcontentloaded"); console.log("  click_more                                  " + (Date.now()-s) + "ms  OK");
  s=Date.now(); const p2=await page.evaluate("document.querySelectorAll('.athing').length"); console.log("  eval_page2                                  " + (Date.now()-s) + "ms  OK  " + p2 + " items");

  // Task 5: Wikipedia
  console.log("\n  --- Task 5: Reference Lookup (Wikipedia) ---");
  s=Date.now(); await page.goto("https://en.wikipedia.org/wiki/Chromium_(web_browser)"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  s=Date.now(); const wc=await page.content(); console.log("  full_content                                " + (Date.now()-s) + "ms  OK  " + wc.length + "b");
  s=Date.now(); const toc=await page.evaluate("Array.from(document.querySelectorAll('#toc a,.toc a,[class*=toc] a')).map(a=>a.textContent.trim()).filter(Boolean)"); console.log("  eval_toc                                    " + (Date.now()-s) + "ms  OK  " + toc.length + " items");
  s=Date.now(); const ib=await page.evaluate("Array.from(document.querySelectorAll('table.infobox tr')).slice(0,15).map(tr=>tr.textContent.trim())"); console.log("  eval_infobox                                " + (Date.now()-s) + "ms  OK  " + ib.length + " rows");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-wiki.png",fullPage:true}); console.log("  screenshot_fullpage                         " + (Date.now()-s) + "ms  OK");

  // Task 6: Form
  console.log("\n  --- Task 6: Form Submission (httpbin) ---");
  s=Date.now(); await page.goto("https://httpbin.org/forms/post"); console.log("  navigate                                    " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.fill("input[name=custname]","John Doe"); console.log("  fill_name                                   " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.fill("input[name=custtel]","555-0123"); console.log("  fill_phone                                  " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.fill("input[name=custemail]","john@example.com"); console.log("  fill_email                                  " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.click("input[name=size][value=medium]"); console.log("  click_radio                                 " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.click("input[name=topping][value=cheese]"); console.log("  click_topping                               " + (Date.now()-s) + "ms  OK");
  s=Date.now(); await page.screenshot({path:"/tmp/sh-form.png"}); console.log("  screenshot                                  " + (Date.now()-s) + "ms  OK");

  await stagehand.close();
  console.log("\n  DONE");
})().catch(e => console.error("STAGEHAND CRASH:", e.message));
