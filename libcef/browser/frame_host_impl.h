// Copyright (c) 2012 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
#define CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
#pragma once

#include <string>
#include "include/cef_frame.h"
#include "libcef/common/response_manager.h"
#include "base/synchronization/lock.h"

class CefBrowserHostImpl;

// Implementation of CefFrame. CefFrameHostImpl objects are owned by the
// CefBrowerHostImpl and will be detached when the browser is notified that the
// associated renderer WebFrame will close.
class CefFrameHostImpl : public CefFrame {
 public:
  CefFrameHostImpl(CefBrowserHostImpl* browser,
               int64 frame_id,
               bool is_main_frame,
               const CefString& url,
               const CefString& name,
               int64 parent_frame_id);
  ~CefFrameHostImpl() override;

  // CefFrame methods
  bool IsValid() override;
  void Undo() override;
  void Redo() override;
  void Cut() override;
  void Copy() override;
  void Paste() override;
  void Delete() override;
  void SelectAll() override;
  void ViewSource() override;
  void GetSource(CefRefPtr<CefStringVisitor> visitor) override;
  void GetText(CefRefPtr<CefStringVisitor> visitor) override;
  void LoadRequest(CefRefPtr<CefRequest> request) override;
  void LoadURL(const CefString& url) override;
  void LoadString(const CefString& string,
                  const CefString& url) override;
  void ExecuteJavaScript(const CefString& jsCode,
                         const CefString& scriptUrl,
                         int startLine) override;
  bool IsMain() override;
  bool IsFocused() override;
  CefString GetName() override;
  int64 GetIdentifier() override;
  CefRefPtr<CefFrame> GetParent() override;
  CefString GetURL() override;
  CefRefPtr<CefBrowser> GetBrowser() override;
  CefRefPtr<CefV8Context> GetV8Context() override;
  void VisitDOM(CefRefPtr<CefDOMVisitor> visitor) override;

  void SetFocused(bool focused);
  void SetAttributes(const CefString& url,
                     const CefString& name,
                     int64 parent_frame_id);

  // Avoids unnecessary string type conversions.
  void SendJavaScript(const std::string& jsCode,
                      const std::string& scriptUrl,
                      int startLine);

  // Detach the frame from the browser.
  void Detach();

  // kMainFrameId must be -1 to align with renderer expectations.
  static const int64 kMainFrameId = -1;
  static const int64 kFocusedFrameId = -2;
  static const int64 kUnspecifiedFrameId = -3;
  static const int64 kInvalidFrameId = -4;

 protected:
  void SendCommand(const std::string& command,
                   CefRefPtr<CefResponseManager::Handler> responseHandler);

  int64 frame_id_;
  bool is_main_frame_;

  // Volatile state information. All access must be protected by the state lock.
  base::Lock state_lock_;
  CefBrowserHostImpl* browser_;
  bool is_focused_;
  CefString url_;
  CefString name_;
  int64 parent_frame_id_;

  IMPLEMENT_REFCOUNTING(CefFrameHostImpl);
  DISALLOW_COPY_AND_ASSIGN(CefFrameHostImpl);
};

#endif  // CEF_LIBCEF_BROWSER_FRAME_HOST_IMPL_H_
