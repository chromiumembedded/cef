// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "content/public/browser/browser_thread.h"
#include "libcef/cef_thread.h"

// Stub implementations to convert BrowserThread calls to CefThread.

namespace content {

namespace {

int GetCefId(BrowserThread::ID browser_id) {
  switch (browser_id) {
    case BrowserThread::UI:
      return CefThread::UI;
    case BrowserThread::IO:
      return CefThread::IO;
    case BrowserThread::FILE:
      return CefThread::FILE;
    default:
      break;
  }

  // The specified BrowserThread ID is not supported.
  NOTREACHED();
  return -1;
}

}  // namespace

// static
bool BrowserThread::PostTask(ID identifier,
                             const tracked_objects::Location& from_here,
                             const base::Closure& task) {
  int cef_id = GetCefId(identifier);
  if (cef_id < 0)
    return false;

  return CefThread::PostTask(static_cast<CefThread::ID>(cef_id), from_here,
                             task);
}

// static
bool BrowserThread::CurrentlyOn(ID identifier) {
  int cef_id = GetCefId(identifier);
  if (cef_id < 0)
    return false;

  return CefThread::CurrentlyOn(static_cast<CefThread::ID>(cef_id));
}

}  // namespace content
