import type { ContestantInfo, ExternalScore } from "../types/benchmark";

export const CONTESTANTS: ContestantInfo[] = [
  {
    name: "zun",
    displayName: "zun (CEF)",
    description:
      "Chromium Embedded Framework fork with native agent-browser APIs, " +
      "dirty-tracking persistence, AX caching, and stealth config",
    url: "https://github.com/maceip/cef",
    language: "C++",
    license: "BSD-3-Clause",
    isOurs: true,
  },
  {
    name: "agent-browser",
    displayName: "agent-browser",
    description:
      "Vercel's Rust CLI for browser automation with snapshot refs, " +
      "annotated screenshots, and session management",
    url: "https://github.com/vercel-labs/agent-browser",
    language: "Rust",
    license: "Apache-2.0",
    isOurs: false,
  },
  {
    name: "browser-use",
    displayName: "Browser Use",
    description:
      "Python browser automation framework with agent loop, " +
      "97% on Online-Mind2Web, auto-research optimization",
    url: "https://github.com/browser-use/browser-use",
    language: "Python",
    license: "MIT",
    isOurs: false,
  },
  {
    name: "stagehand",
    displayName: "Stagehand",
    description:
      "Browserbase's TypeScript SDK combining code precision with " +
      "AI-driven actions via Playwright",
    url: "https://github.com/browserbase/stagehand",
    language: "TypeScript",
    license: "MIT",
    isOurs: false,
  },
  {
    name: "devtools-mcp",
    displayName: "Chrome DevTools MCP",
    description:
      "Official Chrome DevTools MCP server with 29 tools for " +
      "automation, debugging, and performance analysis",
    url: "https://github.com/ChromeDevTools/chrome-devtools-mcp",
    language: "TypeScript",
    license: "Apache-2.0",
    isOurs: false,
  },
];

/**
 * Published scores from external benchmarks for context.
 * These are NOT our measurements — they're from public leaderboards.
 */
export const EXTERNAL_SCORES: ExternalScore[] = [
  // Online-Mind2Web (browser-use.com)
  {
    benchmark: "Online-Mind2Web",
    agent: "Browser Use",
    score: 97,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "UI-TARS-2",
    score: 88,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "OpenAI Operator",
    score: 61,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "Stagehand (Gemini 2.5)",
    score: 65,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "Google Gemini CUA",
    score: 69,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
  },

  // WebVoyager (steel.dev leaderboard)
  {
    benchmark: "WebVoyager",
    agent: "Surfer 2",
    score: 97.1,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
  },
  {
    benchmark: "WebVoyager",
    agent: "Browser Use",
    score: 89.1,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
  },
  {
    benchmark: "WebVoyager",
    agent: "OpenAI Operator",
    score: 87,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
  },
  {
    benchmark: "WebVoyager",
    agent: "Stagehand",
    score: 65,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
  },
];
