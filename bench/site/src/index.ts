/**
 * agent-browser-bench site — data layer entry point.
 *
 * This module exports everything needed to render the benchmark site.
 * UI components will be added later — this is the data foundation.
 */

// Types
export type {
  BenchmarkReport,
  SpeedBenchmarkRun,
  AccuracyBenchmarkRun,
  DriverSpeedReport,
  DriverAccuracyReport,
  SpeedResult,
  AccuracyResult,
  AccuracyTask,
  PercentileStats,
  HardwareInfo,
  RankEntry,
  ContestantInfo,
  ExternalScore,
  TaskCategory,
  TaskDifficulty,
} from "./types/benchmark";

// Data
export { CONTESTANTS, EXTERNAL_SCORES } from "./data/contestants";
export { SPEED_TASKS, ACCURACY_TASKS } from "./data/tasks";
export type { SpeedTask } from "./data/tasks";
export { MOCK_REPORT } from "./data/mock-results";

// Utilities
export {
  computeStats,
  geometricMean,
  formatMs,
  formatPct,
  taskWinner,
  speedup,
  toMarkdownTable,
} from "./data/stats";
