// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/agent_scheduler.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"

namespace {

// Shared task factory that can be cloned across iterations.
// Since base::OnceCallback can only run once, we store the original task
// factories as shared pointers to std::function which are copyable.
using EvalTaskFactory =
    std::shared_ptr<std::function<CefAgentScheduler::EvalResult()>>;

// Manages the state machine for a multi-iteration evaluation loop.
// Ref-counted because it is shared across thread pool callbacks.
class EvalLoopRunner : public base::RefCountedThreadSafe<EvalLoopRunner> {
 public:
  EvalLoopRunner(
      std::vector<std::pair<std::string, EvalTaskFactory>> task_factories,
      CefAgentScheduler::EvalConfig config,
      CefAgentScheduler::IterationCallback on_iteration,
      CefAgentScheduler::EvalLoopCallback on_complete,
      CefAgentScheduler* scheduler,
      scoped_refptr<base::SequencedTaskRunner> origin_runner)
      : task_factories_(std::move(task_factories)),
        config_(std::move(config)),
        on_iteration_(std::move(on_iteration)),
        on_complete_(std::move(on_complete)),
        scheduler_(scheduler),
        origin_runner_(std::move(origin_runner)) {}

  void Start() { RunNextIteration(); }

 private:
  friend class base::RefCountedThreadSafe<EvalLoopRunner>;
  ~EvalLoopRunner() = default;

  void RunNextIteration() {
    if (current_iteration_ >= config_.iterations) {
      // All iterations complete.
      FinishLoop();
      return;
    }

    // Reset per-iteration state.
    current_results_.clear();
    pending_count_ = task_factories_.size();
    active_count_ = 0;
    next_task_index_ = 0;
    iteration_start_ = base::TimeTicks::Now();

    // Kick off the first batch of tasks up to max_concurrent.
    DispatchNextBatch();
  }

  void DispatchNextBatch() {
    while (next_task_index_ < task_factories_.size() &&
           active_count_ < config_.max_concurrent) {
      DispatchTask(next_task_index_);
      next_task_index_++;
      active_count_++;
    }
  }

  void DispatchTask(size_t index) {
    const auto& tag = task_factories_[index].first;
    const auto& factory = task_factories_[index].second;

    // Generate a task ID for this evaluation task.
    std::string task_id = base::StringPrintf(
        "eval_iter%zu_task%zu", current_iteration_, index);

    // Capture what we need for the thread pool task.
    auto factory_copy = factory;
    std::string tag_copy = tag;
    std::string task_id_copy = task_id;

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE,
        {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        base::BindOnce(
            [](EvalTaskFactory factory_fn, std::string id,
               std::string tag_val) -> CefAgentScheduler::EvalResult {
              base::TimeTicks start = base::TimeTicks::Now();
              CefAgentScheduler::EvalResult result = (*factory_fn)();
              base::TimeTicks end = base::TimeTicks::Now();
              result.task_id = std::move(id);
              result.tag = std::move(tag_val);
              result.duration = end - start;
              return result;
            },
            std::move(factory_copy), std::move(task_id_copy),
            std::move(tag_copy)),
        base::BindOnce(&EvalLoopRunner::OnTaskComplete, this));
  }

  void OnTaskComplete(CefAgentScheduler::EvalResult result) {
    current_results_.push_back(std::move(result));
    active_count_--;
    pending_count_--;

    if (pending_count_ > 0) {
      // More tasks to dispatch or still running.
      DispatchNextBatch();
      return;
    }

    // All tasks for this iteration are done.
    OnIterationComplete();
  }

  void OnIterationComplete() {
    CefAgentScheduler::IterationSummary summary;
    summary.iteration = current_iteration_;
    summary.total_tasks = current_results_.size();
    summary.total_duration = base::TimeTicks::Now() - iteration_start_;

    for (auto& result : current_results_) {
      if (result.success) {
        summary.succeeded++;
      } else {
        summary.failed++;
      }

      // Update per-tag breakdown.
      auto& tag_summary = summary.by_tag[result.tag];
      tag_summary.total++;
      if (result.success) {
        tag_summary.succeeded++;
      }
    }

    // Compute success rates.
    if (summary.total_tasks > 0) {
      summary.success_rate =
          static_cast<double>(summary.succeeded) / summary.total_tasks;
    }
    for (auto& [tag, ts] : summary.by_tag) {
      if (ts.total > 0) {
        ts.success_rate = static_cast<double>(ts.succeeded) / ts.total;
      }
    }

    summary.results = std::move(current_results_);
    all_summaries_.push_back(std::move(summary));

    // Fire per-iteration callback.
    if (on_iteration_) {
      on_iteration_.Run(all_summaries_.back());
    }

    current_iteration_++;
    RunNextIteration();
  }

  void FinishLoop() {
    // Store results in scheduler's history via public helper.
    scheduler_->StoreEvalResults(all_summaries_);

    if (on_complete_) {
      std::move(on_complete_).Run(std::move(all_summaries_));
    }
  }

