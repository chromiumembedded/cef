import type { PercentileStats, SpeedResult } from "../types/benchmark";

/**
 * Compute percentile statistics from an array of timing measurements.
 */
export function computeStats(timings: number[]): PercentileStats {
  if (timings.length === 0) {
    return { p50: 0, p95: 0, p99: 0, min: 0, max: 0, mean: 0, stddev: 0 };
  }

  const sorted = [...timings].sort((a, b) => a - b);
  const n = sorted.length;

  const percentile = (p: number): number => {
    const idx = (p / 100) * (n - 1);
    const lower = Math.floor(idx);
    const upper = Math.ceil(idx);
    if (lower === upper) return sorted[lower];
    return sorted[lower] + (idx - lower) * (sorted[upper] - sorted[lower]);
  };

  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const variance = sorted.reduce((a, b) => a + (b - mean) ** 2, 0) / n;

  return {
    p50: percentile(50),
    p95: percentile(95),
    p99: percentile(99),
    min: sorted[0],
    max: sorted[n - 1],
    mean,
    stddev: Math.sqrt(variance),
  };
}

/**
 * Compute geometric mean of an array of positive numbers.
 * Used for overall ranking — treats each task equally regardless of scale.
 */
export function geometricMean(values: number[]): number {
  if (values.length === 0) return 0;
  const product = values.reduce((a, b) => a * Math.log(b), 0);
  return Math.exp(product / values.length);
}

/**
 * Format milliseconds for display.
 */
export function formatMs(ms: number): string {
  if (ms < 1) return `${ms.toFixed(2)}ms`;
  if (ms < 10) return `${ms.toFixed(1)}ms`;
  if (ms < 1000) return `${Math.round(ms)}ms`;
  return `${(ms / 1000).toFixed(2)}s`;
}

/**
 * Format a percentage for display.
 */
export function formatPct(pct: number): string {
  return `${pct.toFixed(1)}%`;
}

/**
 * Determine the winner for a task (lowest p50).
 */
export function taskWinner(
  results: Map<string, SpeedResult>
): { driver: string; p50: number } | null {
  let best: { driver: string; p50: number } | null = null;

  for (const [driver, result] of results) {
    if (!result.allSucceeded) continue;
    const stats = computeStats(result.timings);
    if (!best || stats.p50 < best.p50) {
      best = { driver, p50: stats.p50 };
    }
  }

  return best;
}

/**
 * Compute the speedup factor of driver A vs driver B for a task.
 * Returns how many times faster A is (> 1 means A is faster).
 */
export function speedup(aMs: number, bMs: number): number {
  if (aMs <= 0) return Infinity;
  return bMs / aMs;
}

/**
 * Generate a markdown table from benchmark results.
 */
export function toMarkdownTable(
  headers: string[],
  rows: string[][]
): string {
  const widths = headers.map((h, i) =>
    Math.max(h.length, ...rows.map((r) => (r[i] || "").length))
  );

  const pad = (s: string, w: number) => s.padEnd(w);
  const sep = widths.map((w) => "-".repeat(w));

  const lines = [
    `| ${headers.map((h, i) => pad(h, widths[i])).join(" | ")} |`,
    `| ${sep.map((s) => s).join(" | ")} |`,
    ...rows.map(
      (r) => `| ${r.map((c, i) => pad(c || "", widths[i])).join(" | ")} |`
    ),
  ];

  return lines.join("\n");
}
