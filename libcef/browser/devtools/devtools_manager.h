// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
#pragma once

#include "include/cef_browser.h"

#include "base/memory/weak_ptr.h"

class CefBrowserHostBase;
class CefDevToolsController;
class CefDevToolsFrontend;

namespace content {
class WebContents;
}

// Manages DevTools instances. Methods must be called on the UI thread unless
// otherwise indicated.
class CefDevToolsManager {
 public:
  // |inspected_browser| will outlive this object.
  explicit CefDevToolsManager(CefBrowserHostBase* inspected_browser);

  CefDevToolsManager(const CefDevToolsManager&) = delete;
  CefDevToolsManager& operator=(const CefDevToolsManager&) = delete;

  ~CefDevToolsManager();

  // See CefBrowserHost methods of the same name for documentation.
  void ShowDevTools(const CefWindowInfo& windowInfo,
                    CefRefPtr<CefClient> client,
                    const CefBrowserSettings& settings,
                    const CefPoint& inspect_element_at);
  void CloseDevTools();
  bool HasDevTools();
  bool SendDevToolsMessage(const void* message, size_t message_size);
  int ExecuteDevToolsMethod(int message_id,
                            const CefString& method,
                            CefRefPtr<CefDictionaryValue> param);

  // These methods are used to implement
  // CefBrowserHost::AddDevToolsMessageObserver. CreateRegistration is safe to
  // call on any thread. InitializeRegistrationOnUIThread should be called
  // immediately afterwards on the UI thread.
  static CefRefPtr<CefRegistration> CreateRegistration(
      CefRefPtr<CefDevToolsMessageObserver> observer);
  void InitializeRegistrationOnUIThread(
      CefRefPtr<CefRegistration> registration);

 private:
  void OnFrontEndDestroyed();

  bool EnsureController();

  CefBrowserHostBase* const inspected_browser_;

  // CefDevToolsFrontend will delete itself when the frontend WebContents is
  // destroyed.
  CefDevToolsFrontend* devtools_frontend_ = nullptr;

  // Used for sending DevTools protocol messages without an active frontend.
  std::unique_ptr<CefDevToolsController> devtools_controller_;

  base::WeakPtrFactory<CefDevToolsManager> weak_ptr_factory_;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_MANAGER_H_
