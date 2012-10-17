// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
#define CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
#pragma once

#include "include/cef_trace.h"

#include "base/debug/trace_event.h"
#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/trace_subscriber.h"

// May only be accessed on the browser process UI thread.
class CefTraceSubscriber : public content::TraceSubscriber {
 public:
  CefTraceSubscriber();
  virtual ~CefTraceSubscriber();

  bool BeginTracing(CefRefPtr<CefTraceClient> client,
                    const std::string& categories);
  bool GetTraceBufferPercentFullAsync();
  bool EndTracingAsync();

 private:
  // content::TraceSubscriber methods:
  virtual void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& trace_fragment) OVERRIDE;
  virtual void OnTraceBufferPercentFullReply(float percent_full) OVERRIDE;
  virtual void OnEndTracingComplete() OVERRIDE;

  bool collecting_trace_data_;
  CefRefPtr<CefTraceClient> client_;
};

#endif  // CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
