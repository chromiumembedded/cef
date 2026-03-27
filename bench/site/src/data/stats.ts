import type {
  AccuracyBenchmarkRun,
  AccuracyResult,
  AccuracyTask,
  DataProvenance,
  DriverAccuracyReport,
  DriverTrajectoryDistillation,
  ExternalScore,
  PercentileStats,
  SpeedResult,
  TaskDifficulty,
  TaskTrajectoryDistillation,
  TrajectoryDistillationReport,
  TrajectorySummary,
} from "../types/benchmark";

const EMPTY_TRAJECTORY_SUMMARY: TrajectorySummary = {
  sampleCount: 0,
  successRate: 0,
  judgeAdjustedSuccessRate: 0,
  partialRate: 0,
  failureRate: 0,
  avgSteps: 0,
  avgTimeMs: 0,
  avgTokens: 0,
  avgBrowserShare: 0,
  avgLlmShare: 0,
  stepNormalizedEfficiency: 0,
  tokenEfficiency: 0,
};

function mean(values: number[]): number {
  if (values.length === 0) {
    return 0;
  }
  return values.reduce((sum, value) => sum + value, 0) / values.length;
}

function isTrustedProvenance(provenance: DataProvenance): boolean {
  return provenance.trust === "trusted";
}

function judgeScore(result: AccuracyResult): number {
  switch (result.judgeVerdict) {
    case "pass":
      return 1;
    case "partial":
      return 0.5;
    case "fail":
      return 0;
    default:
      return result.success ? 1 : 0;
  }
}

export function buildTrajectorySummary(
  results: AccuracyResult[]
): TrajectorySummary {
  if (results.length === 0) {
    return { ...EMPTY_TRAJECTORY_SUMMARY };
  }

  const sampleCount = results.length;
  const successes = results.filter((result) => result.success).length;
  const partials = results.filter(
    (result) => result.judgeVerdict === "partial"
  ).length;
  const judgeAdjustedSuccess = mean(results.map((result) => judgeScore(result)));
  const avgSteps = mean(results.map((result) => result.steps));
  const avgTimeMs = mean(results.map((result) => result.totalTimeMs));
  const avgTokens = mean(results.map((result) => result.tokensUsed));
  const avgBrowserShare = mean(
    results.map((result) =>
      result.totalTimeMs > 0 ? result.browserTimeMs / result.totalTimeMs : 0
    )
  );
  const avgLlmShare = mean(
    results.map((result) =>
      result.totalTimeMs > 0 ? result.llmTimeMs / result.totalTimeMs : 0
    )
  );

  return {
    sampleCount,
    successRate: successes / sampleCount,
    judgeAdjustedSuccessRate: judgeAdjustedSuccess,
    partialRate: partials / sampleCount,
    failureRate: 1 - successes / sampleCount,
    avgSteps,
    avgTimeMs,
    avgTokens,
    avgBrowserShare,
    avgLlmShare,
    stepNormalizedEfficiency: avgSteps > 0 ? judgeAdjustedSuccess / avgSteps : 0,
    tokenEfficiency:
      avgTokens > 0 ? judgeAdjustedSuccess / avgTokens : judgeAdjustedSuccess,
  };
}

function buildTaskIndex(
  tasks: AccuracyTask[]
): Map<string, AccuracyTask> {
  return new Map(tasks.map((task) => [task.taskId, task]));
}

function emptyDifficultyBreakdown(): Record<TaskDifficulty, TrajectorySummary> {
  return {
    easy: { ...EMPTY_TRAJECTORY_SUMMARY },
    medium: { ...EMPTY_TRAJECTORY_SUMMARY },
    hard: { ...EMPTY_TRAJECTORY_SUMMARY },
  };
}

