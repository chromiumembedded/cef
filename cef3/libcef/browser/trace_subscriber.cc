// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "libcef/browser/trace_subscriber.h"
#include "include/cef_trace.h"
#include "libcef/browser/thread_util.h"

#include "content/public/browser/trace_controller.h"

CefTraceSubscriber::CefTraceSubscriber()
    : collecting_trace_data_(false) {
  CEF_REQUIRE_UIT();
}

CefTraceSubscriber::~CefTraceSubscriber() {
  CEF_REQUIRE_UIT();
  if (collecting_trace_data_)
    content::TraceController::GetInstance()->CancelSubscriber(this);
}

bool CefTraceSubscriber::BeginTracing(CefRefPtr<CefTraceClient> client,
                                      const std::string& categories) {
  CEF_REQUIRE_UIT();

  if (collecting_trace_data_)
    return false;

  collecting_trace_data_ = true;
  client_ = client;

  return content::TraceController::GetInstance()->BeginTracing(
      this, categories);
}

bool CefTraceSubscriber::EndTracingAsync() {
  CEF_REQUIRE_UIT();

  if (!collecting_trace_data_)
    return false;

  return content::TraceController::GetInstance()->EndTracingAsync(this);
}

bool CefTraceSubscriber::GetTraceBufferPercentFullAsync() {
  CEF_REQUIRE_UIT();

  if (!collecting_trace_data_ || !client_.get())
    return false;

  return content::TraceController::GetInstance()->
      GetTraceBufferPercentFullAsync(this);
}

void CefTraceSubscriber::OnTraceDataCollected(
    const scoped_refptr<base::RefCountedString>& trace_fragment) {
  CEF_REQUIRE_UIT();
  DCHECK(collecting_trace_data_);
  if (client_.get()) {
    client_->OnTraceDataCollected(trace_fragment->data().c_str(),
                                  trace_fragment->data().size());
  }
}

void CefTraceSubscriber::OnTraceBufferPercentFullReply(float percent_full) {
  CEF_REQUIRE_UIT();
  DCHECK(collecting_trace_data_);
  DCHECK(client_.get());
  client_->OnTraceBufferPercentFullReply(percent_full);
}

void CefTraceSubscriber::OnEndTracingComplete() {
  CEF_REQUIRE_UIT();
  DCHECK(collecting_trace_data_);
  collecting_trace_data_ = false;
  if (client_.get())
    client_->OnEndTracingComplete();
}
