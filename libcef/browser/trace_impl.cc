// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "base/time/time.h"
#include "cef/include/cef_trace.h"
#include "cef/libcef/browser/context.h"
#include "cef/libcef/browser/thread_util.h"
#include "cef/libcef/browser/trace_subscriber.h"

bool CefBeginTracing(const CefString& categories,
                     CefRefPtr<CefCompletionCallback> callback) {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  CefTraceSubscriber* subscriber = CefContext::Get()->GetTraceSubscriber();
  if (!subscriber) {
    return false;
  }

  return subscriber->BeginTracing(categories, callback);
}

bool CefEndTracing(const CefString& tracing_file,
                   CefRefPtr<CefEndTracingCallback> callback) {
  if (!CONTEXT_STATE_VALID()) {
    DCHECK(false) << "context not valid";
    return false;
  }

  if (!CEF_CURRENTLY_ON_UIT()) {
    DCHECK(false) << "called on invalid thread";
    return false;
  }

  CefTraceSubscriber* subscriber = CefContext::Get()->GetTraceSubscriber();
  if (!subscriber) {
    return false;
  }

  return subscriber->EndTracing(base::FilePath(tracing_file), callback);
}

int64_t CefNowFromSystemTraceTime() {
  return base::TimeTicks::Now().ToInternalValue();
}