  std::vector<std::pair<std::string, EvalTaskFactory>> task_factories_;
  CefAgentScheduler::EvalConfig config_;
  CefAgentScheduler::IterationCallback on_iteration_;
  CefAgentScheduler::EvalLoopCallback on_complete_;
  CefAgentScheduler* scheduler_;  // Outlives the loop.
  scoped_refptr<base::SequencedTaskRunner> origin_runner_;

  size_t current_iteration_ = 0;
  size_t pending_count_ = 0;
  size_t active_count_ = 0;
  size_t next_task_index_ = 0;
  base::TimeTicks iteration_start_;
  std::vector<CefAgentScheduler::EvalResult> current_results_;
  std::vector<CefAgentScheduler::IterationSummary> all_summaries_;
};

}  // namespace

CefAgentScheduler::CefAgentScheduler(const Config& config) : config_(config) {}

CefAgentScheduler::~CefAgentScheduler() = default;

std::string CefAgentScheduler::ScheduleTask(const std::string& agent_id,
                                             Priority priority,
                                             base::OnceClosure work,
                                             base::OnceClosure on_complete) {
  base::AutoLock lock(lock_);

  auto task = std::make_unique<Task>();
  task->id = GenerateTaskId();
  task->agent_id = agent_id;
  task->priority = priority;
  task->work = std::move(work);
  task->on_complete = std::move(on_complete);
  task->queued_at = base::TimeTicks::Now();
  task->timeout = config_.default_timeout;

  std::string task_id = task->id;
  pending_tasks_.push(std::move(task));

  DispatchPending();

  return task_id;
}

bool CefAgentScheduler::CancelTask(const std::string& task_id) {
  base::AutoLock lock(lock_);

  // We can only cancel tasks that are still in the pending queue.
  // Since std::priority_queue doesn't support removal, we rebuild the queue
  // without the cancelled task.
  std::vector<std::unique_ptr<Task>> remaining;
  bool found = false;

  while (!pending_tasks_.empty()) {
    // priority_queue only provides const access to top(), but we need to
    // move elements out. Use const_cast since we're clearing the queue anyway.
    auto& top_ref =
        const_cast<std::unique_ptr<Task>&>(pending_tasks_.top());
    if (top_ref->id == task_id) {
      found = true;
      // Discard this task.
      pending_tasks_.pop();
    } else {
      remaining.push_back(std::move(const_cast<std::unique_ptr<Task>&>(
          pending_tasks_.top())));
      pending_tasks_.pop();
    }
  }

  // Rebuild the queue with remaining tasks.
  for (auto& task : remaining) {
    pending_tasks_.push(std::move(task));
  }

  return found;
}

size_t CefAgentScheduler::GetQueuedTaskCount() const {
  base::AutoLock lock(lock_);
  return pending_tasks_.size();
}

size_t CefAgentScheduler::GetRunningTaskCount() const {
  base::AutoLock lock(lock_);
  return running_tasks_.size();
}

size_t CefAgentScheduler::GetAgentTaskCount(
    const std::string& agent_id) const {
  base::AutoLock lock(lock_);
  auto it = agent_task_counts_.find(agent_id);
  if (it != agent_task_counts_.end()) {
    return it->second;
  }
  return 0;
}

void CefAgentScheduler::Pause() {
  base::AutoLock lock(lock_);
  paused_ = true;
}

void CefAgentScheduler::Resume() {
  base::AutoLock lock(lock_);
  paused_ = false;
  DispatchPending();
}

bool CefAgentScheduler::IsPaused() const {
  base::AutoLock lock(lock_);
  return paused_;
}

void CefAgentScheduler::SetConfig(const Config& config) {
  base::AutoLock lock(lock_);
  config_ = config;
  DispatchPending();
}

bool CefAgentScheduler::TaskComparator::operator()(
    const std::unique_ptr<Task>& a,
    const std::unique_ptr<Task>& b) const {
  // std::priority_queue is a max-heap, so return true if a has LOWER priority
  // than b (meaning b should come first).
  if (a->priority != b->priority) {
    return static_cast<int>(a->priority) < static_cast<int>(b->priority);
  }
  // Same priority: earlier queued_at should come first (lower value = higher
  // priority), so return true if a was queued AFTER b.
  return a->queued_at > b->queued_at;
}

