// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
#define CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_task_manager.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/task_manager/task_manager_observer.h"

namespace task_manager {
class TaskManagerInterface;
}

class CefTaskManagerImpl : public task_manager::TaskManagerObserver,
                           public CefTaskManager {
 public:
  explicit CefTaskManagerImpl(task_manager::TaskManagerInterface* task_manager);
  ~CefTaskManagerImpl();

  CefTaskManagerImpl(const CefTaskManagerImpl&) = delete;
  CefTaskManagerImpl& operator=(const CefTaskManagerImpl&) = delete;

  // CefTaskManager methods:
  size_t GetTasksCount() override;
  bool GetTaskIdsList(TaskIdList& task_ids) override;
  bool GetTaskInfo(int64_t task_id, CefTaskInfo& info) override;
  bool KillTask(int64_t task_id) override;
  int64_t GetTaskIdForBrowserId(int browser_id) override;

 private:
  bool IsValidTaskId(int64_t task_id) const;

  // task_manager::TaskManagerObserver:
  void OnTaskAdded(int64_t id) override;
  void OnTaskToBeRemoved(int64_t id) override;
  void OnTasksRefreshed(const task_manager::TaskIdList& task_ids) override;

  raw_ptr<task_manager::TaskManagerInterface> task_manager_;
  TaskIdList tasks_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefTaskManagerImpl);
};

#endif  // CEF_LIBCEF_BROWSER_TASK_MANAGER_IMPL_H_
