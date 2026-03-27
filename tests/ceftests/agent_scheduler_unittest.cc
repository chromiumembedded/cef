// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/task_environment.h"
#include "cef/libcef/browser/agent_scheduler.h"
#include "tests/gtest/include/gtest/gtest.h"

#include <atomic>
#include <vector>

namespace {

class AgentSchedulerTest : public ::testing::Test {
 protected:
  AgentSchedulerTest()
      : task_environment_(
            base::test::TaskEnvironment::MainThreadType::DEFAULT,
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC) {}

  base::test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(AgentSchedulerTest, ScheduleAndComplete) {
  CefAgentScheduler scheduler;

  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);
  std::atomic<bool> ran{false};

  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kNormal,
      base::BindOnce([](std::atomic<bool>* flag) { flag->store(true); }, &ran),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &done));

  done.Wait();
  EXPECT_TRUE(ran.load());
}

TEST_F(AgentSchedulerTest, PriorityOrdering) {
  CefAgentScheduler::Config config;
  config.max_concurrent_tasks = 1;
  CefAgentScheduler scheduler(config);

  // Pause the scheduler so we can enqueue tasks before any run.
  scheduler.Pause();
  EXPECT_TRUE(scheduler.IsPaused());

  std::vector<int> execution_order;
  base::Lock order_lock;

  base::WaitableEvent low_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent high_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  // Schedule low priority first, then high priority.
  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kLow,
      base::BindOnce(
          [](std::vector<int>* order, base::Lock* lock) {
            base::AutoLock l(*lock);
            order->push_back(0);
          },
          &execution_order, &order_lock),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &low_done));

  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kHigh,
      base::BindOnce(
          [](std::vector<int>* order, base::Lock* lock) {
            base::AutoLock l(*lock);
            order->push_back(1);
          },
          &execution_order, &order_lock),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &high_done));

  EXPECT_EQ(2u, scheduler.GetQueuedTaskCount());

  // Resume and let tasks run.
  scheduler.Resume();
  EXPECT_FALSE(scheduler.IsPaused());

  low_done.Wait();
  high_done.Wait();

  // High priority (1) should have run before low priority (0).
  base::AutoLock l(order_lock);
  ASSERT_EQ(2u, execution_order.size());
  EXPECT_EQ(1, execution_order[0]);
  EXPECT_EQ(0, execution_order[1]);
}

TEST_F(AgentSchedulerTest, ConcurrencyLimit) {
  CefAgentScheduler::Config config;
  config.max_concurrent_tasks = 1;
  CefAgentScheduler scheduler(config);

  base::WaitableEvent task1_started(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task1_proceed(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task1_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task2_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::atomic<bool> task2_ran{false};

  // Task 1 blocks until we signal it to proceed.
  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](base::WaitableEvent* started, base::WaitableEvent* proceed) {
            started->Signal();
            proceed->Wait();
          },
          &task1_started, &task1_proceed),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task1_done));

  // Wait for task 1 to actually start running.
  task1_started.Wait();

  // With max_concurrent=1, task 1 occupies the only slot.
  EXPECT_EQ(1u, scheduler.GetRunningTaskCount());

  // Schedule task 2 while task 1 is running.
  scheduler.ScheduleTask(
      "agent_2", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](std::atomic<bool>* flag) { flag->store(true); }, &task2_ran),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task2_done));

  // Task 2 should be queued, not running yet (we only allow 1 concurrent).
  EXPECT_EQ(1u, scheduler.GetQueuedTaskCount());

  // Let task 1 finish.
  task1_proceed.Signal();
  task1_done.Wait();
  task2_done.Wait();

  EXPECT_TRUE(task2_ran.load());
}

