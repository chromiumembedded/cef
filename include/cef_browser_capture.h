// Copyright (c) 2026 Marshall A. Greenblatt. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ---------------------------------------------------------------------------
//
// The contents of this file must follow a specific format in order to
// support the CEF translator tool. See the translator.README.txt file in the
// tools directory for more information.
//

#ifndef CEF_INCLUDE_CEF_BROWSER_CAPTURE_H_
#define CEF_INCLUDE_CEF_BROWSER_CAPTURE_H_
#pragma once

#include "include/cef_base.h"
#include "include/cef_string_visitor.h"
#include "include/cef_values.h"

///
/// Callback interface for CefBrowserCapture::CaptureAnnotatedScreenshot.
/// Methods will be executed on the browser process UI thread.
///
/*--cef(source=client)--*/
class CefScreenshotCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called after capture completes. |path| is the output file path if
  /// available. |error| is empty on success.
  ///
  /*--cef(optional_param=path,optional_param=error)--*/
  virtual void OnScreenshotCaptured(const CefString& path,
                                    const CefString& error) = 0;
};

///
/// Callback interface for CefBrowserCapture::EvalThenSnapshot.
///
/*--cef(source=client)--*/
class CefEvalSnapshotCallback : public virtual CefBaseRefCounted {
 public:
  ///
  /// Called when both the JavaScript evaluation and snapshot are complete.
  /// |eval_success| indicates if the JS executed without error.
  /// |eval_result| contains the JS return value (may be null).
  /// |eval_error| contains the JS error message if eval_success is false.
  /// |snapshot| contains the page snapshot text.
  ///
  /*--cef(optional_param=eval_result,optional_param=eval_error,
          optional_param=snapshot)--*/
  virtual void OnComplete(bool eval_success,
                           CefRefPtr<CefValue> eval_result,
                           const CefString& eval_error,
                           const CefString& snapshot) = 0;
};

///
/// Browser-scoped capture and snapshot functionality. This interface provides
/// the browser-host equivalent of agent-browser snapshot and annotated
/// screenshot behaviors. The current implementation is scaffold-only and
/// returns placeholder results while the capture pipeline is being completed.
/// Methods may be called on any browser process thread unless otherwise
/// indicated.
///
/*--cef(source=library)--*/
class CefBrowserCapture : public virtual CefBaseRefCounted {
 public:
  ///
  /// Capture a textual snapshot of the current page using |settings| and
  /// return the result asynchronously via |callback|.
  ///
  /*--cef()--*/
  virtual void Snapshot(const CefSnapshotSettings& settings,
                        CefRefPtr<CefStringVisitor> callback) = 0;

  ///
  /// Execute JavaScript and capture a snapshot in a single pipelined call.
  /// The JavaScript is evaluated first, then the snapshot is captured
  /// immediately after (without an additional IPC round-trip for scheduling).
  /// This is the optimal pattern for coding agent workflows.
  ///
  /*--cef()--*/
  virtual void EvalThenSnapshot(const CefString& code,
                                 const CefSnapshotSettings& settings,
                                 CefRefPtr<CefEvalSnapshotCallback> callback) = 0;

  ///
  /// Capture a screenshot of the current page. If |path| is empty a default
  /// output location may be chosen by the implementation. The result will be
  /// returned asynchronously via |callback|.
  ///
  /*--cef(optional_param=path)--*/
  virtual void CaptureAnnotatedScreenshot(
      const CefString& path,
      const CefAnnotatedScreenshotSettings& settings,
      CefRefPtr<CefScreenshotCallback> callback) = 0;
};

#endif  // CEF_INCLUDE_CEF_BROWSER_CAPTURE_H_
