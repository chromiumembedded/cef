// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/trace_subscriber.h"
#include "include/cef_trace.h"
#include "libcef/browser/thread_util.h"

#include "base/debug/trace_event.h"
#include "content/public/browser/tracing_controller.h"

using content::TracingController;

CefTraceSubscriber::CefTraceSubscriber()
    : collecting_trace_data_(false),
      weak_factory_(this) {
  CEF_REQUIRE_UIT();
}

CefTraceSubscriber::~CefTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (collecting_trace_data_) {
    TracingController::GetInstance()->DisableRecording(
        base::FilePath(),
        TracingController::TracingFileResultCallback());
  }
}

bool CefTraceSubscriber::BeginTracing(
    const std::string& categories,
    CefRefPtr<CefCompletionCallback> callback) {
  CEF_REQUIRE_UIT();

  if (collecting_trace_data_)
    return false;

  collecting_trace_data_ = true;

  TracingController::EnableRecordingDoneCallback done_callback;
  if (callback.get())
    done_callback = base::Bind(&CefCompletionCallback::OnComplete, callback);

  TracingController::GetInstance()->EnableRecording(
      categories, TracingController::DEFAULT_OPTIONS, done_callback);
  return true;
}

bool CefTraceSubscriber::EndTracing(
    const base::FilePath& tracing_file,
    CefRefPtr<CefEndTracingCallback> callback) {
  CEF_REQUIRE_UIT();

  if (!collecting_trace_data_)
    return false;

  TracingController::TracingFileResultCallback result_callback;
  if (callback.get()) {
    result_callback =
        base::Bind(&CefTraceSubscriber::OnTracingFileResult,
                   weak_factory_.GetWeakPtr(), callback);
  }

  TracingController::GetInstance()->DisableRecording(
      tracing_file, result_callback);
  return true;
}

void CefTraceSubscriber::OnTracingFileResult(
    CefRefPtr<CefEndTracingCallback> callback,
    const base::FilePath& tracing_file) {
  CEF_REQUIRE_UIT();

  collecting_trace_data_ = false;

  callback->OnEndTracingComplete(tracing_file.value());
}
