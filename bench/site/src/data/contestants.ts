import type {
  ContestantInfo,
  DataProvenance,
  ExternalScore,
} from "../types/benchmark";

const VENDOR_REPORTED_PROVENANCE: DataProvenance = {
  origin: "vendor_report",
  trust: "untrusted",
  collectedFrom: "https://browser-use.com/posts/online-mind2web-benchmark",
  notes:
    "Vendor-published benchmark figures are kept for context only and must " +
    "not be merged into first-party rankings or trajectory distillation.",
};

const PUBLIC_LEADERBOARD_PROVENANCE: DataProvenance = {
  origin: "public_leaderboard",
  trust: "untrusted",
  collectedFrom: "https://leaderboard.steel.dev/",
  notes:
    "Third-party leaderboard snapshot retained as contextual data only. " +
    "Treat as untrusted until independently reproduced.",
};

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
      "Python browser automation framework with agent loop and " +
      "auto-research optimization",
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
    provenance: VENDOR_REPORTED_PROVENANCE,
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "UI-TARS-2",
    score: 88,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
    provenance: VENDOR_REPORTED_PROVENANCE,
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "OpenAI Operator",
    score: 61,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
    provenance: VENDOR_REPORTED_PROVENANCE,
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "Stagehand (Gemini 2.5)",
    score: 65,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
    provenance: VENDOR_REPORTED_PROVENANCE,
  },
  {
    benchmark: "Online-Mind2Web",
    agent: "Google Gemini CUA",
    score: 69,
    sourceUrl: "https://browser-use.com/posts/online-mind2web-benchmark",
    date: "2026-03",
    provenance: VENDOR_REPORTED_PROVENANCE,
  },

  // WebVoyager (steel.dev leaderboard)
  {
    benchmark: "WebVoyager",
    agent: "Surfer 2",
    score: 97.1,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
    provenance: PUBLIC_LEADERBOARD_PROVENANCE,
  },
  {
    benchmark: "WebVoyager",
    agent: "Browser Use",
    score: 89.1,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
    provenance: PUBLIC_LEADERBOARD_PROVENANCE,
  },
  {
    benchmark: "WebVoyager",
    agent: "OpenAI Operator",
    score: 87,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
    provenance: PUBLIC_LEADERBOARD_PROVENANCE,
  },
  {
    benchmark: "WebVoyager",
    agent: "Stagehand",
    score: 65,
    sourceUrl: "https://leaderboard.steel.dev/",
    date: "2026-02",
    provenance: PUBLIC_LEADERBOARD_PROVENANCE,
  },
];
