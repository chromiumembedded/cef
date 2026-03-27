// Copyright (c) 2026 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_BROWSER_CAPTURE_IMPL_H_
#define CEF_LIBCEF_BROWSER_BROWSER_CAPTURE_IMPL_H_
#pragma once

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_browser_capture.h"
#include "cef/include/cef_compound_operation.h"
#include "cef/libcef/browser/element_ref.h"

class CefBrowserHostBase;

// Implementation of the CefBrowserCapture interface. Methods are executed on
// the browser process UI thread.
class CefBrowserCaptureImpl : public CefBrowserCapture {
 public:
  explicit CefBrowserCaptureImpl(CefBrowserHostBase* browser);

  CefBrowserCaptureImpl(const CefBrowserCaptureImpl&) = delete;
  CefBrowserCaptureImpl& operator=(const CefBrowserCaptureImpl&) = delete;

  void Snapshot(const CefSnapshotSettings& settings,
                CefRefPtr<CefStringVisitor> callback) override;
  void EvalThenSnapshot(const CefString& code,
                         const CefSnapshotSettings& settings,
                         CefRefPtr<CefEvalSnapshotCallback> callback) override;
  void CaptureAnnotatedScreenshot(
      const CefString& path,
      const CefAnnotatedScreenshotSettings& settings,
      CefRefPtr<CefScreenshotCallback> callback) override;

  void ExecuteCompoundOperation(
      const CefCompoundOperation& operation,
      CefRefPtr<CefCompoundOperationCallback> callback);

  // Get the element ref index for this browser's capture state.
  CefElementRefIndex& GetRefIndex() { return ref_index_; }

 private:
  ~CefBrowserCaptureImpl() override = default;

  raw_ptr<CefBrowserHostBase> browser_;
  CefElementRefIndex ref_index_;

  IMPLEMENT_REFCOUNTING_DELETE_ON_UIT(CefBrowserCaptureImpl);
};

#endif  // CEF_LIBCEF_BROWSER_BROWSER_CAPTURE_IMPL_H_
