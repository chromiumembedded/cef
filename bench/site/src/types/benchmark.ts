/**
 * Core data types for the agent-browser-bench benchmark site.
 *
 * Two layers:
 *   Layer 1 (Speed): Raw infrastructure latency — deterministic ops, no AI
 *   Layer 2 (Accuracy): Online-Mind2Web subset — real tasks with LLM
 */

// ============================================================
// Layer 1: Infrastructure Speed Benchmark
// ============================================================

export interface SpeedResult {
  /** Task identifier (e.g., "cold_start", "navigate_complex") */
  taskId: string;
  /** Human-readable task name */
  taskName: string;
  /** Task category */
  category: TaskCategory;
  /** Raw timing measurements in milliseconds */
  timings: number[];
  /** Whether the task succeeded on all runs */
  allSucceeded: boolean;
  /** Number of failures out of total runs */
  failures: number;
  /** Error message from first failure, if any */
  firstError?: string;
}

export type TaskCategory =
  | "startup"
  | "navigation"
  | "snapshot"
  | "screenshot"
  | "eval"
  | "compound"
  | "throughput"
  | "state";

export interface PercentileStats {
  p50: number;
  p95: number;
  p99: number;
  min: number;
  max: number;
  mean: number;
  stddev: number;
}

export interface DriverSpeedReport {
  /** Driver short name (e.g., "zun", "agent-browser") */
  driverName: string;
  /** Driver display name */
  displayName: string;
  /** Framework version string */
  version: string;
  /** Per-task results */
  results: SpeedResult[];
  /** Computed stats per task */
  stats: Record<string, PercentileStats>;
  /** Setup/teardown time (ms) */
  setupTime: number;
  teardownTime: number;
}

export interface SpeedBenchmarkRun {
  /** Unique run identifier */
  runId: string;
  /** ISO 8601 timestamp */
  timestamp: string;
  /** Hardware description */
  hardware: HardwareInfo;
  /** All driver reports */
  drivers: DriverSpeedReport[];
  /** Per-task winners (driver name with lowest p50) */
  winners: Record<string, string>;
  /** Overall ranking by geometric mean of p50s */
  ranking: RankEntry[];
}

// ============================================================
// Layer 2: Accuracy Benchmark (Online-Mind2Web subset)
// ============================================================

export type TaskDifficulty = "easy" | "medium" | "hard";

export interface AccuracyTask {
  /** Task ID from Online-Mind2Web */
  taskId: string;
  /** Natural language instruction */
  instruction: string;
  /** Target website */
  website: string;
  /** Difficulty tier */
  difficulty: TaskDifficulty;
  /** Task category (e.g., "shopping", "travel", "finance") */
  domain: string;
}

export interface AccuracyResult {
  /** Reference to task */
  taskId: string;
  /** Whether the agent completed the task successfully */
  success: boolean;
  /** Number of steps taken */
  steps: number;
  /** Total wall-clock time (ms) */
  totalTimeMs: number;
  /** Time spent on LLM inference (ms) */
  llmTimeMs: number;
  /** Time spent on browser operations (ms) */
  browserTimeMs: number;
  /** Number of LLM calls made */
  llmCalls: number;
  /** Token usage */
  tokensUsed: number;
  /** Error message if failed */
  error?: string;
  /** Judge verdict (from agentic judge) */
  judgeVerdict?: "pass" | "fail" | "partial";
  /** Judge confidence (0-1) */
  judgeConfidence?: number;
}

export interface DriverAccuracyReport {
  driverName: string;
  displayName: string;
  version: string;
  /** LLM used for agent decisions */
  llmModel: string;
  /** Per-task results */
  results: AccuracyResult[];
  /** Aggregate accuracy */
  overallAccuracy: number;
  /** Per-difficulty accuracy */
  accuracyByDifficulty: Record<TaskDifficulty, number>;
  /** Per-domain accuracy */
  accuracyByDomain: Record<string, number>;
  /** Average steps per successful task */
  avgSteps: number;
  /** Average time per task (ms) */
  avgTimeMs: number;
  /** Average tokens per task */
  avgTokens: number;
}

export interface AccuracyBenchmarkRun {
  runId: string;
  timestamp: string;
  hardware: HardwareInfo;
  /** LLM model used across all drivers */
  llmModel: string;
  /** Task set used */
  tasks: AccuracyTask[];
  /** All driver reports */
  drivers: DriverAccuracyReport[];
  /** Overall ranking by accuracy */
  ranking: RankEntry[];
}

// ============================================================
// Shared Types
// ============================================================

export interface HardwareInfo {
  /** CPU description */
  cpu: string;
  /** Number of cores */
  cores: number;
  /** RAM in GB */
  ramGb: number;
  /** OS description */
  os: string;
  /** Disk type and size */
  disk: string;
  /** Network description (for accuracy tests) */
  network?: string;
}

export interface RankEntry {
  rank: number;
  driverName: string;
  displayName: string;
  /** Primary score (lower is better for speed, higher for accuracy) */
  score: number;
  /** How the score was computed */
  scoreLabel: string;
}

// ============================================================
// Combined Benchmark Report (what the site renders)
// ============================================================

export interface BenchmarkReport {
  /** Report version for schema evolution */
  schemaVersion: "1.0.0";
  /** Report title */
  title: string;
  /** Report description */
  description: string;
  /** When the report was generated */
  generatedAt: string;
  /** Source repository URL */
  repoUrl: string;

  /** Layer 1: Infrastructure speed results */
  speed?: SpeedBenchmarkRun;
  /** Layer 2: Accuracy results */
  accuracy?: AccuracyBenchmarkRun;

  /** Contestants metadata */
  contestants: ContestantInfo[];

  /** Published leaderboard scores for context */
  externalScores?: ExternalScore[];
}

export interface ContestantInfo {
  name: string;
  displayName: string;
  /** Short description */
  description: string;
  /** Project URL */
  url: string;
  /** Language/runtime */
  language: string;
  /** License */
  license: string;
  /** Whether this is us */
  isOurs: boolean;
}

/** Published scores from external benchmarks for comparison */
export interface ExternalScore {
  /** Benchmark name (e.g., "Online-Mind2Web", "WebVoyager") */
  benchmark: string;
  /** Agent/framework name */
  agent: string;
  /** Score (percentage) */
  score: number;
  /** Source URL */
  sourceUrl: string;
  /** Date of the result */
  date: string;
}
