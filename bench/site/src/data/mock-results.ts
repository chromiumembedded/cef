import type {
  AccuracyBenchmarkRun,
  AccuracyResult,
  BenchmarkReport,
  DriverAccuracyReport,
  RankEntry,
} from "../types/benchmark";
import { CONTESTANTS, EXTERNAL_SCORES } from "./contestants";
import { ACCURACY_TASKS } from "./tasks";
import {
  buildTrajectoryDistillationReport,
  geometricMean,
} from "./stats";

const FIRST_PARTY_PROVENANCE = {
  origin: "first_party" as const,
  trust: "trusted" as const,
  collectedFrom: "bench/results/mock-accuracy.json",
  notes: "Synthetic first-party benchmark data for development and typechecks.",
};

function buildAccuracyReport(
  driverName: string,
  displayName: string,
  llmModel: string,
  pattern: "strong" | "steady"
): DriverAccuracyReport {
  const results: AccuracyResult[] = ACCURACY_TASKS.map((task, index) => {
    const isStrong = pattern === "strong";
    const succeeds =
      task.difficulty === "easy"
        ? true
        : task.difficulty === "medium"
          ? (isStrong ? index % 5 !== 0 : index % 4 !== 0)
          : isStrong
            ? index % 2 === 0
            : index % 3 === 0;
    const partial =
      !succeeds &&
      task.difficulty !== "easy" &&
      (isStrong ? index % 3 === 0 : index % 4 === 0);
    const stepsBase =
      task.difficulty === "easy" ? 4 : task.difficulty === "medium" ? 8 : 13;
    const timeBase =
      task.difficulty === "easy" ? 18_000 : task.difficulty === "medium" ? 44_000 : 88_000;
    const tokenBase =
      task.difficulty === "easy" ? 1_200 : task.difficulty === "medium" ? 3_000 : 6_600;

    const judgeVerdict: AccuracyResult["judgeVerdict"] = succeeds
      ? "pass"
      : partial
        ? "partial"
        : "fail";

    return {
      taskId: task.taskId,
      success: succeeds,
      steps: stepsBase + (index % 3),
      totalTimeMs: timeBase + index * (isStrong ? 450 : 700),
      llmTimeMs: Math.round((timeBase + index * 250) * (isStrong ? 0.58 : 0.64)),
      browserTimeMs: Math.round((timeBase + index * 200) * (isStrong ? 0.42 : 0.36)),
      llmCalls: succeeds ? (task.difficulty === "hard" ? 7 : 4) : 5,
      tokensUsed: tokenBase + index * (isStrong ? 55 : 70),
      error: succeeds ? undefined : partial ? "Judge marked trajectory partial." : "Agent diverged from target trajectory.",
      judgeVerdict,
      judgeConfidence: succeeds ? 0.92 : partial ? 0.61 : 0.48,
    };
  });

  const total = results.length;
  const successes = results.filter((result) => result.success).length;
  const avgSteps =
    results.reduce((sum, result) => sum + result.steps, 0) / total;
  const avgTimeMs =
    results.reduce((sum, result) => sum + result.totalTimeMs, 0) / total;
  const avgTokens =
    results.reduce((sum, result) => sum + result.tokensUsed, 0) / total;

  const byDifficulty = { easy: 0, medium: 0, hard: 0 };
  const byDifficultyCount = { easy: 0, medium: 0, hard: 0 };
  const byDomain: Record<string, { total: number; success: number }> = {};

  results.forEach((result) => {
    const task = ACCURACY_TASKS.find((candidate) => candidate.taskId === result.taskId);
    if (!task) {
      return;
    }
    byDifficulty[task.difficulty] += result.success ? 1 : 0;
    byDifficultyCount[task.difficulty] += 1;

    const domainEntry = byDomain[task.domain] ?? { total: 0, success: 0 };
    domainEntry.total += 1;
    domainEntry.success += result.success ? 1 : 0;
    byDomain[task.domain] = domainEntry;
  });

  const accuracyByDomain: Record<string, number> = {};
  Object.entries(byDomain).forEach(([domain, summary]) => {
    accuracyByDomain[domain] =
      summary.total === 0 ? 0 : summary.success / summary.total;
  });

  return {
    driverName,
    displayName,
    version: "0.1.0",
    llmModel,
    provenance: FIRST_PARTY_PROVENANCE,
    results,
    overallAccuracy: successes / total,
    accuracyByDifficulty: {
      easy:
        byDifficultyCount.easy === 0
          ? 0
          : byDifficulty.easy / byDifficultyCount.easy,
      medium:
        byDifficultyCount.medium === 0
          ? 0
          : byDifficulty.medium / byDifficultyCount.medium,
      hard:
        byDifficultyCount.hard === 0
          ? 0
          : byDifficulty.hard / byDifficultyCount.hard,
    },
    accuracyByDomain,
    avgSteps,
    avgTimeMs,
    avgTokens,
  };
}

