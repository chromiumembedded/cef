// Copyright (c) 2024 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "tests/cefclient/browser/task_manager_test.h"

#include "include/cef_task_manager.h"

namespace {

constexpr auto kTestUrlPath = "/task_manager";

struct TaskInfo {
  int64_t task_id;
  CefString title;
  cef_task_type_t type;
  bool is_killable;
};

std::string TaskTypeToString(cef_task_type_t type) {
  switch (type) {
    case CEF_TASK_TYPE_UNKNOWN:
      return "Unknown";
    case CEF_TASK_TYPE_BROWSER:
      return "Browser";
    case CEF_TASK_TYPE_GPU:
      return "GPU";
    case CEF_TASK_TYPE_ZYGOTE:
      return "Zygote";
    case CEF_TASK_TYPE_UTILITY:
      return "Utility";
    case CEF_TASK_TYPE_RENDERER:
      return "Renderer";
    case CEF_TASK_TYPE_EXTENSION:
      return "Extension";
    case CEF_TASK_TYPE_GUEST:
      return "Guest";
    case CEF_TASK_TYPE_PLUGIN_DEPRECATED:
      return "Plugin (Deprecated)";
    case CEF_TASK_TYPE_SANDBOX_HELPER:
      return "Sandbox Helper";
    case CEF_TASK_TYPE_DEDICATED_WORKER:
      return "Dedicated Worker";
    case CEF_TASK_TYPE_SHARED_WORKER:
      return "Shared Worker";
    case CEF_TASK_TYPE_SERVICE_WORKER:
      return "Service Worker";
    case CEF_TASK_TYPE_NUM_VALUES:
      NOTREACHED();
      break;
  }
  return "Unknown";
}

std::string TasksToJsonString(const std::vector<CefTaskInfo>& tasks,
                              int64_t browser_task_id) {
  std::string json = "[";
  for (size_t i = 0; i < tasks.size(); ++i) {
    const auto& task = tasks[i];
    if (i > 0) {
      json += ",";
    }
    json += "{";
    json += "\"id\":" + std::to_string(task.id) + ",";
    json += "\"type\":\"" + TaskTypeToString(task.type) + "\",";
    json += "\"is_killable\":";
    json += (task.is_killable ? "true," : "false,");
    json += "\"title\":\"" + CefString(&task.title).ToString() + "\",";
    json += "\"cpu_usage\":" + std::to_string(task.cpu_usage) + ",";
    json += "\"number_of_processors\":" +
            std::to_string(task.number_of_processors) + ",";
    json += "\"memory\":" + std::to_string(task.memory) + ",";
    json += "\"gpu_memory\":" + std::to_string(task.gpu_memory) + ",";
    json += "\"is_gpu_memory_inflated\":";
    json += std::string(task.is_gpu_memory_inflated ? "true" : "false") + ",";
    json += "\"is_this_browser\":";
    json += (task.id == browser_task_id ? "true" : "false");
    json += "}";
  }
  json += "]";
  return json;
}

// Handle messages in the browser process.
class Handler final : public CefMessageRouterBrowserSide::Handler {
 public:
  explicit Handler() : task_manager_(CefTaskManager::GetTaskManager()) {}
  // Called due to cefQuery execution in binary_transfer.html.
  bool OnQuery(CefRefPtr<CefBrowser> browser,
               CefRefPtr<CefFrame> frame,
               int64_t query_id,
               const CefString& request,
               bool persistent,
               CefRefPtr<Callback> callback) override {
    // Only handle messages from the test URL.
    const std::string& url = frame->GetURL();
    if (!client::test_runner::IsTestURL(url, kTestUrlPath)) {
      return false;
    }

    if (request == "get_tasks") {
      CefTaskManager::TaskIdList task_ids;
      task_manager_->GetTaskIdsList(task_ids);

      std::vector<CefTaskInfo> tasks;
      for (auto task_id : task_ids) {
        CefTaskInfo info;
        task_manager_->GetTaskInfo(task_id, info);
        tasks.push_back(info);
      }

      int64_t browser_task_id =
          task_manager_->GetTaskIdForBrowserId(browser->GetIdentifier());

      callback->Success(TasksToJsonString(tasks, browser_task_id));
      return true;
    }

    // Otherwise assume that task id was passed as string to kill it
    int64_t task_id = std::stoll(request.ToString());
    task_manager_->KillTask(task_id);
    callback->Success("");
    return true;
  }

 private:
  CefRefPtr<CefTaskManager> task_manager_;
};

}  // namespace

namespace client::task_manager_test {

void CreateMessageHandlers(test_runner::MessageHandlerSet& handlers) {
  handlers.insert(new Handler());
}

}  // namespace client::task_manager_test
