// Copyright (c) 2020 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#ifndef CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_MANAGER_H_
#define CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_MANAGER_H_
#pragma once

#include <memory>

#include "base/memory/raw_ptr.h"
#include "cef/include/cef_browser.h"

class CefBrowserHostBase;
class CefDevToolsController;

namespace content {
class WebContents;
}

// Manages DevTools protocol messages without an active frontend. Methods must
// be called on the UI thread unless otherwise indicated.
class CefDevToolsProtocolManager {
 public:
  // |inspected_browser| will outlive this object.
  explicit CefDevToolsProtocolManager(CefBrowserHostBase* inspected_browser);

  CefDevToolsProtocolManager(const CefDevToolsProtocolManager&) = delete;
  CefDevToolsProtocolManager& operator=(const CefDevToolsProtocolManager&) =
      delete;

  ~CefDevToolsProtocolManager();

  // See CefBrowserHost methods of the same name for documentation.
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
  bool EnsureController();

  const raw_ptr<CefBrowserHostBase> inspected_browser_;

  std::unique_ptr<CefDevToolsController> devtools_controller_;
};

#endif  // CEF_LIBCEF_BROWSER_DEVTOOLS_DEVTOOLS_PROTOCOL_MANAGER_H_
