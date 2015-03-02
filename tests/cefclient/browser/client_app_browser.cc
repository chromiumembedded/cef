// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "cefclient/browser/client_app_browser.h"

#include "include/base/cef_logging.h"
#include "include/cef_cookie.h"

namespace client {

ClientAppBrowser::ClientAppBrowser() {
}

void ClientAppBrowser::OnContextInitialized() {
  CreateDelegates(delegates_);

  // Register cookieable schemes with the global cookie manager.
  CefRefPtr<CefCookieManager> manager =
      CefCookieManager::GetGlobalManager(NULL);
  DCHECK(manager.get());
  manager->SetSupportedSchemes(cookieable_schemes_, NULL);

  print_handler_ = CreatePrintHandler();

  DelegateSet::iterator it = delegates_.begin();
  for (; it != delegates_.end(); ++it)
    (*it)->OnContextInitialized(this);
}

void ClientAppBrowser::OnBeforeChildProcessLaunch(
      CefRefPtr<CefCommandLine> command_line) {
  DelegateSet::iterator it = delegates_.begin();
  for (; it != delegates_.end(); ++it)
    (*it)->OnBeforeChildProcessLaunch(this, command_line);
}

void ClientAppBrowser::OnRenderProcessThreadCreated(
    CefRefPtr<CefListValue> extra_info) {
  DelegateSet::iterator it = delegates_.begin();
  for (; it != delegates_.end(); ++it)
    (*it)->OnRenderProcessThreadCreated(this, extra_info);
}

}  // namespace client