void CefAgentScheduler::DispatchPending() {
  // Must be called with lock_ held.
  lock_.AssertAcquired();

  if (paused_) {
    return;
  }

  while (!pending_tasks_.empty() &&
         running_tasks_.size() < config_.max_concurrent_tasks) {
    // Peek at the top task to check per-agent limits.
    auto& top_ref =
        const_cast<std::unique_ptr<Task>&>(pending_tasks_.top());

    const std::string& agent_id = top_ref->agent_id;
    auto agent_it = agent_task_counts_.find(agent_id);
    size_t agent_count =
        (agent_it != agent_task_counts_.end()) ? agent_it->second : 0;

    if (agent_count >= config_.max_per_agent_tasks) {
      // This agent is at its limit. We can't easily skip to the next task
      // in a priority queue, so stop dispatching. Tasks will resume when
      // a running task for this agent completes.
      break;
    }

    // Move the task out of the queue.
    auto task = std::move(
        const_cast<std::unique_ptr<Task>&>(pending_tasks_.top()));
    pending_tasks_.pop();

    task->started_at = base::TimeTicks::Now();

    // Track the running task.
    std::string task_id = task->id;
    std::string task_agent_id = task->agent_id;
    running_tasks_[task_id] = task_agent_id;
    agent_task_counts_[task_agent_id]++;

    // Extract the closures before posting.
    base::OnceClosure work = std::move(task->work);
    base::OnceClosure on_complete = std::move(task->on_complete);

    // Post the work to the thread pool. When it completes, call
    // OnTaskComplete on the current sequence.
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
        std::move(work),
        base::BindOnce(
            [](CefAgentScheduler* scheduler, std::string id,
               std::string agent, base::OnceClosure complete_cb) {
              if (complete_cb) {
                std::move(complete_cb).Run();
              }
              scheduler->OnTaskComplete(id, agent);
            },
            base::Unretained(this), task_id, task_agent_id,
            std::move(on_complete)));
  }
}

void CefAgentScheduler::OnTaskComplete(const std::string& task_id,
                                        const std::string& agent_id) {
  base::AutoLock lock(lock_);

  running_tasks_.erase(task_id);

  auto it = agent_task_counts_.find(agent_id);
  if (it != agent_task_counts_.end()) {
    if (it->second <= 1) {
      agent_task_counts_.erase(it);
    } else {
      it->second--;
    }
  }

  DispatchPending();
}

std::string CefAgentScheduler::GenerateTaskId() {
  // Must be called with lock_ held.
  lock_.AssertAcquired();
  return base::StringPrintf("task_%" PRIu64, next_task_id_++);
}

void CefAgentScheduler::RunEvaluationLoop(
    std::vector<
        std::pair<std::string, base::OnceCallback<EvalResult()>>> tasks,
    const EvalConfig& config,
    IterationCallback on_iteration,
    EvalLoopCallback on_complete) {
  DCHECK(!tasks.empty());
  DCHECK(config.iterations > 0);

  // Convert OnceCallbacks into shared std::function factories so they can
  // be invoked across multiple iterations. Each OnceCallback is wrapped in
  // a shared_ptr<std::function> that captures it via shared_ptr so the
  // underlying callback is called only once per iteration clone.
  //
  // Since we need to re-run each task every iteration, the caller's
  // callbacks must be safe to conceptually "clone". We wrap each in a
  // shared function object. Note: the original OnceCallback is consumed
  // on the first call. For subsequent iterations the factory is re-copied
  // from the shared storage, but each copy is only called once.
  //
  // For true multi-iteration support, we require the callbacks to be
  // repeating. We accept OnceCallback in the API for single-iteration
  // convenience and wrap them accordingly.
  std::vector<std::pair<std::string, EvalTaskFactory>> factories;
  factories.reserve(tasks.size());

  for (auto& [tag, callback] : tasks) {
    // Wrap the OnceCallback in a shared_ptr so copies of the std::function
    // all reference the same underlying callback. For multi-iteration
    // loops with iterations > 1, callers should ensure their callbacks
    // are safe to invoke repeatedly (e.g., by providing repeating work).
    auto shared_cb =
        std::make_shared<base::OnceCallback<EvalResult()>>(std::move(callback));
    auto factory = std::make_shared<std::function<EvalResult()>>(
        [shared_cb]() -> EvalResult {
          if (*shared_cb) {
            return std::move(*shared_cb).Run();
          }
          // Callback already consumed (multi-iteration without repeating
          // callback). Return a failure result.
          EvalResult result;
          result.success = false;
          result.error = "Task callback already consumed in prior iteration";
          return result;
        });
    factories.emplace_back(std::move(tag), std::move(factory));
  }

  auto runner = base::MakeRefCounted<EvalLoopRunner>(
      std::move(factories), config, std::move(on_iteration),
      std::move(on_complete), this,
      base::SequencedTaskRunner::GetCurrentDefault());

  runner->Start();
}

const std::vector<std::vector<CefAgentScheduler::IterationSummary>>&
CefAgentScheduler::GetEvalHistory() const {
  base::AutoLock lock(lock_);
  return eval_history_;
}

void CefAgentScheduler::ClearEvalHistory() {
  base::AutoLock lock(lock_);
  eval_history_.clear();
}

void CefAgentScheduler::StoreEvalResults(
    std::vector<IterationSummary> summaries) {
  base::AutoLock lock(lock_);
  eval_history_.push_back(std::move(summaries));
}