function buildAccuracyRanking(
  reports: DriverAccuracyReport[]
): RankEntry[] {
  return [...reports]
    .sort((left, right) => right.overallAccuracy - left.overallAccuracy)
    .map((report, index) => ({
      rank: index + 1,
      driverName: report.driverName,
      displayName: report.displayName,
      score: report.overallAccuracy,
      scoreLabel: "Overall accuracy",
    }));
}

const ACCURACY_DRIVERS: DriverAccuracyReport[] = [
  buildAccuracyReport("zun", "zun (CEF)", "GPT-4.1", "strong"),
  buildAccuracyReport(
    "stagehand",
    "Stagehand",
    "Gemini 2.5 Flash",
    "steady"
  ),
];

const MOCK_ACCURACY_BASE: AccuracyBenchmarkRun = {
  runId: "mock-accuracy-001",
  timestamp: new Date().toISOString(),
  hardware: {
    cpu: "52-core Intel Xeon",
    cores: 52,
    ramGb: 442,
    os: "Ubuntu 22.04 (Linux 6.8, NVIDIA GPU)",
    disk: "5.4TB NVMe",
    network: "1 Gbps datacenter egress",
  },
  llmModel: "Mixed mock models",
  provenance: FIRST_PARTY_PROVENANCE,
  tasks: ACCURACY_TASKS,
  drivers: ACCURACY_DRIVERS,
  ranking: buildAccuracyRanking(ACCURACY_DRIVERS),
};

const MOCK_ACCURACY: AccuracyBenchmarkRun = {
  ...MOCK_ACCURACY_BASE,
  trajectoryDistillation: buildTrajectoryDistillationReport(
    MOCK_ACCURACY_BASE,
    EXTERNAL_SCORES
  ),
};

/**
 * Mock benchmark results for development.
 * Replace with real data from bench/results/latest.json after running.
 */