TEST_F(AgentSchedulerTest, PerAgentLimit) {
  CefAgentScheduler::Config config;
  config.max_concurrent_tasks = 8;
  config.max_per_agent_tasks = 1;
  CefAgentScheduler scheduler(config);

  base::WaitableEvent task1_started(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task1_proceed(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task1_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task2_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  std::atomic<bool> task2_ran{false};

  // Both tasks for the same agent.
  scheduler.ScheduleTask(
      "same_agent", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](base::WaitableEvent* started, base::WaitableEvent* proceed) {
            started->Signal();
            proceed->Wait();
          },
          &task1_started, &task1_proceed),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task1_done));

  task1_started.Wait();
  EXPECT_EQ(1u, scheduler.GetAgentTaskCount("same_agent"));

  scheduler.ScheduleTask(
      "same_agent", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](std::atomic<bool>* flag) { flag->store(true); }, &task2_ran),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task2_done));

  // Task 2 should be queued because per-agent limit is 1.
  EXPECT_EQ(1u, scheduler.GetAgentTaskCount("same_agent"));
  EXPECT_GE(scheduler.GetQueuedTaskCount(), 1u);

  task1_proceed.Signal();
  task1_done.Wait();
  task2_done.Wait();

  EXPECT_TRUE(task2_ran.load());
}

TEST_F(AgentSchedulerTest, CancelPending) {
  CefAgentScheduler scheduler;
  scheduler.Pause();

  std::atomic<bool> ran{false};

  std::string task_id = scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](std::atomic<bool>* flag) { flag->store(true); }, &ran));

  EXPECT_EQ(1u, scheduler.GetQueuedTaskCount());
  EXPECT_TRUE(scheduler.CancelTask(task_id));
  EXPECT_EQ(0u, scheduler.GetQueuedTaskCount());

  scheduler.Resume();
  // Give a moment for any accidental dispatch.
  task_environment_.RunUntilIdle();

  EXPECT_FALSE(ran.load());
}

TEST_F(AgentSchedulerTest, PauseResume) {
  CefAgentScheduler scheduler;
  scheduler.Pause();

  std::atomic<bool> ran{false};
  base::WaitableEvent done(base::WaitableEvent::ResetPolicy::MANUAL,
                           base::WaitableEvent::InitialState::NOT_SIGNALED);

  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](std::atomic<bool>* flag) { flag->store(true); }, &ran),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &done));

  // While paused, the task should be queued but not running.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(ran.load());
  EXPECT_EQ(1u, scheduler.GetQueuedTaskCount());
  EXPECT_EQ(0u, scheduler.GetRunningTaskCount());

  // Resume and verify the task runs.
  scheduler.Resume();
  done.Wait();
  EXPECT_TRUE(ran.load());
}

TEST_F(AgentSchedulerTest, QueuedAndRunningCounts) {
  CefAgentScheduler::Config config;
  config.max_concurrent_tasks = 1;
  CefAgentScheduler scheduler(config);

  EXPECT_EQ(0u, scheduler.GetQueuedTaskCount());
  EXPECT_EQ(0u, scheduler.GetRunningTaskCount());

  base::WaitableEvent task_started(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task_proceed(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  base::WaitableEvent task_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);

  scheduler.ScheduleTask(
      "agent_1", CefAgentScheduler::Priority::kNormal,
      base::BindOnce(
          [](base::WaitableEvent* started, base::WaitableEvent* proceed) {
            started->Signal();
            proceed->Wait();
          },
          &task_started, &task_proceed),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task_done));

  task_started.Wait();
  EXPECT_EQ(1u, scheduler.GetRunningTaskCount());

  // Add a second task that will be queued (max_concurrent=1).
  base::WaitableEvent task2_done(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  scheduler.ScheduleTask(
      "agent_2", CefAgentScheduler::Priority::kNormal,
      base::DoNothing(),
      base::BindOnce(
          [](base::WaitableEvent* event) { event->Signal(); }, &task2_done));

  EXPECT_EQ(1u, scheduler.GetQueuedTaskCount());
  EXPECT_EQ(1u, scheduler.GetRunningTaskCount());

  task_proceed.Signal();
  task_done.Wait();
  task2_done.Wait();

  EXPECT_EQ(0u, scheduler.GetQueuedTaskCount());
  EXPECT_EQ(0u, scheduler.GetRunningTaskCount());
}
