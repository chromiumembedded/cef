// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_AGENT_SCHEDULER_H_
#define CEF_LIBCEF_BROWSER_AGENT_SCHEDULER_H_
#pragma once

#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "base/values.h"

// Internal agent scheduler for managing concurrent agent workloads.
//
// Provides priority-based task scheduling, concurrency limits, and
// workload balancing across agent sessions. Tasks are dispatched to
// the browser's thread pool with controlled parallelism.
//
// This is internal infrastructure — agents interact through CDP as before.
// The scheduler manages the internal execution of their requests.
class CefAgentScheduler {
 public:
  // Priority levels for agent tasks.
  enum class Priority {
    kLow = 0,       // Background operations (prefetching, cleanup)
    kNormal = 1,    // Standard agent operations
    kHigh = 2,      // User-initiated operations
    kCritical = 3,  // Time-sensitive operations (auth flows)
  };

  // Represents a scheduled task.
  struct Task {
    std::string id;
    std::string agent_id;
    Priority priority = Priority::kNormal;
    base::OnceClosure work;
    base::OnceClosure on_complete;
    base::TimeTicks queued_at;
    base::TimeTicks started_at;
    base::TimeDelta timeout;
  };

  // Scheduler configuration.
  struct Config {
    // Maximum number of concurrent tasks across all agents.
    size_t max_concurrent_tasks = 8;

    // Maximum number of concurrent tasks per agent.
    size_t max_per_agent_tasks = 4;

    // Default task timeout.
    base::TimeDelta default_timeout = base::Seconds(60);
  };

  explicit CefAgentScheduler(const Config& config = Config());
  ~CefAgentScheduler();

  CefAgentScheduler(const CefAgentScheduler&) = delete;
  CefAgentScheduler& operator=(const CefAgentScheduler&) = delete;

  // Schedule a task for execution.
  // Returns a task ID that can be used to cancel or query status.
  std::string ScheduleTask(
      const std::string& agent_id,
      Priority priority,
      base::OnceClosure work,
      base::OnceClosure on_complete = base::OnceClosure());

  // Cancel a scheduled (not yet running) task.
  bool CancelTask(const std::string& task_id);

  // Get the number of queued tasks.
  size_t GetQueuedTaskCount() const;

  // Get the number of currently running tasks.
  size_t GetRunningTaskCount() const;

  // Get the number of running tasks for a specific agent.
  size_t GetAgentTaskCount(const std::string& agent_id) const;

  // Pause/resume scheduling (running tasks continue).
  void Pause();
  void Resume();
  bool IsPaused() const;

  // Update configuration.
  void SetConfig(const Config& config);

  // ===== Evaluation Loop Support =====

  // Configuration for an evaluation loop.
  struct EvalConfig {
    // Number of iterations to run.
    size_t iterations = 1;

    // Agent ID to use for the evaluation tasks.
    std::string agent_id;

    // Tags for train/validation split. Tasks are tagged with one of these.
    std::vector<std::string> tags;

    // Maximum concurrent evaluation tasks per iteration.
    size_t max_concurrent = 4;

    // Timeout per evaluation task.
    base::TimeDelta task_timeout = base::Seconds(60);
  };

  // Result of a single evaluation task.
  struct EvalResult {
    std::string task_id;
    std::string tag;  // e.g., "train" or "validation"
    bool success = false;
    std::string error;
    base::TimeDelta duration;
    base::Value::Dict metadata;  // Arbitrary result data
  };

  // Summary of one evaluation iteration.
  struct IterationSummary {
    size_t iteration = 0;
    size_t total_tasks = 0;
    size_t succeeded = 0;
    size_t failed = 0;
    double success_rate = 0.0;
    base::TimeDelta total_duration;

    // Per-tag breakdown.
    struct TagSummary {
      size_t total = 0;
      size_t succeeded = 0;
      double success_rate = 0.0;
    };
    std::map<std::string, TagSummary> by_tag;

    // All individual results for this iteration.
    std::vector<EvalResult> results;
  };

  // Callback for when an evaluation loop completes.
  using EvalLoopCallback =
      base::OnceCallback<void(std::vector<IterationSummary> summaries)>;

  // Callback for per-iteration progress.
  using IterationCallback =
      base::RepeatingCallback<void(const IterationSummary& summary)>;

  // Submit evaluation tasks. Each task is a closure that produces an EvalResult.
  // |tasks| is a vector of (tag, work) pairs.
  // |config| controls iteration count and concurrency.
  // |on_iteration| is called after each iteration completes (optional).
  // |on_complete| is called when all iterations are done.
  void RunEvaluationLoop(
      std::vector<
          std::pair<std::string, base::OnceCallback<EvalResult()>>> tasks,
      const EvalConfig& config,
      IterationCallback on_iteration,
      EvalLoopCallback on_complete);

  // Get the history of completed evaluation loops.
  const std::vector<std::vector<IterationSummary>>& GetEvalHistory() const;

  // Clear evaluation history.
  void ClearEvalHistory();

  // Store a completed evaluation loop's results.
  // Internal use only — called by the eval loop runner on completion.
  void StoreEvalResults(std::vector<IterationSummary> summaries);

 private:
  // Priority comparator for the task queue.
  struct TaskComparator {
    bool operator()(const std::unique_ptr<Task>& a,
                    const std::unique_ptr<Task>& b) const;
  };

  // Try to dispatch tasks from the queue.
  void DispatchPending();

  // Called when a task completes.
  void OnTaskComplete(const std::string& task_id,
                      const std::string& agent_id);

  // Generate a unique task ID.
  std::string GenerateTaskId();

  mutable base::Lock lock_;
  Config config_;
  bool paused_ = false;
  uint64_t next_task_id_ = 1;

  // Priority queue of pending tasks.
  std::priority_queue<std::unique_ptr<Task>,
                      std::vector<std::unique_ptr<Task>>,
                      TaskComparator>
      pending_tasks_;

  // Currently running tasks by ID.
  std::map<std::string, std::string> running_tasks_;  // task_id -> agent_id

  // Count of running tasks per agent.
  std::map<std::string, size_t> agent_task_counts_;

  // Evaluation loop state.
  std::vector<std::vector<IterationSummary>> eval_history_;
};

#endif  // CEF_LIBCEF_BROWSER_AGENT_SCHEDULER_H_
