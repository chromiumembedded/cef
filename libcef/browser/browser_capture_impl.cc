// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cef/libcef/browser/browser_capture_impl.h"

#include "cef/libcef/browser/browser_host_base.h"
#include "cef/libcef/browser/thread_util.h"

namespace {

void RunSnapshotCallback(CefRefPtr<CefStringVisitor> callback,
                         const CefString& value) {
  if (!callback) {
    return;
  }
  CEF_POST_TASK(CEF_UIT,
                base::BindOnce(&CefStringVisitor::Visit, callback, value));
}

void RunScreenshotCallback(CefRefPtr<CefScreenshotCallback> callback,
                           const CefString& path,
                           const CefString& error) {
  if (!callback) {
    return;
  }
  CEF_POST_TASK(
      CEF_UIT,
      base::BindOnce(&CefScreenshotCallback::OnScreenshotCaptured, callback,
                     path, error));
}

}  // namespace

CefBrowserCaptureImpl::CefBrowserCaptureImpl(CefBrowserHostBase* browser)
    : browser_(browser) {}

void CefBrowserCaptureImpl::Snapshot(const CefSnapshotSettings& settings,
                                     CefRefPtr<CefStringVisitor> callback) {
  CefString message;
  message.FromString(
      "Browser capture snapshot scaffolding is not implemented.");
  RunSnapshotCallback(callback, message);
}

void CefBrowserCaptureImpl::CaptureAnnotatedScreenshot(
    const CefString& path,
    const CefAnnotatedScreenshotSettings& settings,
    CefRefPtr<CefScreenshotCallback> callback) {
  RunScreenshotCallback(
      callback, path,
      "Browser capture screenshot scaffolding is not implemented.");
}