function distillDriverTrajectories(
  driver: DriverAccuracyReport,
  taskIndex: Map<string, AccuracyTask>
): DriverTrajectoryDistillation {
  const resultsByDifficulty: Record<TaskDifficulty, AccuracyResult[]> = {
    easy: [],
    medium: [],
    hard: [],
  };
  const resultsByDomain = new Map<string, AccuracyResult[]>();
  const perTask: Record<string, TaskTrajectoryDistillation> = {};

  for (const result of driver.results) {
    const task = taskIndex.get(result.taskId);
    if (!task) {
      continue;
    }

    resultsByDifficulty[task.difficulty].push(result);

    const domainResults = resultsByDomain.get(task.domain) ?? [];
    domainResults.push(result);
    resultsByDomain.set(task.domain, domainResults);

    const taskSummary = buildTrajectorySummary([result]);
    perTask[task.taskId] = {
      taskId: task.taskId,
      difficulty: task.difficulty,
      domain: task.domain,
      ...taskSummary,
    };
  }

  const byDomain: Record<string, TrajectorySummary> = {};
  for (const [domain, domainResults] of resultsByDomain) {
    byDomain[domain] = buildTrajectorySummary(domainResults);
  }

  return {
    driverName: driver.driverName,
    displayName: driver.displayName,
    provenance: driver.provenance,
    overall: buildTrajectorySummary(driver.results),
    byDifficulty: {
      easy: buildTrajectorySummary(resultsByDifficulty.easy),
      medium: buildTrajectorySummary(resultsByDifficulty.medium),
      hard: buildTrajectorySummary(resultsByDifficulty.hard),
    },
    byDomain,
    perTask,
  };
}

/**
 * Distill trusted accuracy trajectories into compact summaries for rendering.
 * Third-party scores are returned separately and never blended into aggregates.
 */
export function buildTrajectoryDistillationReport(
  accuracy: AccuracyBenchmarkRun,
  externalScores: ExternalScore[] = []
): TrajectoryDistillationReport {
  const taskIndex = buildTaskIndex(accuracy.tasks);
  const trustedDrivers = accuracy.drivers.filter((driver) =>
    isTrustedProvenance(driver.provenance)
  );
  const excludedExternalScores = externalScores.filter(
    (score) => score.provenance.trust === "untrusted"
  );

  return {
    methodology:
      "Summaries are derived only from trusted first-party accuracy runs. " +
      "Judge verdicts are weighted pass=1, partial=0.5, fail=0. " +
      "External leaderboard data is excluded from all aggregates.",
    provenance: {
      origin: "derived",
      trust: "trusted",
      collectedFrom: `trajectory_distillation:${accuracy.runId}`,
      notes:
        "Derived from trusted accuracy trajectories in this benchmark report.",
    },
    drivers: trustedDrivers.map((driver) =>
      distillDriverTrajectories(driver, taskIndex)
    ),
    excludedExternalScores,
  };
}

/**
 * Compute percentile statistics from an array of timing measurements.
 */
export function computeStats(timings: number[]): PercentileStats {
  if (timings.length === 0) {
    return { p50: 0, p95: 0, p99: 0, min: 0, max: 0, mean: 0, stddev: 0 };
  }

  const sorted = [...timings].sort((a, b) => a - b);
  const n = sorted.length;
  const first = sorted[0];
  const last = sorted[n - 1];
  if (first === undefined || last === undefined) {
    return { p50: 0, p95: 0, p99: 0, min: 0, max: 0, mean: 0, stddev: 0 };
  }

  const percentile = (p: number): number => {
    const idx = (p / 100) * (n - 1);
    const lower = Math.floor(idx);
    const upper = Math.ceil(idx);
    const lowerValue = sorted[lower];
    const upperValue = sorted[upper];
    if (lowerValue === undefined || upperValue === undefined) {
      return 0;
    }
    if (lower === upper) return lowerValue;
    return lowerValue + (idx - lower) * (upperValue - lowerValue);
  };

  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const variance = sorted.reduce((a, b) => a + (b - mean) ** 2, 0) / n;

  return {
    p50: percentile(50),
    p95: percentile(95),
    p99: percentile(99),
    min: first,
    max: last,
    mean,
    stddev: Math.sqrt(variance),
  };
}

/**
 * Compute geometric mean of an array of positive numbers.
 * Used for overall ranking — treats each task equally regardless of scale.
 */
export function geometricMean(values: number[]): number {
  const positive = values.filter((value) => value > 0);
  if (positive.length === 0) return 0;
  const logSum = positive.reduce((sum, value) => sum + Math.log(value), 0);
  return Math.exp(logSum / positive.length);
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
  const widths = headers.map((header, index) => {
    let width = header.length;
    for (const row of rows) {
      width = Math.max(width, (row[index] ?? "").length);
    }
    return width;
  });

  const pad = (s: string, w: number) => s.padEnd(w);
  const sep = widths.map((w) => "-".repeat(w));

  const lines = [
    `| ${headers
      .map((header, index) => pad(header, widths[index] ?? header.length))
      .join(" | ")} |`,
    `| ${sep.map((s) => s).join(" | ")} |`,
    ...rows.map(
      (row) =>
        `| ${row
          .map((cell, index) => pad(cell || "", widths[index] ?? cell.length))
          .join(" | ")} |`
    ),
  ];

  return lines.join("\n");
}
