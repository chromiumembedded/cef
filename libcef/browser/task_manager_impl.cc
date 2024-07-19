// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "cef/libcef/browser/task_manager_impl.h"

#include "base/check.h"
#include "base/ranges/algorithm.h"
#include "base/system/sys_info.h"
#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "chrome/browser/task_manager/task_manager_interface.h"

namespace {
#if BUILDFLAG(IS_MAC)
// Match Activity Monitor's default refresh rate.
const int64_t kRefreshTimeMS = 2000;
#else
const int64_t kRefreshTimeMS = 1000;
#endif  // BUILDFLAG(IS_MAC)

CefTaskManager::TaskType toCefTaskType(task_manager::Task::Type type) {
  switch (type) {
    case task_manager::Task::UNKNOWN:
    case task_manager::Task::ARC:
    case task_manager::Task::CROSTINI:
    case task_manager::Task::PLUGIN_VM:
    case task_manager::Task::NACL:
    case task_manager::Task::LACROS:
      return CEF_TASK_TYPE_UNKNOWN;
    case task_manager::Task::BROWSER:
      return CEF_TASK_TYPE_BROWSER;
    case task_manager::Task::GPU:
      return CEF_TASK_TYPE_GPU;
    case task_manager::Task::ZYGOTE:
      return CEF_TASK_TYPE_ZYGOTE;
    case task_manager::Task::UTILITY:
      return CEF_TASK_TYPE_UTILITY;
    case task_manager::Task::RENDERER:
      return CEF_TASK_TYPE_RENDERER;
    case task_manager::Task::EXTENSION:
      return CEF_TASK_TYPE_EXTENSION;
    case task_manager::Task::GUEST:
      return CEF_TASK_TYPE_GUEST;
    case task_manager::Task::PLUGIN:
      return CEF_TASK_TYPE_PLUGIN;
    case task_manager::Task::SANDBOX_HELPER:
      return CEF_TASK_TYPE_SANDBOX_HELPER;
    case task_manager::Task::DEDICATED_WORKER:
      return CEF_TASK_TYPE_DEDICATED_WORKER;
    case task_manager::Task::SHARED_WORKER:
      return CEF_TASK_TYPE_SHARED_WORKER;
    case task_manager::Task::SERVICE_WORKER:
      return CEF_TASK_TYPE_SERVICE_WORKER;
  }

  return CEF_TASK_TYPE_UNKNOWN;
}

int64_t GetRefreshTypes() {
  return task_manager::REFRESH_TYPE_CPU |
         task_manager::REFRESH_TYPE_GPU_MEMORY |
         task_manager::REFRESH_TYPE_MEMORY_FOOTPRINT;
}

}  // namespace

CefTaskManagerImpl::CefTaskManagerImpl(
    task_manager::TaskManagerInterface* task_manager)
    : TaskManagerObserver(base::Milliseconds(kRefreshTimeMS),
                          GetRefreshTypes()),
      task_manager_(task_manager) {
  DCHECK(task_manager_);
  task_manager_->AddObserver(this);
  tasks_ = task_manager->GetTaskIdsList();
}

CefTaskManagerImpl::~CefTaskManagerImpl() {
  task_manager_->RemoveObserver(this);
}

void CefTaskManagerImpl::OnTaskAdded(int64_t id) {
  tasks_ = observed_task_manager()->GetTaskIdsList();
}

void CefTaskManagerImpl::OnTaskToBeRemoved(int64_t id) {
  auto index = base::ranges::find(tasks_, id);
  if (index != tasks_.end()) {
    tasks_.erase(index);
  }
}

void CefTaskManagerImpl::OnTasksRefreshed(
    const task_manager::TaskIdList& task_ids) {
  tasks_ = task_ids;
}

size_t CefTaskManagerImpl::GetTasksCount() {
  CEF_REQUIRE_UIT_RETURN(0U);

  return tasks_.size();
}

bool CefTaskManagerImpl::GetTaskIdsList(TaskIdList& task_ids) {
  CEF_REQUIRE_UIT_RETURN(false);

  task_ids = tasks_;
  return true;
}

bool CefTaskManagerImpl::GetTaskInfo(int64_t task_id, CefTaskInfo& info) {
  CEF_REQUIRE_UIT_RETURN(false);

  if (!IsValidTaskId(task_id)) {
    return false;
  }

  info.id = task_id;
  info.type = toCefTaskType(task_manager_->GetType(task_id));
  info.is_killable = task_manager_->IsTaskKillable(task_id);
  CefString(&info.title) = task_manager_->GetTitle(task_id);
  info.cpu_usage = task_manager_->GetPlatformIndependentCPUUsage(task_id);
  // Avoid returning quiet_NaN for CPU usage.
  if (info.cpu_usage != info.cpu_usage) {
    info.cpu_usage = 0.0;
  }
  info.number_of_processors = base::SysInfo::NumberOfProcessors();
  info.memory = task_manager_->GetMemoryFootprintUsage(task_id);

  bool has_duplicates = false;
  info.gpu_memory = task_manager_->GetGpuMemoryUsage(task_id, &has_duplicates);
  info.is_gpu_memory_inflated = has_duplicates;
  return true;
}

bool CefTaskManagerImpl::KillTask(int64_t task_id) {
  CEF_REQUIRE_UIT_RETURN(false);

  if (!IsValidTaskId(task_id)) {
    return false;
  }

  if (!task_manager_->IsTaskKillable(task_id)) {
    return false;
  }

  task_manager_->KillTask(task_id);
  return true;
}

int64_t CefTaskManagerImpl::GetTaskIdForBrowserId(int browser_id) {
  CEF_REQUIRE_UIT_RETURN(-1);

  auto browser = CefBrowserHostBase::GetBrowserForBrowserId(browser_id);
  if (!browser) {
    return -1;
  }

  auto* web_contents = browser->GetWebContents();
  if (!web_contents) {
    return -1;
  }

  return task_manager_->GetTaskIdForWebContents(web_contents);
}

bool CefTaskManagerImpl::IsValidTaskId(int64_t task_id) const {
  return base::ranges::find(tasks_, task_id) != tasks_.end();
}

CefRefPtr<CefTaskManager> CefTaskManager::GetTaskManager() {
  // Verify that the context is in a valid state.
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return nullptr;
  }

  CEF_REQUIRE_UIT_RETURN(nullptr);

  return new CefTaskManagerImpl(
      task_manager::TaskManagerInterface::GetTaskManager());
}