export const MOCK_REPORT: BenchmarkReport = {
  schemaVersion: "1.1.0",
  title: "agent-browser-bench v0.1.0",
  description:
    "Infrastructure speed and trajectory-aware accuracy benchmark for browser automation frameworks. " +
    "External leaderboard numbers are shown as untrusted context only.",
  generatedAt: new Date().toISOString(),
  repoUrl: "https://github.com/maceip/cef",
  contestants: CONTESTANTS,
  externalScores: EXTERNAL_SCORES,
  speed: {
    runId: "mock-001",
    timestamp: new Date().toISOString(),
    hardware: {
      cpu: "52-core Intel Xeon",
      cores: 52,
      ramGb: 442,
      os: "Ubuntu 22.04 (Linux 6.8, NVIDIA GPU)",
      disk: "5.4TB NVMe",
    },
    drivers: [
      {
        driverName: "zun",
        displayName: "zun (CEF)",
        version: "0.1.0",
        setupTime: 180,
        teardownTime: 50,
        results: [],
        stats: {
          cold_start: {
            p50: 180,
            p95: 220,
            p99: 250,
            min: 160,
            max: 260,
            mean: 195,
            stddev: 25,
          },
          navigate_complex: {
            p50: 850,
            p95: 1100,
            p99: 1300,
            min: 780,
            max: 1400,
            mean: 900,
            stddev: 120,
          },
          snapshot_full: {
            p50: 45,
            p95: 80,
            p99: 120,
            min: 30,
            max: 130,
            mean: 55,
            stddev: 20,
          },
          snapshot_interactive: {
            p50: 20,
            p95: 35,
            p99: 50,
            min: 15,
            max: 55,
            mean: 25,
            stddev: 10,
          },
          screenshot_plain: {
            p50: 65,
            p95: 100,
            p99: 130,
            min: 50,
            max: 140,
            mean: 75,
            stddev: 20,
          },
          screenshot_annotated: {
            p50: 120,
            p95: 180,
            p99: 220,
            min: 95,
            max: 230,
            mean: 135,
            stddev: 30,
          },
          eval_simple: {
            p50: 3,
            p95: 8,
            p99: 12,
            min: 2,
            max: 15,
            mean: 4,
            stddev: 2,
          },
          eval_complex: {
            p50: 25,
            p95: 45,
            p99: 60,
            min: 18,
            max: 65,
            mean: 30,
            stddev: 10,
          },
          click_wait_snapshot: {
            p50: 890,
            p95: 1200,
            p99: 1500,
            min: 750,
            max: 1600,
            mean: 950,
            stddev: 150,
          },
          navigate_10x: {
            p50: 7500,
            p95: 9000,
            p99: 10000,
            min: 6800,
            max: 10500,
            mean: 8000,
            stddev: 800,
          },
          state_save_load: {
            p50: 30,
            p95: 55,
            p99: 70,
            min: 22,
            max: 75,
            mean: 35,
            stddev: 12,
          },
        },
      },
      {
        driverName: "stagehand",
        displayName: "Stagehand",
        version: "0.17.0",
        setupTime: 320,
        teardownTime: 90,
        results: [],
        stats: {
          cold_start: {
            p50: 410,
            p95: 520,
            p99: 610,
            min: 350,
            max: 640,
            mean: 430,
            stddev: 60,
          },
          navigate_complex: {
            p50: 1040,
            p95: 1390,
            p99: 1600,
            min: 930,
            max: 1710,
            mean: 1125,
            stddev: 155,
          },
          snapshot_full: {
            p50: 82,
            p95: 140,
            p99: 180,
            min: 65,
            max: 190,
            mean: 95,
            stddev: 28,
          },
          snapshot_interactive: {
            p50: 34,
            p95: 56,
            p99: 70,
            min: 26,
            max: 76,
            mean: 40,
            stddev: 11,
          },
          screenshot_plain: {
            p50: 110,
            p95: 160,
            p99: 220,
            min: 90,
            max: 235,
            mean: 128,
            stddev: 32,
          },
          screenshot_annotated: {
            p50: 170,
            p95: 255,
            p99: 310,
            min: 145,
            max: 330,
            mean: 195,
            stddev: 44,
          },
          eval_simple: {
            p50: 9,
            p95: 15,
            p99: 20,
            min: 6,
            max: 22,
            mean: 10,
            stddev: 3,
          },
          eval_complex: {
            p50: 40,
            p95: 63,
            p99: 82,
            min: 31,
            max: 85,
            mean: 47,
            stddev: 14,
          },
          click_wait_snapshot: {
            p50: 1170,
            p95: 1510,
            p99: 1820,
            min: 980,
            max: 1900,
            mean: 1260,
            stddev: 180,
          },
          navigate_10x: {
            p50: 9220,
            p95: 10650,
            p99: 11750,
            min: 8450,
            max: 12000,
            mean: 9700,
            stddev: 860,
          },
          state_save_load: {
            p50: 54,
            p95: 82,
            p99: 99,
            min: 43,
            max: 106,
            mean: 61,
            stddev: 14,
          },
        },
      },
    ],
    winners: {
      cold_start: "zun",
      navigate_complex: "zun",
      snapshot_full: "zun",
      snapshot_interactive: "zun",
      screenshot_plain: "zun",
      screenshot_annotated: "zun",
      eval_simple: "zun",
      eval_complex: "zun",
      click_wait_snapshot: "zun",
      navigate_10x: "zun",
      state_save_load: "zun",
    },
    ranking: [
      {
        rank: 1,
        driverName: "zun",
        displayName: "zun (CEF)",
        score: geometricMean([180, 850, 45, 20, 65, 120, 3, 25, 890, 7500, 30]),
        scoreLabel: "Geometric mean of p50s (ms)",
      },
      {
        rank: 2,
        driverName: "stagehand",
        displayName: "Stagehand",
        score: geometricMean([410, 1040, 82, 34, 110, 170, 9, 40, 1170, 9220, 54]),
        scoreLabel: "Geometric mean of p50s (ms)",
      },
    ],
  },
  accuracy: MOCK_ACCURACY,
};
