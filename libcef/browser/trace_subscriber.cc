// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/trace_subscriber.h"
#include "include/cef_trace.h"
#include "libcef/browser/thread_util.h"

#include "base/files/file_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/tracing_controller.h"

namespace {

// Create the temporary file and then execute |callback| on the thread
// represented by |message_loop_proxy|. 
void CreateTemporaryFileOnFileThread(
    scoped_refptr<base::SequencedTaskRunner> message_loop_proxy,
    base::Callback<void(const base::FilePath&)> callback) {
  CEF_REQUIRE_FILET();
  base::FilePath file_path;
  if (!base::CreateTemporaryFile(&file_path))
    LOG(ERROR) << "Failed to create temporary file.";
  message_loop_proxy->PostTask(FROM_HERE, base::Bind(callback, file_path));
}

// Release the wrapped callback object after completion.
class CefCompletionCallbackWrapper : public CefCompletionCallback {
 public:
  explicit CefCompletionCallbackWrapper(
      CefRefPtr<CefCompletionCallback> callback)
      : callback_(callback) {
  }

  void OnComplete() override {
    if (callback_) {
      callback_->OnComplete();
      callback_ = nullptr;
    }
  }

 private:
  CefRefPtr<CefCompletionCallback> callback_;

  IMPLEMENT_REFCOUNTING(CefCompletionCallbackWrapper);
  DISALLOW_COPY_AND_ASSIGN(CefCompletionCallbackWrapper);
};

}  // namespace

using content::TracingController;

CefTraceSubscriber::CefTraceSubscriber()
    : collecting_trace_data_(false),
      weak_factory_(this) {
  CEF_REQUIRE_UIT();
}

CefTraceSubscriber::~CefTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (collecting_trace_data_)
    TracingController::GetInstance()->StopTracing(NULL);
}

bool CefTraceSubscriber::BeginTracing(
    const std::string& categories,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();

  if (collecting_trace_data_)
    return false;

  collecting_trace_data_ = true;

  TracingController::StartTracingDoneCallback done_callback;
  if (callback.get()) {
    // Work around a bug introduced in http://crbug.com/542390#c22 that keeps a
    // reference to |done_callback| after execution.
    callback = new CefCompletionCallbackWrapper(callback);
    done_callback =
        base::Bind(&CefCompletionCallback::OnComplete, callback.get());
  }

  TracingController::GetInstance()->StartTracing(
      base::trace_event::TraceConfig(categories, ""),
      done_callback);
  return true;
}

bool CefTraceSubscriber::EndTracing(
    const base::FilePath& tracing_file,
    CefRefPtr<CefEndTracingCallback> callback) {
  CEF_REQUIRE_UIT();

  if (!collecting_trace_data_)
    return false;

  if (!callback.get()) {
    // Discard the trace data.
    collecting_trace_data_ = false;
    TracingController::GetInstance()->StopTracing(NULL);
    return true;
  }

  if (tracing_file.empty()) {
    // Create a new temporary file path on the FILE thread, then continue.
    CEF_POST_TASK(CEF_FILET,
        base::Bind(CreateTemporaryFileOnFileThread,
            base::ThreadTaskRunnerHandle::Get(),
            base::Bind(&CefTraceSubscriber::ContinueEndTracing,
                       weak_factory_.GetWeakPtr(), callback)));
    return true;
  }

  base::Closure result_callback =
      base::Bind(&CefTraceSubscriber::OnTracingFileResult,
                 weak_factory_.GetWeakPtr(), callback, tracing_file);

  TracingController::GetInstance()->StopTracing(
      TracingController::CreateFileSink(tracing_file, result_callback));
  return true;
}

void CefTraceSubscriber::ContinueEndTracing(
    CefRefPtr<CefEndTracingCallback> callback,
    const base::FilePath& tracing_file) {
  CEF_REQUIRE_UIT();
  if (!tracing_file.empty())
    EndTracing(tracing_file, callback);
}

void CefTraceSubscriber::OnTracingFileResult(
    CefRefPtr<CefEndTracingCallback> callback,
    const base::FilePath& tracing_file) {
  CEF_REQUIRE_UIT();

  collecting_trace_data_ = false;

  callback->OnEndTracingComplete(tracing_file.value());
}
