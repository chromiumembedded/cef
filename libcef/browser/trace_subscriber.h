// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
#define CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
#pragma once

#include "include/cef_trace.h"

#include "base/files/file_path.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"

// May only be accessed on the browser process UI thread.
class CefTraceSubscriber {
 public:
  CefTraceSubscriber();
  virtual ~CefTraceSubscriber();

  bool BeginTracing(const std::string& categories,
                    CefRefPtr<CefCompletionCallback> callback);
  bool EndTracing(const base::FilePath& tracing_file,
                  CefRefPtr<CefEndTracingCallback> callback);

 private:
  void ContinueEndTracing(CefRefPtr<CefEndTracingCallback> callback,
                          const base::FilePath& tracing_file);
  void OnTracingFileResult(CefRefPtr<CefEndTracingCallback> callback,
                           const base::FilePath& tracing_file);

  bool collecting_trace_data_;
  base::WeakPtrFactory<CefTraceSubscriber> weak_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_TRACE_SUBSCRIBER_H_
